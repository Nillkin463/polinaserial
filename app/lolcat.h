#ifndef APP_LOLCAT_H
#define APP_LOLCAT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void lolcat_init();
void lolcat_reset();

typedef int (*lolcat_handler_t)(
    const uint8_t *data,
    size_t data_len,
    uint8_t *out,
    size_t *out_len
);

int lolcat_push_ascii(
    const uint8_t *data,
    size_t data_len,
    uint8_t *out,
    size_t *out_len
);

int lolcat_push_one(
    const uint8_t *data,
    size_t len,
    uint8_t *out,
    size_t *out_len
);

#endif
