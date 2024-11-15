#ifndef MENU_H
#define MENU_H

#include <ll.h>
#include <CoreFoundation/CoreFoundation.h>

typedef struct {
    ll_t next;

    CFStringRef tty_name;
    CFStringRef tty_suffix;

    CFStringRef callout_device;

    CFStringRef usb_name;
} menu_serial_item_t;

/* misbehaves on Sonoma with USB name and SN */
#define STR_FROM_CFSTR(cftsr)   CFStringGetCStringPtr(cftsr, kCFStringEncodingUTF8)

int menu_find_devices();

menu_serial_item_t *menu_pick();
menu_serial_item_t *menu_find_by_callout(const char *callout);

const char *menu_get_callout_from_item(menu_serial_item_t *item);

void menu_destroy();

#endif
