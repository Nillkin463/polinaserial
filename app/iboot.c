#include "iboot.h"
#include "iboot_config.h"

#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <app/misc.h>

typedef enum {
    STATE_WAITING_FOR_HMAC = 0,
    STATE_WAITING_FOR_LINE,
} iboot_hmac_state_t;

static uint64_t current_hmac = 0;
static uint8_t hmac_digit_count = 0;

static uint32_t current_line = 0;
static uint8_t line_digit_count = 0;

static iboot_hmac_state_t state = STATE_WAITING_FOR_HMAC;

static void iboot_clear_state() {
    current_hmac = 0;
    hmac_digit_count = 0;

    current_line = 0;
    line_digit_count = 0;

    state = STATE_WAITING_FOR_HMAC;
}

static int8_t iboot_hmac_character(char c) {
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 0xA;
    } else if (c >= '0' && c <= '9') {
        return c - '0';
    } else {
        return -1;
    }
}

static int8_t iboot_line_character(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else {
        return -1;
    }
}

static const char *iboot_find_file_for_hmac(uint64_t hmac) {
    if (hmac < iboot_hmac_config[0].hmac || hmac > iboot_hmac_config[iboot_hmac_config_count - 1].hmac) {
        return NULL;
    }

    const iboot_hmac_config_t *current_config = (const iboot_hmac_config_t *)&iboot_hmac_config;

    size_t l = 0;
    size_t r = iboot_hmac_config_count - 1;

    while (l <= r) {
        ssize_t c = (l + r) / 2;

        const iboot_hmac_config_t *current = (const iboot_hmac_config_t *)&current_config[c];

        if (current->hmac < hmac) { 
            l = c + 1;
        } else if (current->hmac > hmac) {
            r = c - 1;
        } else {
            return current->file;
        }
    }

    return NULL;
}

int iboot_push_data(const char *data, size_t data_len, iboot_file_pos_t pos[], size_t *pos_cnt) {
    size_t max_pos_cnt = *pos_cnt;
    size_t _pos_cnt = 0;

    for (size_t i = 0; i < data_len; i++) {
        char c = data[i];

        switch (state) {
            case STATE_WAITING_FOR_HMAC: {
                int8_t hex_digit = iboot_hmac_character(c);

                if (hex_digit != -1) {
                    if (hmac_digit_count == 64 / 4) {
                        iboot_clear_state();
                        break;
                    }

                    current_hmac |= (uint64_t)hex_digit << ((64 - hmac_digit_count * 4) - 4);

                    hmac_digit_count++;

                } else {
                    if (c == ':' && hmac_digit_count > 0) {
                        if (hmac_digit_count != 64 / 4) {
                            current_hmac >>= (64 - hmac_digit_count * 4);
                        }

                        state = STATE_WAITING_FOR_LINE;
                    } else {
                        iboot_clear_state();
                    }
                }

                break;
            }

            case STATE_WAITING_FOR_LINE: {
                int8_t line_digit = iboot_line_character(c);

                if (line_digit != -1) {
                    if (line_digit_count == 6) {
                        iboot_clear_state();
                        break;
                    }

                    current_line = current_line * 10 + line_digit;

                } else {
                    if (c == '\r' || c == '\n' || c == ' ') {
                        const char *file = iboot_find_file_for_hmac(current_hmac);

                        if (file) {
                            iboot_file_pos_t *curr = &pos[_pos_cnt];

                            curr->off = i;
                            curr->file = file;
                            curr->line = current_line;

                            _pos_cnt++;

                            if (_pos_cnt == max_pos_cnt) {
                                return -1;
                            }
                        }

                        iboot_clear_state();
                    }
                }

                break;
            }
        }
    }

    *pos_cnt = _pos_cnt;

    return 0;
}

#define START_FILE  "\t" ANSI_START ANSI_GREEN ANSI_DELIM ANSI_BOLD ANSI_END "<"
#define END_FILE    ">" ANSI_START ANSI_RESET ANSI_END

void iboot_print_file(int fd, iboot_file_pos_t *pos) {
    char line_buf[8] = { 0 };
    char *line = NULL;

    write(fd, START_FILE, CONST_STRLEN(START_FILE));
    write(fd, pos->file, strlen(pos->file));
    write(fd, ":", 1);
    line = itoa(pos->line, line_buf, sizeof(line_buf));
    write(fd, line, sizeof(line_buf) - (line - line_buf));
    write(fd, END_FILE, CONST_STRLEN(END_FILE));
}
