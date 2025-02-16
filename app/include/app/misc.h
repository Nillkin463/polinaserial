#ifndef APP_MISC_H
#define APP_MISC_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define STR_IMPL_(x) #x
#define STR(x) STR_IMPL_(x)

#define INFO_NO_BREAK(format, ...) printf("\033[1m" format "\033[0m", ##__VA_ARGS__);
#define INFO(format, ...) printf("\033[1m" format "\033[0m\r\n", ##__VA_ARGS__);
#define SUCCESS(format, ...) printf("\033[1;32m" format "\033[0m\r\n", ##__VA_ARGS__);
#define WARNING(format, ...) printf("\033[1;33m" format "\033[0m\r\n", ##__VA_ARGS__);
#define ERROR(format, ...) printf("\033[1;31m" format "\033[0m\r\n", ##__VA_ARGS__);

#define REQUIRE(_cond, _label) \
    if (!(_cond)) { \
        goto _label; \
    }

#define REQUIRE_NOERR(_expr, _label) \
    if (_expr != 0) { \
        goto _label; \
    }

#define REQUIRE_PANIC(_cond) \
    if (!(_cond)) { \
        ERROR("REQUIRE_PANIC - " #_cond); \
        abort(); \
    }

int mkdir_recursive(const char *path);
int parse_numeric_arg(const char *arg, int base, uint64_t *val, uint64_t min_val, uint64_t max_val);
const char *bool_on_off(bool status);
const char *last_path_component(const char *path);

#endif
