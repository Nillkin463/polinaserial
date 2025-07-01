#ifndef APP_HALT_H
#define APP_HALT_H


__attribute__((noreturn))
void _panic(const char *file, const char *func, int line, const char *fmt, ...);

/* 
 * call panic() (printf-like) in case of an unexpected & fatal error,
 * it will print a traceback & other infos for debug
 */
#define panic(x...) _panic(__FILE__, __FUNCTION__, __LINE__, x)

#endif
