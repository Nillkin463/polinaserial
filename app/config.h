#ifndef APP_CONFIG_INTERNAL_H
#define APP_CONFIG_INTERNAL_H

#include <stdbool.h>
#include <unistd.h>

typedef struct {
    /* filter */
    bool filter_return;
    bool filter_delete;
    bool filter_iboot;
    bool filter_lolcat;

    /* state */
    useconds_t delay;
    bool retry;

    /* miscallenous */
    bool logging_disabled;
} app_config_t;

int  app_config_load(int argc, const char *argv[], app_config_t *config);
void app_config_print_internal(app_config_t *config);

#endif
