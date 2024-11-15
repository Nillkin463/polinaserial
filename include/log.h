#ifndef LOG_H
#define LOG_H

#include <menu.h>

int     log_init(menu_serial_item_t *serial_item);
int     log_push(const char *buffer, size_t length);
void    log_queisce();

#endif
