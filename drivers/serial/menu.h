#ifndef SERIAL_MENU_H
#define SERIAL_MENU_H

#include <limits.h>

typedef struct {
    char tty_name[128];
    char tty_suffix[64];
    char callout[256];
    char usb_name[128];
} serial_dev_t;

int  serial_find_devices();
void serial_dev_list_destroy();

serial_dev_t *serial_dev_find_by_callout(const char *callout);

serial_dev_t *menu_pick();

#endif
