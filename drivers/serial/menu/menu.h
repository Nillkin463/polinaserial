#ifndef SERIAL_MENU_H
#define SERIAL_MENU_H

#include <limits.h>
#include <app/ll.h>

typedef struct {
    ll_t next;
    char *tty_name;
    char *tty_suffix;
    char *callout;
    char *usb_name;
} serial_dev_t;

int serial_find_devices();

void serial_dev_destroy(void *dev);
void serial_dev_list_destroy();

serial_dev_t *serial_dev_find_by_callout(const char *callout);

serial_dev_t *menu_pick();

#endif
