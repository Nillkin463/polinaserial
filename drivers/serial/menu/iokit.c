#include <string.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/USBSpec.h>

#include <app/misc.h>
#include "menu.h"

#define STR_FROM_CFSTR_ALLOC(_target, _cftsr) \
    CFIndex _len_##_cftsr = CFStringGetMaximumSizeForEncoding(CFStringGetLength(_cftsr), kCFStringEncodingUTF8); \
    REQUIRE_PANIC(_len_##_cftsr != kCFNotFound); \
    REQUIRE_PANIC((_target = malloc(_len_##_cftsr))); \
    REQUIRE_PANIC(CFStringGetCString(_cftsr, _target, _len_##_cftsr, kCFStringEncodingUTF8));

#define __cf_release(_ref) \
    if (_ref) { \
        CFRelease(_ref); \
        _ref = NULL; \
    }

#define __iokit_release(_obj) \
    if (_obj) { \
        IOObjectRelease(_obj); \
        _obj = IO_OBJECT_NULL; \
    }

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
            WARNING("couldn't get IOObject name");
            __iokit_release(parent);
            return IO_OBJECT_NULL;
        }

        if (prev_parent) {
            __iokit_release(prev_parent);
        }

    } while (strcmp(name, class) != 0);

    return parent;
}

static serial_dev_t *iokit_serial_dev_from_service(io_service_t service) {
    serial_dev_t *device = NULL;
    CFMutableDictionaryRef properties = NULL;
    CFStringRef tty_name = NULL;
    CFStringRef tty_suffix = NULL;
    CFStringRef callout_device = NULL;
    CFStringRef usb_name = NULL;
    io_registry_entry_t usb_parent = IO_OBJECT_NULL;
    CFMutableDictionaryRef usb_parent_properties = NULL;

    if (IORegistryEntryCreateCFProperties(service, &properties, kCFAllocatorDefault, kNilOptions) != KERN_SUCCESS) {
        ERROR("couldn't get IOService properties");
        goto out;
    }

    tty_name = CFDictionaryGetValue(properties, CFSTR("IOTTYDevice"));
    if (!tty_name) {
        ERROR("couldn't get IOTTYDevice");
        goto out;
    }

    tty_suffix = CFDictionaryGetValue(properties, CFSTR("IOTTYSuffix"));
    if (!tty_suffix) {
        ERROR("couldn't get IOTTYSuffix");
        goto out;
    }

    callout_device = CFDictionaryGetValue(properties, CFSTR("IOCalloutDevice"));
    if (!callout_device) {
        ERROR("couldn't get IOCalloutDevice");
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
            WARNING("couldn't get USB IOService properties");
        }
    }

    device = calloc(sizeof(serial_dev_t), 1);
    if (!device) {
        ERROR("couldn't allocate memory");
        goto out;
    }

    STR_FROM_CFSTR_ALLOC(device->tty_name, tty_name);
    __cf_release(tty_name);

    if (CFStringGetLength(tty_suffix) > 0) {
        STR_FROM_CFSTR_ALLOC(device->tty_suffix, tty_suffix);
    } else {
        device->tty_suffix = NULL;
    }

    __cf_release(tty_suffix);

    STR_FROM_CFSTR_ALLOC(device->callout, callout_device);
    __cf_release(callout_device);

    if (usb_name) {
        STR_FROM_CFSTR_ALLOC(device->usb_name, usb_name);
        __cf_release(usb_name);
    }

out:
    // __cf_release(properties);
    __cf_release(usb_parent_properties);
    __iokit_release(usb_parent);

    return device;
}

serial_dev_t *iokit_serial_find_devices() {
    serial_dev_t *devices = NULL;
    CFMutableDictionaryRef matching_dict = NULL;
    io_iterator_t iterator = IO_OBJECT_NULL;
    io_service_t service = IO_OBJECT_NULL;

    matching_dict = IOServiceMatching("IOSerialBSDClient");
    if (!matching_dict) {
        ERROR("couldn't get matching dictionary from IOKit");
        goto fail;
    }

    if (IOServiceGetMatchingServices(kIOMasterPortDefault, matching_dict, &iterator) != KERN_SUCCESS) {
        ERROR("couldn't get matching services");
        goto fail;
    }

    while ((service = IOIteratorNext(iterator))) {
        serial_dev_t *item = iokit_serial_dev_from_service(service);
        __iokit_release(service);

        if (!item) {
            goto fail;
        }

        ll_add((ll_t **)&devices, item);
    }

    goto out;

fail:
    ll_destroy((ll_t **)&devices, serial_dev_destroy);

out:
    // __cf_release(matching_dict);
    __iokit_release(iterator);
    __iokit_release(service);

    return devices;
}
