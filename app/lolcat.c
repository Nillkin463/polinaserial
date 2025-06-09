#include "lolcat.h"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <app/misc.h>

#define COLOR_256  "\x1B[38;5;"
#define STOP       "m"
#define RESET      "\x1B[0m"

#define SKIP_COUNT  (3)

struct lolcat_color {
    char clr[4];
    int len;
};

#define LOLCAT_CLR(_clr) \
    {#_clr, CONST_STRLEN(#_clr)}

static const struct lolcat_color lolcat_lut[] = {
    LOLCAT_CLR(214), LOLCAT_CLR(208), LOLCAT_CLR(208), LOLCAT_CLR(203),
    LOLCAT_CLR(203), LOLCAT_CLR(198), LOLCAT_CLR(198), LOLCAT_CLR(199),
    LOLCAT_CLR(199), LOLCAT_CLR(164), LOLCAT_CLR(164), LOLCAT_CLR(128),
    LOLCAT_CLR(129), LOLCAT_CLR(93),  LOLCAT_CLR(93),  LOLCAT_CLR(63),
    LOLCAT_CLR(63),  LOLCAT_CLR(63),  LOLCAT_CLR(33),  LOLCAT_CLR(33),
    LOLCAT_CLR(39),  LOLCAT_CLR(38),  LOLCAT_CLR(44),  LOLCAT_CLR(44),
    LOLCAT_CLR(49),  LOLCAT_CLR(49),  LOLCAT_CLR(48),  LOLCAT_CLR(48),
    LOLCAT_CLR(83),  LOLCAT_CLR(83),  LOLCAT_CLR(118), LOLCAT_CLR(118),
    LOLCAT_CLR(154), LOLCAT_CLR(154), LOLCAT_CLR(184), LOLCAT_CLR(184),
    LOLCAT_CLR(178), LOLCAT_CLR(214)
};

#define LOLCAT_LUT_CNT  (sizeof(lolcat_lut) / sizeof(*lolcat_lut))

static int lut_pos = 0;
static int lut_line_pos = 0;
static int lut_pos_skip = 0;

static int lut_pos_increment_simple(int curr) {
    return (curr == LOLCAT_LUT_CNT - 1) ? 0 : ++curr;
}

static int lut_pos_increment(int curr) {
    lut_pos_skip++;

    if (lut_pos_skip != SKIP_COUNT) {
        return curr;
    }

    lut_pos_skip = 0;

    return lut_pos_increment_simple(curr);
}

static bool lolcat_printable(char c) {
    return c > 0x20 && c < 0x7F;
}

#define PUSH(__data, __len) \
    do { \
        if (_out_len + __len > max_len) { \
            POLINA_ERROR("\nmax_len: %zu, _out_len + __len: %zu", max_len, _out_len + __len); \
            return -1; \
        } \
        memcpy(out + _out_len, __data, __len); \
        _out_len += __len; \
    } while (0)

int lolcat_push_ascii(const uint8_t *data, size_t data_len, uint8_t *out, size_t *out_len) {
    size_t max_len = *out_len;
    size_t _out_len = 0;

    for (size_t i = 0; i < data_len; i++) {
        uint8_t c = data[i];

        if (lolcat_printable(c)) {
            const struct lolcat_color *clr = &lolcat_lut[lut_pos];

            PUSH(COLOR_256, CONST_STRLEN(COLOR_256));
            PUSH(clr->clr, clr->len);
            PUSH(STOP, CONST_STRLEN(STOP));

            PUSH(&c, 1);

            lut_pos = lut_pos_increment(lut_pos);

        } else {
            switch (c) {
                case ' ':
                    PUSH(" ", 1);
                    lut_pos = lut_pos_increment(lut_pos);
                    break;

                case '\n':
                    lut_line_pos = lut_pos_increment_simple(lut_line_pos);
                    lut_pos = lut_line_pos;
                    /* break is missed intentionally */
    
                default:
                    PUSH(&c, 1);
                    break;
            }
        }
    }

    *out_len = _out_len;

    return 0;
}

int lolcat_push_unicode(const uint8_t *data, size_t char_len, uint8_t *out, size_t *out_len) {
    size_t max_len = *out_len;
    size_t _out_len = 0;

    const struct lolcat_color *clr = &lolcat_lut[lut_pos];

    PUSH(COLOR_256, CONST_STRLEN(COLOR_256));
    PUSH(clr->clr, clr->len);
    PUSH(STOP, CONST_STRLEN(STOP));

    PUSH(data, char_len);

    lut_pos = lut_pos_increment(lut_pos);

    *out_len = _out_len;

    return 0;
}

void lolcat_reset() {
    write(STDOUT_FILENO, RESET, CONST_STRLEN(RESET));
    fflush(stdout);
}

void lolcat_init() {
    lut_pos = arc4random_uniform(LOLCAT_LUT_CNT - 1);
    lut_line_pos = lut_pos;
}
