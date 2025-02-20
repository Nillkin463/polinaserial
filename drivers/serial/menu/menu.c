#include <string.h>
#include <app/misc.h>
#include "menu.h"

static serial_dev_t *_devices = NULL;

serial_dev_t *menu_pick() {
    if (!_devices) {
        ERROR("no devices found");
        goto fail;
    }

    unsigned int index = 0;
    static const char start = 'a';

    INFO("\nSelect a device from the list below:");

    ll_iterate(_devices, serial_dev_t *, curr, {
        printf("\t(%c) ", index + start);

        if (curr->usb_name) {
            INFO_NO_BREAK("%s", curr->usb_name);

            if (curr->tty_suffix) {
                INFO_NO_BREAK("-%s", curr->tty_suffix);
            }

            printf(" on ");
        }

        printf("%s", curr->callout);

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

    ll_iterate(_devices, serial_dev_t *, curr, {
        if (iterate_index == result_index) {
            return curr;
        }

        iterate_index++;
    });

fail:
    return NULL;
}

serial_dev_t *serial_dev_find_by_callout(const char *callout) {
    ll_iterate(_devices, serial_dev_t *, curr, {
        if (strcmp(callout, curr->callout) == 0) {
            return curr;
        }
    });

    return NULL;
}

serial_dev_t *iokit_serial_find_devices();

int serial_find_devices() {
    if ((_devices = iokit_serial_find_devices())) {
        return 0;
    }

    return -1;
}

void serial_dev_destroy(void *arg) {
    serial_dev_t *dev = arg;

    free(dev->tty_name);
    free(dev->callout);

    if (dev->tty_suffix) {
        free(dev->tty_suffix);
    }

    if (dev->usb_name) {
        free(dev->usb_name);
    }
}

void serial_dev_list_destroy() {
    if (_devices) {
        ll_destroy((ll_t **)&_devices, serial_dev_destroy);
        _devices = NULL;
    }
}
