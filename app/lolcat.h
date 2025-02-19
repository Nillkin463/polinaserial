#ifndef APP_LOLCAT_H
#define APP_LOLCAT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void lolcat_init();

int lolcat_push_data(
    const char *data,
    size_t data_len,
    char *out,
    size_t *out_len,
    uint16_t *offs,
    size_t *offs_cnt
);

#endif
