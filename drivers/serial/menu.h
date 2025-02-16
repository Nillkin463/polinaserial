#ifndef SERIAL_MENU_H
#define SERIAL_MENU_H

#include <limits.h>
#include <app/ll.h>
#include <CoreFoundation/CoreFoundation.h>

typedef struct {
    ll_t next;

    CFStringRef tty_name;
    CFStringRef tty_suffix;
    CFStringRef callout_device;
    CFStringRef usb_name;
} menu_serial_item_t;

int menu_find_devices();

menu_serial_item_t *menu_pick();
menu_serial_item_t *menu_find_by_callout(const char *callout);

void menu_destroy();

#define MENU_STR_FROM_CFSTR_PREALLOC(_target, _cftsr, _len) \
    REQUIRE_PANIC(CFStringGetCString(_cftsr, _target, _len, kCFStringEncodingUTF8));

#define MENU_STR_FROM_CFSTR(_target, _cftsr) \
    char _target[PATH_MAX]; \
    MENU_STR_FROM_CFSTR_PREALLOC(_target, _cftsr, PATH_MAX);


#endif
