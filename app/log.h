#ifndef LOG_H
#define LOG_H

int  log_init(const char *dev_name);
int  log_push(const char *buf, size_t len);
void log_queisce();

#endif
