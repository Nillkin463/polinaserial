#include "lolcat.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#define ESCAPE_SEQUENCE "\x1B[38;5;%dm%c\x1B[39m"
#define SKIP_COUNT      (3)

static const uint8_t lolcat_lut[] = {
    214, 208, 208, 203, 203, 198, 198, 199, 199, 164, 164, 128, 129, 93, 93, 63, 63, 63, 33, 33, 39, 38, 44, 44, 49, 49, 48, 48, 83, 83, 118, 118, 154, 154, 184, 184, 178, 214
};

static const size_t lolcat_lut_size = sizeof(lolcat_lut);

static int lut_position = 0;
static int lut_position_skip = 0;
static int lut_line_position = 0;

static int lut_position_increment_simple(int current) {
    return (current == lolcat_lut_size - 1) ? 0 : ++current;
}

static int lut_position_increment(int current) {
    lut_position_skip++;

    if (lut_position_skip != SKIP_COUNT) {
        return current;
    }

    lut_position_skip = 0;

    return lut_position_increment_simple(current);
}

static bool lolcat_printable(char c) {
    return c > 0x20 && c < 0x7F;
}

void lolcat_print_char(int fd, char c) {
    if (lolcat_printable(c)) {
        char sequence[20];

        int ret = snprintf(sequence, sizeof(sequence), ESCAPE_SEQUENCE, lolcat_lut[lut_position], c);

        write(fd, sequence, ret);

        lut_position = lut_position_increment(lut_position);

    } else {
        switch (c) {
            case ' ':
                write(fd, " ", 1);
                lut_position = lut_position_increment(lut_position);
                break;
            
            case '\n':
                lut_line_position = lut_position_increment_simple(lut_line_position);
                lut_position = lut_line_position;
                // break is missed intentionally

            default:
                write(fd, &c, 1);
                break;
        }
    }
}

void lolcat_init() {
    lut_position = arc4random_uniform(lolcat_lut_size - 1);
    lut_line_position = lut_position;
}
