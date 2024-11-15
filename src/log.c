#include <log.h>
#include <utils.h>

#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <aio.h>


#define LOGS_FOLDER "Library/Logs/" PRODUCT_NAME
#define BUFFER_SIZE 8192

char path[PATH_MAX + 1];
int log_fd = -1;
bool logging_started = false;

struct log_buffer {
    char buffer[BUFFER_SIZE];
    size_t count;
    struct aiocb aiocbp;
    bool aio_started;
};

struct log_buffer buffers[2];

struct timespec timeout = {
    .tv_sec = 1,
    .tv_nsec = 0
};

int curr_buffer = 0;

static int aio_write_queue(int fd, struct log_buffer *buffer) {
    memset(&buffer->aiocbp, 0, sizeof(struct aiocb));

    buffer->aiocbp.aio_fildes = fd;
    buffer->aiocbp.aio_offset = 0;
    buffer->aiocbp.aio_buf = buffer->buffer;
    buffer->aiocbp.aio_nbytes = buffer->count;
    buffer->aiocbp.aio_reqprio = 0;
    buffer->aiocbp.aio_sigevent.sigev_notify = SIGEV_NONE;

    if (!buffer->aio_started) {
        buffer->aio_started = true;
    }

    return aio_write(&buffer->aiocbp);
}

static int aio_barrier(struct log_buffer *buffer) {
    struct aiocb *list[] = {
        &buffer->aiocbp
    };

    return aio_suspend((const struct aiocb *const *)&list, 1, &timeout);
}

static int aio_check_return(struct log_buffer *buffer) {
    return aio_return(&buffer->aiocbp) == buffer->count ? 0 : -1;
}

static int aio_check_operation(struct log_buffer *buffer) {
    if (!buffer->aio_started) {
        return 0;
    }

    if (aio_barrier(buffer) != 0) {
        return -1;
    }

    if (aio_check_return(buffer) != 0) {
        return -1;
    }

    return 0;
}

static int log_push_char(char c) {
    if (buffers[curr_buffer].count == BUFFER_SIZE) {
        if (aio_check_operation(&buffers[!curr_buffer]) != 0) {
            ERROR_PRINT("\r\nprevious log write failed - %s\r", strerror(errno));
            return -1;
        }

        if (aio_write_queue(log_fd, &buffers[curr_buffer]) != 0) {
            ERROR_PRINT("\r\nfailed to queue log write - %s\r", strerror(errno));
            return -1;
        }

        curr_buffer = ~curr_buffer & 1;

        buffers[curr_buffer].count = 0;
    }

    buffers[curr_buffer].buffer[buffers[curr_buffer].count] = c;
    buffers[curr_buffer].count++;

    return 0;
}

static bool is_loggable_character(char c) {
    return (c >= 0x20 && c <= 0x7e) || c == '\t' || c == '\n';
}

int log_push(const char *buffer, size_t length) {
    if (!logging_started) {
        logging_started = true;
    }

    for (size_t i = 0; i < length; i++) {
        char c = buffer[i];
    
        if (is_loggable_character(c)) {
            if (log_push_char(c) != 0) {
                return -1;
            }
        }
    }

    return 0;
}

int mkdir_recursive(const char *path) {
    char temp[PATH_MAX + 1];
    char *curr = (char *)&temp;

    if (strlcpy(temp, path, sizeof(temp)) >= sizeof(temp)) {
        ERROR_PRINT("path is too big");
        return -1;
    }

    while (1) {
        char *slash = strchr(curr, '/');

        if (slash) {
            if (slash == curr) {
                curr++;
                continue;
            }

            *slash = '\0';
        }

        if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
            ERROR_PRINT("failed to create a directory - %s", strerror(errno));
            return -1;
        }

        if (slash) {
            *slash = '/';
            curr = slash + 1;
        } else {
            return 0;
        }
    }

}

int log_init(menu_serial_item_t *serial_item) {
    const char *home_folder = getenv("HOME");
    if (!home_folder) {
        ERROR_PRINT("couldn't get $HOME");
        return -1;
    }

    if (snprintf(path, sizeof(path), "%s/" LOGS_FOLDER, home_folder) >= sizeof(path)) {
        ERROR_PRINT("resulting path for logging is getting too big (?!)");
        return -1;
    }

    if (snprintf(path, sizeof(path), "%s/%s", path, STR_FROM_CFSTR(serial_item->tty_name)) >= sizeof(path)) {
        ERROR_PRINT("resulting path for logging is getting too big (?!)");
        return -1;
    }

    if (mkdir_recursive(path) != 0) {
        return -1;
    }

    char filename[128];
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    if (snprintf(filename, sizeof(filename), "%d-%02d-%02d_%02d-%02d-%02d.log",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec) >= sizeof(filename)) {
        
        ERROR_PRINT("resulting filename for logging is getting too big (?!)");
        return -1;
    }

    puts("");
    BOLD_PRINT_NO_RETURN("Logging directory: ");
    puts(path);

    BOLD_PRINT_NO_RETURN("Logging file: ");
    puts(filename);
    puts("");

    if (snprintf(path, sizeof(path), "%s/%s", path, filename) >= sizeof(path)) {
        ERROR_PRINT("resulting path for logging is getting too big (?!)");
        return -1;
    }

    if ((log_fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644)) < 0) {
        ERROR_PRINT("failed to create logging file");
        return -1;
    }

    return 0;
}

void log_queisce() {
    if (log_fd != -1) {
        if (aio_check_operation(&buffers[!curr_buffer]) != 0) {
            ERROR_PRINT("previous log write failed (on exit) - %s\r", strerror(errno));
        } else {
            write(log_fd, buffers[curr_buffer].buffer, buffers[curr_buffer].count);
        }

        close(log_fd);
    }

    if (!logging_started) {
        remove(path);
    }
}

