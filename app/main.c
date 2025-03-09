#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <mach-o/getsect.h>

#include <app/config.h>
#include <app/driver.h>
#include <app/event.h>
#include <app/term.h>
#include <app/misc.h>
#include <app/tty.h>
#include "lolcat.h"
#include "config.h"
#include "event.h"
#include "iboot.h"
#include "log.h"

#define DEFAULT_DRIVER  "uart"

static app_config_t config = { 0 };

static struct {
    const driver_t *driver;
    event_t event;
} ctx = { 0 };

static int app_get_driver(const driver_t **driver, int *argc, const char ***argv) {
    const char *driver_name = DEFAULT_DRIVER;

    if (*argc == 1) {
        goto find;
    } else {
        const char *argv1 = (*argv)[1];
        if (argv1[0] != '-') {
            driver_name = argv1;
            (*argv)++;
            (*argc)--;
        }
    }

find:
    DRIVER_ITERATE({
        if (strcmp(driver_name, curr->name) == 0) {
            *driver = curr;
            break;
        }
    });

    if (!*driver) {
        POLINA_ERROR("driver \"%s\" is not known", driver_name);
        return -1;
    }

    return 0;
}

static int app_term_set_raw() {
    int ret = -1;
    struct termios new_attrs = { 0 };

    REQUIRE_NOERR(tty_get_attrs(STDIN_FILENO, &new_attrs), out);
    cfmakeraw(&new_attrs);

    if (config.filter_return) {
        new_attrs.c_oflag  = OPOST;
        new_attrs.c_oflag |= ONLCR;
    }

    new_attrs.c_cc[VTIME] = 1;
    new_attrs.c_cc[VMIN] = 0;

    REQUIRE_NOERR(tty_set_attrs(STDIN_FILENO, &new_attrs), out);
    ret = 0;

out:
    return ret;
}

int app_event_signal(app_event_t event) {
    event_signal(&ctx.event, event);
    return 0;
}

static app_event_t app_event_loop() {
    app_event_t event = event_wait(&ctx.event);

    POLINA_LINE_BREAK();

    switch (event) {
        case APP_EVENT_DISCONNECT_DEVICE: {
            POLINA_INFO("[disconnected - device disappeared]");
            break;
        }

        case APP_EVENT_DISCONNECT_USER: {
            POLINA_INFO("[disconnected]");
            break;
        }

        case APP_EVENT_ERROR: {
            POLINA_ERROR("[internal error]");
            break;
        }

        default:
            break;
    }

    event_unsignal(&ctx.event);

    return event;
}

#define APP_OUT_BUFFER_SIZE     (DRIVER_MAX_BUFFER_SIZE * 8 + 32)
#define APP_OUT_IBOOT_POS_CNT   (128)

static int app_out_callback(char *in_buf, size_t in_len) {
    char *buf = in_buf;
    size_t len = in_len;

    char processed[APP_OUT_BUFFER_SIZE];
    size_t processed_len = APP_OUT_BUFFER_SIZE;

    iboot_file_pos_t iboot_pos[APP_OUT_IBOOT_POS_CNT];
    size_t iboot_pos_cnt = APP_OUT_IBOOT_POS_CNT;

    uint16_t offs[DRIVER_MAX_BUFFER_SIZE];
    size_t offs_cnt = DRIVER_MAX_BUFFER_SIZE;

    /* lolcat & iBoot filters need XXX */
    if (config.filter_lolcat || config.filter_iboot) {
        uint16_t *_offs = NULL;
        size_t *_offs_cnt = NULL;

        if (config.filter_iboot) {
            REQUIRE_NOERR(iboot_push_data(in_buf, in_len, iboot_pos, &iboot_pos_cnt), fail);
            _offs = offs;
            _offs_cnt = &offs_cnt;
        }

        if (config.filter_lolcat) {
            REQUIRE_NOERR(lolcat_push_data(in_buf, in_len, processed, &processed_len, _offs, _offs_cnt), fail);
            buf = processed;
            len = processed_len;
        }
    }

    if (config.filter_iboot && iboot_pos_cnt) {
        off_t buf_off = 0;

        for (size_t i = 0; i < iboot_pos_cnt; i++) {
            iboot_file_pos_t *curr = &iboot_pos[i];
            uint16_t curr_char_off = curr->off;

            if (config.filter_lolcat) {
                curr_char_off = offs[curr_char_off];
            }

            REQUIRE(curr_char_off <= len, fail);

            write(STDOUT_FILENO, buf + buf_off, curr_char_off - buf_off);
            buf_off += curr_char_off;

            iboot_print_file(STDOUT_FILENO, curr);
        }

        if (buf_off < len) {
            write(STDOUT_FILENO, buf + buf_off, len - buf_off);
        }
    } else {
        write(STDOUT_FILENO, buf, len);
    }

    if (!config.logging_disabled) {
        REQUIRE_NOERR(log_push(in_buf, in_len), fail);
    }

    return 0;

fail:
    return -1;
}

static int app_handle_user_input(char c) {
    if (config.filter_delete && c == 0x7F) {
        c = 0x08;
    }

    if (ctx.driver->write(&c, sizeof(c)) != 0) {
        return -1;
    }

    if (config.delay) {
        usleep(config.delay);
    }

    return 0;
}

static void *app_user_input_thread(void *arg) {
    struct kevent ke = { 0 };
    app_event_t event = APP_EVENT_NONE;

    int kq = kqueue();
    if (kq == -1) {
        POLINA_ERROR("kqueue() failed");
        event = APP_EVENT_ERROR;
        goto out;
    }
    
    EV_SET(&ke, STDIN_FILENO, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(kq, &ke, 1, NULL, 0, NULL);

    char c = 0;
    while (true) {         
        memset(&ke, 0, sizeof(ke));
        int i = kevent(kq, NULL, 0, &ke, 1, NULL);
        
        if (i == 0) {
            continue;
        }

        ssize_t r = read(STDIN_FILENO, &c, 1);

        if (c == 0x1D) {
            event = APP_EVENT_DISCONNECT_USER;
            goto out;
        }

        if (app_handle_user_input(c) != 0) {
            event = APP_EVENT_ERROR;
            goto out; 
        } 
    }

out:
    app_event_signal(event);
    close(kq);

    return NULL;
}

void app_config_print() {
    ctx.driver->config_print();
    app_config_print_internal(&config);
}

static char *__get_tag() {
    static char *tag;

    if (tag) {
        return tag;
    }

    unsigned long size;
    char *data = (char *)getsectiondata(dlsym(RTLD_MAIN_ONLY, "_mh_execute_header"), "__TEXT", "__build_tag", &size);
    if (!data) {
        POLINA_WARNING("couldn't get embedded build tag");
        data = PRODUCT_NAME "-???";
        size = sizeof(PRODUCT_NAME "-???");
    }

    tag = malloc(size + 1);
    memcpy(tag, data, size);
    tag[size] = '\0';

    return tag;
}

void app_version() {
    POLINA_INFO("%s", __get_tag());
    POLINA_INFO("made by john (@nyan_satan)");
    POLINA_LINE_BREAK();
}

static void help(const char *program_name) {
    printf("usage: %s DRIVER <options>\n", program_name);
    printf("\n");

    DRIVER_ITERATE({
        POLINA_INFO_NO_BREAK("%s", curr->name);
        printf(":\n");
        curr->help();
        printf("\n");
    });

    printf("available filter options:\n");
    printf("\t-n\tadd \\r to lines without it, good for diags/probe debug logs/etc.\n");
    printf("\t-k\treplace delete keys (0x7F) with backspace (0x08), good for diags\n");
    printf("\t-i\ttry to identify filenames in obfuscated iBoot output\n");
    printf("\n");

    printf("available miscallenous options:\n");
    printf("\t-r\ttry to reconnect in case of connection loss\n");
    printf("\t-u <usecs>\tdelay period in microseconds after each inputted character,\n");
    printf("\t\t\tdefault - 0 (no delay), 20000 is good for XNU\n");
    printf("\t-l\tlolcat the output, good for screenshots\n");
    printf("\t-g\tdisable logging\n");
    printf("\t-h\tshow this help menu\n");
    printf("\n");

    printf("default DRIVER is \"" DEFAULT_DRIVER "\"\n");
    printf("logs are collected to ~/Library/Logs/" PRODUCT_NAME "/\n");
}

int main(int argc, const char *argv[]) {    
    int ret = -1;

    app_term_scroll();
    app_term_clear_page();

    /* print version */
    app_version();

    /* getting driver & advancing args if needed */
    if (app_get_driver(&ctx.driver, &argc, &argv) != 0) {
        return -1;
    }

    /* load app configuration from args */
    if (app_config_load(argc, argv, &config) != 0) {
        help(argv[0]);
        return -1;
    }

    /* initialize selected driver */
    REQUIRE_NOERR(ctx.driver->init(argc, argv), out);

    /* XXX */
    app_config_print();

    /* save TTY's configuration to restore it later */
    REQUIRE_NOERR(app_term_save_attrs(), out);

    /* set terminal to raw mode */
    REQUIRE_NOERR(app_term_set_raw(), out);

    /* driver preflight - XXX */
    REQUIRE_NOERR(ctx.driver->preflight(), out);

    /* initialize logging subsystem if needed */
    if (!config.logging_disabled) {
        char dev_name[PATH_MAX + 1] = { 0 };
        ctx.driver->log_name(dev_name, sizeof(dev_name));

        REQUIRE_NOERR(log_init(dev_name), out);
    }

    /* initialize lolcat if needed */
    if (config.filter_lolcat) {
        lolcat_init();
    }

    /* initialize app event struct */
    event_init(&ctx.event);

    /* starting the driver */
    REQUIRE_NOERR(ctx.driver->start(app_out_callback), out);

    /* if we reach this point... */
    POLINA_INFO("\n[connected - press CTRL+] to exit]");

    /* create user input handler thread */
    pthread_t user_input_thr = { 0 };
    pthread_create(&user_input_thr, NULL, app_user_input_thread, NULL);

    app_event_t event;
    while (true) {
        /* this will block until we disconnect or something errors out */
        event = app_event_loop();

        if (config.retry && event == APP_EVENT_DISCONNECT_DEVICE) {
            POLINA_WARNING("[trying to reconnect]");
            REQUIRE_NOERR(ctx.driver->restart(), out);
            POLINA_SUCCESS("[reconnected]");
        } else {
            break;
        }
    }

    ret = event == APP_EVENT_ERROR ? -1 : 0;

out:
    /* shutting down the driver */
    ctx.driver->quiesce();

    /* shutting down logging subsystem - this can take a bit of time */
    if (!config.logging_disabled) {
        log_queisce();
    }

    /* restore terminal config back to original state */
    if (app_term_restore_attrs() != 0) {
        POLINA_ERROR("could NOT restore terminal attributes - you might want to kill it");
        ret = -1;
    }

    return ret;
}
