#ifndef APP_APP_H
#define APP_APP_H

#include <stdbool.h>

/* prints app's build tag + copyright string */
void app_version();

/* prints configuration, both app & driver specific */
void app_print_cfg();

/* checks whether argument is consumed by app itself (and not driver) */
bool app_config_arg_consumed(char c);

/* (try to) shutdown the app gracefully, doesn't exit by its' own */
int app_quiesce(int ret);

#include <app/driver.h>
#include <app/event.h>
#include <app/halt.h>
#include <app/ll.h>
#include <app/misc.h>
#include <app/term.h>
#include <app/tty.h>

#endif
