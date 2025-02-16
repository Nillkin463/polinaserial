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
    struct termios old_attrs;
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
        ERROR("driver \"%s\" is not known", driver_name);
        return -1;
    }

    return 0;
}

static int app_tty_set_raw() {
    struct termios new_attrs = { 0 };
    memcpy(&new_attrs, &ctx.old_attrs, sizeof(new_attrs));

    cfmakeraw(&new_attrs);

    if (config.filter_return) {
        new_attrs.c_oflag  = OPOST;
        new_attrs.c_oflag |= ONLCR;
    }

    new_attrs.c_cc[VTIME] = 1;
    new_attrs.c_cc[VMIN] = 0;

    return tty_set_attrs(STDIN_FILENO, &new_attrs);
}

int app_event_signal(app_event_t event) {
    event_signal(&ctx.event, event);
    return 0;
}

static void app_event_loop() {
    app_event_t event = event_wait(&ctx.event);

    printf("\n\r");

    switch (event) {
        case APP_EVENT_DISCONNECT_DEVICE: {
            INFO("[disconnected - device disappeared]\r");
            break;
        }

        case APP_EVENT_DISCONNECT_USER: {
            INFO("[disconnected]\r");
            break;
        }

        case APP_EVENT_ERROR: {
            INFO("[internal error]\r");
            break;
        }

        default:
            break;
    }

    event_unsignal(&ctx.event);
}

static int app_out_callback(char *buffer, size_t length) {
    /* lolcat & iBoot filters need per-character handling */
    if (config.filter_lolcat || config.filter_iboot) {
        for (size_t i = 0; i < length; i++) {
            char c = buffer[i];

            /*
             * iBoot filter doesn't write output on its' own,
             * and yet we still have to push characters one by one
             * to be able to insert iBoot filenames appropriately
             */
            if (config.filter_iboot) {
                iboot_push_char(STDOUT_FILENO, c);
            }

            /* lolcat writes on its' own */
            if (config.filter_lolcat) {
                lolcat_print_char(STDOUT_FILENO, c);
            } else {
                write(STDOUT_FILENO, &c, sizeof(c));
            }
        }
    } else {
        write(STDOUT_FILENO, buffer, length);
    }

    if (!config.logging_disabled) {
        if (log_push(buffer, length) != 0) {
            return -1;
        }
    }

    return 0;
}

static int app_handle_user_input(char c) {
    if (config.filter_delete && c == 0x7F) {
        c = 0x08;
    }

    if (ctx.driver->write(&c, sizeof(c)) != 0) {
        return -1;
    }

    if (config.wait) {
        usleep(config.wait);
    }

    return 0;
}

static void *app_user_input_thread(void *arg) {
    struct kevent ke = { 0 };
    app_event_t event = APP_EVENT_NONE;

    int kq = kqueue();
    if (kq == -1) {
        ERROR("kqueue() failed");
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

static void version() {
    unsigned long size;
    uint8_t *data = getsectiondata(dlsym(RTLD_MAIN_ONLY, "_mh_execute_header"), "__TEXT", "__build_tag", &size);
    if (data) {
        char tag[size + 1];
        memcpy(tag, data, size);
        tag[size] = '\0';
        INFO("%s", tag);
    } else {
        WARNING("failed to get embedded build tag");
        INFO(PRODUCT_NAME "-???");
    }

    INFO("made by john (@nyan_satan)");
    printf("\n");
}

static void help(const char *program_name) {
    printf("usage: %s DRIVER <options>\n", program_name);
    printf("\n");

    DRIVER_ITERATE({
        INFO_NO_BREAK("%s", curr->name);
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
    printf("\t-w <usecs>\twait period in microseconds after each inputted character,\n");
    printf("\t\t\tdefault - 0 (no wait), 20000 is good for XNU\n");
    printf("\t-l\tlolcat the output, good for screenshots\n");
    printf("\t-g\tdisable logging\n");
    printf("\t-h\tshow this help menu\n");
    printf("\n");

    printf("default DRIVER is \"" DEFAULT_DRIVER "\"\n");
    printf("logs are collected to ~/Library/Logs/" PRODUCT_NAME "/\n");
}

int main(int argc, const char *argv[]) {    
    int ret = -1;
    bool tty_inited = false;

    /* print version */
    version();

    /* getting driver & advancing args if needed */
    if (app_get_driver(&ctx.driver, &argc, &argv) != 0) {
        return -1;
    }

    /* load app configuration from args */
    if (app_config_load(argc, argv, &config) != 0) {
        help(argv[0]);
        return -1;
    }

    /* initialize app event struct */
    event_init(&ctx.event);

    /* initialize selected driver */
    REQUIRE_NOERR(ctx.driver->init(argc, argv), out);

    /* print driver configuration */
    ctx.driver->config_print();

    /* print app configuration */
    app_config_print(&config);

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

    /* save TTY's configuration to restore it later */
    REQUIRE_NOERR(tty_get_attrs(STDIN_FILENO, &ctx.old_attrs), out);

    /* set terminal to raw mode */
    REQUIRE_NOERR(app_tty_set_raw(), out);
    tty_inited = true;

    /* starting the driver */
    REQUIRE_NOERR(ctx.driver->start(app_out_callback), out);

    /* if we reach this point... */
    INFO("\n[connected - press CTRL+] to exit]");

    /* create user input handler thread */
    pthread_t user_input_thr = { 0 };
    pthread_create(&user_input_thr, NULL, app_user_input_thread, NULL);

    /* this will block until we disconnect or something errors out */
    app_event_loop();

    ret = ctx.event.value == APP_EVENT_ERROR ? -1 : 0;

out:
    /* shutting down the driver */
    ctx.driver->quiesce();

    /* shutting down logging subsystem - this can take a bit of time */
    if (!config.logging_disabled) {
        log_queisce();
    }

    /* restore terminal config back to original state */
    if (tty_inited) {
        if (tty_set_attrs(STDIN_FILENO, &ctx.old_attrs) != 0) {
            ERROR("could NOT restore TTY's attributes - you might want to kill this terminal\r");
            ret = -1;
        }
    }

    return ret;
}
