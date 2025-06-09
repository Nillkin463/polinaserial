#ifndef APP_UNICODE_H
#define APP_UNICODE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    kSeqNone = 0,
    kSeqNormal,
    kSeqControl,
    kSeqEscapeCSI,
    kSeqUnicode,
    kSeqUnknown
} seq_type_t;

typedef enum {
    kEscSeqNone = 0,
    kEscSeqGoing
} esc_seq_state_t;

typedef struct {
    seq_type_t type;
    bool has_utf8_first_byte;
} seq_ctx_t;

int seq_process_chars(seq_ctx_t *ctx, const uint8_t *buf, size_t len);

#endif
