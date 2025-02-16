#ifndef SERIAL_DEVICE_H
#define SERIAL_DEVICE_H

#include <stdbool.h>
#include <termios.h>
#include "config.h"

int device_open_with_callout(const char *callout);

int device_set_speed(int fd, speed_t speed);

void tty_set_attrs_from_config(struct termios *attrs, serial_config_t *config);

#endif
