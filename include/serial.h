#ifndef SERIAL_H
#define SERIAL_H

#include <stddef.h>

#define BUFFER_SIZE 1024

typedef int (*serial_callback_t)(int target_fd, char *buffer, size_t length);

int serial_loop(int device_fd, serial_callback_t in_callback, serial_callback_t out_callback);

#endif
