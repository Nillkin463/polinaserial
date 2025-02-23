#include <sys/stat.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include <app/misc.h>
#include "event.h"
#include "log.h"

#define LOGS_FOLDER     "Library/Logs/" PRODUCT_NAME
#define BUFFER_SIZE     (8192)

typedef struct {
    char buf[BUFFER_SIZE];
    size_t cnt;
} log_buffer_t;

static struct {
    pthread_t thr;
    event_t flush_event;

    log_buffer_t bufs[2];
    int curr_buf;

    int fd;
    bool inited;
    bool started;
    char path[PATH_MAX + 1];
} ctx = { 0 };

typedef struct __attribute__((packed)) {
    uint8_t lock : 1;
    uint8_t idx : 1;
    uint8_t quiesce : 1;
} flush_event_t;

static void *log_thread_handler(void *arg) {
    while (true) {
        uint64_t _ev = event_wait(&ctx.flush_event);
        flush_event_t ev = *(flush_event_t *)&_ev;
        log_buffer_t *buf = &ctx.bufs[ev.idx];

        write(ctx.fd, buf->buf, buf->cnt);
        buf->cnt = 0;

        if (ev.quiesce) {
            goto out;
        }

        event_unsignal(&ctx.flush_event);
    }

out:
    return NULL;
}

static int log_push_char(char c) {
    log_buffer_t *buf = &ctx.bufs[ctx.curr_buf];

    if (buf->cnt == BUFFER_SIZE) {
        flush_event_t ev_type = {true, ctx.curr_buf, false};
        event_signal(&ctx.flush_event, *((uint64_t *)&ev_type));

        ctx.curr_buf = ~ctx.curr_buf & 1;

        buf = &ctx.bufs[ctx.curr_buf];
        buf->cnt = 0;
    }

    buf->buf[buf->cnt] = c;
    buf->cnt++;

    return 0;
}

static bool is_loggable_character(char c) {
    return (c >= 0x20 && c <= 0x7e) || c == '\t' || c == '\n';
}

int log_push(const char *buf, size_t len) {
    if (!ctx.started) {
        ctx.started = true;
    }

    for (size_t i = 0; i < len; i++) {
        char c = buf[i];
    
        if (is_loggable_character(c)) {
            if (log_push_char(c) != 0) {
                return -1;
            }
        }
    }

    return 0;
}

int log_init(const char *dev_name) {
    ctx.fd = -1;

    const char *home_folder = getenv("HOME");
    if (!home_folder) {
        ERROR("couldn't get $HOME");
        return -1;
    }

    if (snprintf(ctx.path, sizeof(ctx.path), "%s/" LOGS_FOLDER, home_folder) >= sizeof(ctx.path)) {
        ERROR("resulting folder path for logging is getting too big (?!)");
        return -1;
    }

    if (snprintf(ctx.path, sizeof(ctx.path), "%s/%s", ctx.path, dev_name) >= sizeof(ctx.path)) {
        ERROR("resulting folder path for logging is getting too big (?!)");
        return -1;
    }

    if (mkdir_recursive(ctx.path) != 0) {
        ERROR("couldn't create logging folder");
        return -1;
    }

    char filename[128] = { 0 };
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    if (snprintf(filename, sizeof(filename), "%d-%02d-%02d_%02d-%02d-%02d.log",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec) >= sizeof(filename)) {
        
        ERROR("resulting filename for logging is getting too big (?!)");
        return -1;
    }

    printf("\n");
    INFO_NO_BREAK("Logging folder: ");
    puts(ctx.path);

    INFO_NO_BREAK("Logging file: ");
    puts(filename);

    if (snprintf(ctx.path, sizeof(ctx.path), "%s/%s", ctx.path, filename) >= sizeof(ctx.path)) {
        ERROR("resulting path for logging is getting too big (?!)");
        return -1;
    }

    if ((ctx.fd = open(ctx.path, O_WRONLY | O_APPEND | O_CREAT, 0644)) < 0) {
        ERROR("couldn't create logging file");
        return -1;
    }

    event_init(&ctx.flush_event);

    pthread_create(&ctx.thr, NULL, log_thread_handler, NULL);

    ctx.inited = true;

    return 0;
}

void log_queisce() {
    if (ctx.inited) {
        flush_event_t ev_type = {true, ctx.curr_buf, true};
        event_signal(&ctx.flush_event, *((uint64_t *)&ev_type));
        pthread_join(ctx.thr, NULL);

        if (ctx.fd != -1) {
            close(ctx.fd);
        }
    }

    if (!ctx.started) {
        remove(ctx.path);
    }

    memset(&ctx, 0, sizeof(ctx));
}
