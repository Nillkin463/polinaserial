#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <app/tty.h>
#include <app/term.h>
#include <app/misc.h>

#define SCROLL_UP_FMT   "\033[%dS"
#define CLEAR_TERM      "\033[H\033[J"
#define CLEAR_LINE      "\033[2K\r"
#define HIDE_CURSOR     "\033[?25l"
#define SHOW_CURSOR     "\033[?25h"

int app_term_scroll() {
    struct winsize w = { 0 };
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
        return -1;
    }

    printf(SCROLL_UP_FMT, w.ws_row);

    return 0;
}

void app_term_clear_page() {
    printf(CLEAR_TERM);
}

void app_term_clear_line() {
    printf(CLEAR_LINE);
}

void app_term_hide_cursor() {
    printf(HIDE_CURSOR);
}

void app_term_show_cursor() {
    printf(SHOW_CURSOR);
}

static bool attrs_were_saved = false;
static struct termios saved_attrs = { 0 };

int app_term_save_attrs() {
    if (attrs_were_saved) {
        ERROR("trying to save terminal attributes again");
        return -1;
    }

    REQUIRE_NOERR(tty_get_attrs(STDIN_FILENO, &saved_attrs), fail);

    attrs_were_saved = true;

    return 0;

fail:
    ERROR("couldn't save terminal attributes");
    return -1;
}

int app_term_restore_attrs() {
    if (attrs_were_saved) {
        return tty_set_attrs(STDIN_FILENO, &saved_attrs);
    } else {
        return 0;
    }
}
