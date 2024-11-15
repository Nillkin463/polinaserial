#ifndef IBOOT_H
#define IBOOT_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint64_t hmac;
    const char *file;
} iboot_hmac_config_t;

void iboot_push_char(int fd, char c);

#endif
