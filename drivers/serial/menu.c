#include <string.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/USBSpec.h>

#include <app/misc.h>
#include "menu.h"

static menu_serial_item_t *_devices = NULL;

static io_registry_entry_t iokit_get_parent_with_class(io_service_t service, const char *class) {
    io_name_t name;
    io_registry_entry_t prev_parent = IO_OBJECT_NULL;
    io_registry_entry_t parent = service;
    
    do {
        if (parent != service) {
            prev_parent = parent;
        }

        if (IORegistryEntryGetParentEntry(parent, kIOServicePlane, &parent) != KERN_SUCCESS) {
            return IO_OBJECT_NULL;
        }

        if (IOObjectGetClass(parent, name) != KERN_SUCCESS) {
            WARNING("failed to get IOObject name");
            IOObjectRelease(parent);
            return IO_OBJECT_NULL;
        }

        if (prev_parent) {
            IOObjectRelease(prev_parent);
        }

    } while (strcmp(name, class) != 0);

    return parent;
}

static menu_serial_item_t *menu_device_from_service(io_service_t service) {
    menu_serial_item_t *device = NULL;
    CFMutableDictionaryRef properties = NULL;
    CFStringRef tty_name = NULL;
    CFStringRef tty_suffix = NULL;
    CFStringRef callout_device = NULL;
    io_registry_entry_t usb_parent = IO_OBJECT_NULL;
    CFMutableDictionaryRef usb_parent_properties = NULL;
    CFStringRef usb_name = NULL;

    if (IORegistryEntryCreateCFProperties(service, &properties, kCFAllocatorDefault, kNilOptions) != KERN_SUCCESS) {
        ERROR("failed to get IOService properties");
        goto out;
    }

    tty_name = CFDictionaryGetValue(properties, CFSTR("IOTTYDevice"));
    if (!tty_name) {
        ERROR("failed to get IOTTYDevice");
        goto out;
    }

    tty_suffix = CFDictionaryGetValue(properties, CFSTR("IOTTYSuffix"));
    if (!tty_suffix) {
        ERROR("failed to get IOTTYSuffix");
        goto out;
    }

    callout_device = CFDictionaryGetValue(properties, CFSTR("IOCalloutDevice"));
    if (!callout_device) {
        ERROR("failed to get IOCalloutDevice");
        goto out;
    }

    usb_parent = iokit_get_parent_with_class(service, "IOUSBHostDevice");
    if (!usb_parent) {
        usb_parent = iokit_get_parent_with_class(service, "IOUSBDevice");
    }

    if (usb_parent) {
        if (IORegistryEntryCreateCFProperties(usb_parent, &usb_parent_properties, kCFAllocatorDefault, kNilOptions) == KERN_SUCCESS) {
            usb_name = CFDictionaryGetValue(usb_parent_properties, CFSTR(kUSBProductString));
        } else {
            WARNING("failed to get USB IOService properties");
        }
    }

    device = calloc(sizeof(menu_serial_item_t), 1);
    if (!device) {
        ERROR("failed to allocate memory");
        goto out;
    }

    device->tty_name = tty_name;
    CFRetain(tty_name);

    if (CFStringGetLength(tty_suffix) > 0) {
        device->tty_suffix = tty_suffix;
        CFRetain(tty_suffix);
    } else {
        device->tty_suffix = NULL;
    }

    device->callout_device = callout_device;
    CFRetain(callout_device);

    if (usb_name) {
        device->usb_name = usb_name;
        CFRetain(usb_name);
    }

out:
    if (properties) {
        CFRelease(properties);
    }

    if (usb_parent) {
        IOObjectRelease(usb_parent);
    }

    if (usb_parent_properties) {
        CFRelease(usb_parent_properties);
    }

    return device;
}

int menu_find_devices() {
    CFMutableDictionaryRef matching_dict = IOServiceMatching("IOSerialBSDClient");
    if (!matching_dict) {
        ERROR("failed to get matching dict from IOKit");
        return -1;
    }

    io_iterator_t iterator;

    if (IOServiceGetMatchingServices(kIOMasterPortDefault, matching_dict, &iterator) != KERN_SUCCESS) {
        ERROR("failed to get matching services");
        CFRelease(matching_dict);
        return -1;
    }

    io_service_t service;

    while ((service = IOIteratorNext(iterator))) {
        menu_serial_item_t *item = menu_device_from_service(service);
        IOObjectRelease(service);

        if (!item) {
            IOObjectRelease(iterator);
            ERROR("failed to get menu device");
            return -1;
        }

        ll_add_element(&_devices, item);
    }

    return 0;
}

menu_serial_item_t *menu_pick() {
    if (!_devices) {
        ERROR("no devices found");
        goto fail;
    }

    unsigned int index = 0;
    static const char start = 'a';

    INFO("\nSelect a device from the list below:");

    ll_iterate(_devices, menu_serial_item_t *, current, {
        printf("\t(%c) ", index + start);

        if (current->usb_name) {
            MENU_STR_FROM_CFSTR(__usb_name, current->usb_name);
            INFO_NO_BREAK("%s", __usb_name);

            if (current->tty_suffix) {
                MENU_STR_FROM_CFSTR(__tty_suffix, current->tty_suffix);
                INFO_NO_BREAK("-%s", __tty_suffix);
            }

            printf(" on ");
        }

        MENU_STR_FROM_CFSTR(__callout, current->callout_device);
        printf("%s", __callout);

        printf("\n");

        index++;
    });

    unsigned int result_index = -1;

    do {
        INFO_NO_BREAK("Option: ");

        char result_char = getchar();
        if (result_char == '\n') {
            continue;
        }

        getchar();
        result_index = result_char - start;
    } while (result_index >= index);

    unsigned int iterate_index = 0;

    ll_iterate(_devices, menu_serial_item_t *, current, {
        if (iterate_index == result_index) {
            return current;
        }

        iterate_index++;
    });

fail:
    return NULL;
}

menu_serial_item_t *menu_find_by_callout(const char *callout) {
    ll_iterate(_devices, menu_serial_item_t *, current, {
        MENU_STR_FROM_CFSTR(__callout, current->callout_device);
        if (strcmp(callout, __callout) == 0) {
            return current;
        }
    });

    return NULL;
}

static void _menu_destroy_element(menu_serial_item_t *element) {
    CFRelease(element->tty_name);
    CFRelease(element->callout_device);

    if (element->tty_suffix) {
        CFRelease(element->tty_suffix);
    }

    if (element->usb_name) {
        CFRelease(element->usb_name);
    }
}

void menu_destroy() {
    if (_devices) {
        ll_destroy(_devices, _menu_destroy_element);
        _devices = NULL;
    }
}
