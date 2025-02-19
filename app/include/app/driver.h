#ifndef APP_DRIVER_H
#define APP_DRIVER_H

#include <stddef.h>
#include <limits.h>

#define DRIVER_MAX_BUFFER_SIZE  (1024)

typedef int (*driver_event_cb_t)(char *buf, size_t len);

typedef struct {
    char name[24];
    int  (*init)(int argc, const char *argv[]);
    int  (*preflight)();
    int  (*start)(driver_event_cb_t out_cb);
    int  (*write)(char *buf, size_t len);
    int  (*quiesce)();
    void (*log_name)(char name[], size_t len);
    void (*config_print)();
    void (*help)();
} driver_t;

#define DRIVER_ADD(name, init, preflight, start, write, quiesce, log_name, config_print, help) \
    __attribute__((used)) driver_t __driver_##name __attribute__((section("__DATA,__drivers"))) = \
        {#name, init, preflight, start, write, quiesce, log_name, config_print, help};

extern void *_gDrivers      __asm("section$start$__DATA$__drivers");
extern void *_gDriversEnd   __asm("section$end$__DATA$__drivers");

static const driver_t *gDrivers = (driver_t *)&_gDrivers;
static const driver_t *gDriversEnd = (driver_t *)&_gDriversEnd;

#define DRIVER_ITERATE(action) \
    for (int i = 0; i < ((void *)gDriversEnd - (void *)gDrivers) / sizeof(driver_t); i++) { \
        const driver_t *curr = gDrivers + i; \
        action \
    }

#endif
