#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include <app/driver.h>
#include <app/config.h>
#include <app/event.h>
#include <app/misc.h>
#include <app/tty.h>

#include "menu/menu.h"
#include "baudrate_presets.h"
#include "config.h"
#include "device.h"

static struct {
    bool ready;

    serial_dev_t *picked;
    char callout[PATH_MAX + 1];

    int dev_fd;

    struct termios old_attrs;
    bool attrs_modified;

    int kq;
    pthread_t loop_thr;
} ctx = { 0 };

static serial_config_t config = { 0 };

static int init(int argc, const char *argv[]) {
    int ret = -1;

    ctx.dev_fd = -1;

    REQUIRE_NOERR(serial_config_load(argc, argv, &config), out);

    ret = 0;

out:
    return ret;
}

static int preflight() {
    int ret = -1;

    if (!config.device) {
        ctx.picked = menu_pick();
        if (!ctx.picked) {
            goto out;
        }

        strlcpy(ctx.callout, ctx.picked->callout, sizeof(ctx.callout));
    } else {
        if (serial_find_devices() != 0) {
            ERROR("couldn't look up serial devices");
            goto out;
        }

        ctx.picked = serial_dev_find_by_callout(config.device);
        strlcpy(ctx.callout, config.device, strlen(config.device) + 1);
    }

    ctx.ready = true;
    ret = 0;

out:
    return ret;
}

#define LOOP_SHUTDOWN_ID    (613)

static void *serial_loop(void *arg) {
    driver_event_cb_t cb = arg;
    app_event_t event = APP_EVENT_NONE;
    struct kevent ke = { 0 };
    char buf[DRIVER_MAX_BUFFER_SIZE];

    ctx.kq = kqueue();
    if (ctx.kq == -1) {
        ERROR("kqueue() failed");
        return NULL;
    }

    EV_SET(&ke, LOOP_SHUTDOWN_ID, EVFILT_USER, EV_ADD, 0, 0, NULL);
    kevent(ctx.kq, &ke, 1, NULL, 0, NULL);

    EV_SET(&ke, ctx.dev_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(ctx.kq, &ke, 1, NULL, 0, NULL);

    EV_SET(&ke, ctx.dev_fd, EVFILT_VNODE, EV_ADD, NOTE_DELETE, 0, NULL);
    kevent(ctx.kq, &ke, 1, NULL, 0, NULL);

    while (true) {
        memset(&ke, 0, sizeof(ke));
        int i = kevent(ctx.kq, NULL, 0, &ke, 1, NULL);

        if (i == 0) {
            continue;
        }

        if (ke.filter == EVFILT_VNODE && ke.fflags & NOTE_DELETE) {
            ctx.dev_fd = -1;
            event = APP_EVENT_DISCONNECT_DEVICE;
            goto out;

        } else if (ke.filter == EVFILT_READ && ke.ident == ctx.dev_fd) {
            ssize_t r = read(ctx.dev_fd, buf, sizeof(buf));

            if (r > 0) {
                if (cb(buf, r) != 0) {
                    event = APP_EVENT_ERROR;
                    goto out;
                }
            }
        } else if (ke.filter == EVFILT_USER) {
            goto out_no_signal;
        }
    }

out:
    app_event_signal(event);

out_no_signal:
    return NULL;
}

static void close_fd() {
    if (ctx.attrs_modified) {
        tty_set_attrs(ctx.dev_fd, &ctx.old_attrs);
    }

    close(ctx.dev_fd);
    ctx.dev_fd = -1;
}

static int start(driver_event_cb_t out_cb) {
    int ret = -1;
    struct termios new_attrs = { 0 };

    REQUIRE((ctx.dev_fd = device_open_with_callout(ctx.callout)) != -1, fail);

    REQUIRE_NOERR(tty_get_attrs(ctx.dev_fd, &ctx.old_attrs), fail);
    memcpy(&new_attrs, &ctx.old_attrs, sizeof(struct termios));

    tty_set_attrs_from_config(&new_attrs, &config);

    REQUIRE_NOERR(tty_set_attrs(ctx.dev_fd, &new_attrs), fail);
    REQUIRE_NOERR(device_set_speed(ctx.dev_fd, config.baudrate), fail);

    ctx.attrs_modified = true;

    pthread_create(&ctx.loop_thr, NULL, serial_loop, out_cb);

    ret = 0;
    goto out;

fail:
    if (ctx.dev_fd != -1) {
        close_fd();
    }

out:
    return ret;
}

static int _write(char *buf, size_t len) {
    write(ctx.dev_fd, buf, len);
    return 0;
}

static int quiesce() {
    if (ctx.dev_fd != -1) {
        struct kevent ke = { 0 };
        EV_SET(&ke, LOOP_SHUTDOWN_ID, EVFILT_USER, 0, NOTE_TRIGGER, 0, NULL);
        kevent(ctx.kq, &ke, 1, 0, 0, NULL);

        pthread_join(ctx.loop_thr, NULL);

        close(ctx.kq);

        close_fd();
    }

    serial_dev_list_destroy();

    memset(&ctx, 0, sizeof(ctx));

    return 0;
}

static void log_name(char name[], size_t len) {
    if (!ctx.ready) {
        ERROR("serial driver uninitialized, but log_name() was called?!");
        abort();
    }

    if (ctx.picked) {
        strlcpy(name, ctx.picked->tty_name, len);
    } else {
        strlcpy(name, last_path_component(ctx.callout), len);
    }
}

static void config_print() {
    serial_config_print(&config);
}

DRIVER_ADD(uart, init, preflight, start, _write, quiesce, log_name, config_print, serial_help);
