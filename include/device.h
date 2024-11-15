#ifndef DEVICE_H
#define DEVICE_H

#include <termios.h>
#include <stdbool.h>

#include <configuration.h>

int device_open_with_callout(const char *callout);

int device_get_attributes(int fd, struct termios *attributes);
int device_set_attributes(int fd, struct termios *attributes);
int device_set_speed(int fd, speed_t speed);

void attributes_set_from_configuration(
    struct termios *attributes,
    configuration_t *configuration,
    bool is_terminal
);

#endif
