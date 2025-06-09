#include <app/misc.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

extern char __build_tag[];
static char panicMsg[1024] = { 0 };

__attribute__((noreturn))
void _panic(const char *file, const char *func, int line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(panicMsg, sizeof(panicMsg), fmt, ap);
    va_end(ap);

    POLINA_ERROR("\n\n[polinaserial panic]: %s\n", panicMsg);

    POLINA_ERROR("tag:\t%s", __build_tag);
    POLINA_ERROR("file:\t%s:%d", file, line);
    POLINA_ERROR("func:\t%s()", func);

    POLINA_ERROR("\nsomething really terrible has happened, please report this panic");

    abort();
}
