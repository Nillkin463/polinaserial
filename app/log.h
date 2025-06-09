#ifndef LOG_H
#define LOG_H

#include <stdint.h>
#include <stddef.h>

int  log_init(const char *dev_name);
int  log_push(const uint8_t *buf, size_t len);
void log_queisce();

#endif
