#ifndef APP_APP_H
#define APP_APP_H

#include <stdbool.h>

void app_version();
void app_print_cfg();
bool app_config_arg_consumed(char c);
int  app_quiesce(int ret);

#include <app/driver.h>
#include <app/event.h>
#include <app/halt.h>
#include <app/ll.h>
#include <app/misc.h>
#include <app/term.h>
#include <app/tty.h>

#endif
