#ifndef IOKIT_MENU_H
#define IOKIT_MENU_H

#include <IOKit/IOKitLib.h>
#include <stdbool.h>
#include <app.h>
#include "menu.h"

typedef void (*iokit_event_cb_t)(io_service_t service, uint64_t id, bool added);

int  iokit_register_serial_devices_events(iokit_event_cb_t cb);
void iokit_unregister_serial_devices_events();

int iokit_serial_dev_from_service(io_service_t service, serial_dev_t *device);

typedef struct {
    ll_t ll;
    serial_dev_t dev;
} serial_dev_list_t;

serial_dev_list_t *iokit_serial_find_devices();

#endif
