#include <_stdlib.h>
#include <stdint.h>
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

static int init(int argc, const char *argv[]) {
    return 0;
}

static int preflight() {
    return 0;
}

const static uint8_t others[] = { 0x7, 0x9, 0xa, 0xb, 0xd, 0x1b };

static void *random_loop(void *arg) {
    driver_event_cb_t cb = arg;
    app_event_t event = APP_EVENT_NONE;
    uint8_t buf[DRIVER_MAX_BUFFER_SIZE];

    while (true) {
        uint32_t r = arc4random_uniform(sizeof(buf));
        
        for (uint32_t i = 0; i < r; i++) {
            uint8_t c = (uint8_t)arc4random_uniform(0x6f);
            if (c < 0x10) {
                c = others[arc4random_uniform(sizeof(others))];
            } else {
                c += 0x10;
            }

            buf[i] = c;
        }

        if (cb(buf, r) != 0) {
            event = APP_EVENT_ERROR;
            goto out;
        }

        usleep(arc4random_uniform(250));
    }

out:
    app_event_signal(event);

out_no_signal:
    return NULL;
}

pthread_t loop_thr;

static int start(driver_event_cb_t out_cb) {
    pthread_create(&loop_thr, NULL, random_loop, out_cb);
    return 0;
}

static int restart() {
    return 0;
}

static int _write(uint8_t *buf, size_t len) {
    return 0;
}

static int quiesce() {
    return 0;
}

static void log_name(char name[], size_t len) {
    strlcpy(name, "RANDOM", len);
}

static void config_print() {
}

static void help() {
}

DRIVER_ADD(
    random,
    init,
    preflight,
    start,
    restart,
    _write,
    quiesce,
    log_name,
    config_print,
    help
);
