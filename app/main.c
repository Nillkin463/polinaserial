#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/event.h>
#include <mach-o/getsect.h>

#include <app.h>
#include "lolcat.h"
#include "config.h"
#include "event.h"
#include "iboot.h"
#include "seq.h"
#include "log.h"

#define DEFAULT_DRIVER  "serial"

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

/* gotta switch terminal into "raw" mode to disable echo, control sequences handling and etc. */
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

    /*
     * reset all lolcat color modes, otherwise messages below
     * will have a color of the last character from serial output
     */
    if (config.filter_lolcat) {
        lolcat_reset();
    }

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

/* gotta have this buffer much larger than driver's because of lolcat and etc. */
uint8_t out_buf[DRIVER_MAX_BUFFER_SIZE * 16] = { 0 };
static seq_ctx_t seq_ctx = { 0 };

static int app_out_cb(uint8_t *in_buf, size_t in_len) {
    size_t out_len = 0;
    uint8_t *curr_buf = in_buf;
    size_t left = in_len;

    /* just to be safe */
    if (config.filter_lolcat) {
        lolcat_refresh();
    }

    /* call seq_process_chars() until the packet ends */
    while (left) {
        size_t _out_len = 0;
        lolcat_handler_t lolcatify = NULL;
        iboot_log_line_t iboot_line = { 0 };

        /* returns amount of bytes of the same sequence type until it breaks by a different one */
        int cnt = seq_process_chars(&seq_ctx, curr_buf, left);
        REQUIRE_PANIC(cnt > 0);

        if (cnt > left) {
            panic("too many bytes (cnt: %d, left: %d)", cnt, left);
        }

        switch (seq_ctx.type) {
            /* printable ASCII, just lolcatify if requested */
            case kSeqNormal: {
                if (config.filter_lolcat) {
                    lolcatify = lolcat_push_ascii;
                }

                break;
            }

            /*
             * UTF-8, lolcatify, but ONLY if it starts with first UTF-8 byte,
             * inserting ANSI sequences between UTF-8 char bytes will break it
             */
            case kSeqUnicode: {
                if (config.filter_lolcat) {
                    if (seq_ctx.has_utf8_first_byte) {
                        lolcatify = lolcat_push_one;   
                    }
                }

                break;
            }

            /* control characters, CSI sequences and unrecognized stuff don't get any special handling */
            case kSeqControl:
            case kSeqEscapeCSI:
            case kSeqUnknown:
                break;

            default:
                panic("the hell is this sequence (%d)", seq_ctx.type);
        }

        /* feed current ASCII output into iBoot unobfuscator state machine */
        if (config.filter_iboot && (seq_ctx.type == kSeqNormal)) {
            REQUIRE_PANIC_NOERR(iboot_push_data(curr_buf, cnt));
        }

        /*
         * iBoot always prints carriage return (\r) before new line (\n),
         * good moment for us to ask the state machine if it found something,
         * and if yes, we print it
         */
        if (seq_ctx.type == kSeqControl && *curr_buf == '\r') {
            if (iboot_trigger(&iboot_line)) {
                _out_len = sizeof(out_buf) - out_len;
                REQUIRE_PANIC_NOERR(iboot_output_file(&iboot_line, out_buf + out_len, &_out_len));
                out_len += _out_len;
            }
        }

        if (lolcatify) {
            _out_len = sizeof(out_buf) - out_len;
            REQUIRE_PANIC_NOERR(lolcatify(curr_buf, cnt, out_buf + out_len, &_out_len));
            out_len += _out_len;
        } else {
            memcpy(out_buf + out_len, curr_buf, cnt);
            out_len += cnt;
        }

        if (!config.logging_disabled) {
            switch (seq_ctx.type) {
                /* do not push control sequences into log file */
                case kSeqNormal:
                case kSeqUnicode:
                    REQUIRE_PANIC_NOERR(log_push(curr_buf, cnt));

                default:
                    break;
            }
        }

        curr_buf += cnt;
        left -= cnt;
    }

    /* packet processing done, write into terminal at last */
    write(STDOUT_FILENO, out_buf, out_len);

    return 0;
}

static int app_handle_user_input(uint8_t c) {
    if (config.filter_delete && c == 0x7F) {
        c = 0x08;
    }

    if (ctx.driver->write(&c, sizeof(c)) != 0) {
        POLINA_ERROR("couldn't write into device?!");
        return -1;
    }

    if (config.delay) {
        usleep(config.delay);
    }

    return 0;
}

static void *app_user_input_thread(void *arg) {
    pthread_setname_np("user input loop");

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

static int app_conn_cb() {
    POLINA_INFO("\n[connected - press CTRL+] to exit]");
    return 0;
}

static int app_restart_cb() {
    POLINA_INFO("[reconnected]\n");
    return 0;
}

int app_quiesce(int ret) {
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

void app_print_cfg() {
    ctx.driver->print_cfg();
    app_print_cfg_internal(&config);
}

char __build_tag[256] = { 0 };

/* get our build tag from a separate Mach-O section */
static char *__get_tag() {
    if (*__build_tag) {
        return __build_tag;
    }

    unsigned long size = 0;
    char *data = (char *)getsectiondata(dlsym(RTLD_MAIN_ONLY, "_mh_execute_header"), "__TEXT", "__build_tag", &size);
    if (!data) {
        POLINA_WARNING("couldn't get embedded build tag");
        data = PRODUCT_NAME "-???";
        size = sizeof(PRODUCT_NAME "-???");
    }

    strncpy(__build_tag, data, size);

    return __build_tag;
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
    pthread_setname_np("main");

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

    /* print full config - of both driver and the app itself */
    app_print_cfg();

    /* save TTY's configuration to restore it later */
    REQUIRE_NOERR(app_term_save_attrs(), out);

    /* set terminal to raw mode */
    REQUIRE_NOERR(app_term_set_raw(), out);

    /* driver preflight - menu and etc. */
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
    REQUIRE_NOERR(ctx.driver->start(app_out_cb, app_conn_cb), out);

    /* create user input handler thread */
    pthread_t user_input_thr = { 0 };
    pthread_create(&user_input_thr, NULL, app_user_input_thread, NULL);

    app_event_t event;
    while (true) {
        /* this will block until we disconnect or something errors out */
        event = app_event_loop();

        if (config.retry && event == APP_EVENT_DISCONNECT_DEVICE) {
            POLINA_WARNING("[trying to reconnect, press CTRL+] to exit]");
            REQUIRE_NOERR(ctx.driver->restart(app_restart_cb), out);
        } else {
            break;
        }
    }

    ret = event == APP_EVENT_ERROR ? -1 : 0;

out:
    return app_quiesce(ret);
}
