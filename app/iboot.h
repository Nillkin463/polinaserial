#ifndef APP_IBOOT_H
#define APP_IBOOT_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint64_t hmac;
    const char *file;
} iboot_hmac_config_t;

typedef struct {
    uint16_t off;
    int line;
    const char *file;
} iboot_file_pos_t;

int  iboot_push_data(const char *data, size_t data_len, iboot_file_pos_t pos[], size_t *pos_cnt);
void iboot_print_file(int fd, iboot_file_pos_t *pos);

#endif
