#ifndef APP_HALT_H
#define APP_HALT_H

#include "misc.h"

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
        panic("REQUIRE failed - " #_cond); \
    }

#define REQUIRE_PANIC_NOERR(_expr) \
    if (_expr != 0) { \
        panic("REQUIRE_NOERR failed - " #_expr); \
    }

__attribute__((noreturn))
void _panic(const char *file, const char *func, int line, const char *fmt, ...);

#define panic(x...) _panic(__FILE__, __FUNCTION__, __LINE__, x)

#endif
