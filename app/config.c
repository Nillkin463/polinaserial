#include <stdlib.h>
#include <app/misc.h>
#include "config.h"

#define APP_ARGUMENTS   ":nkilghw:"

bool app_config_arg_consumed(char c) {
    for (int i = 0; i < sizeof(APP_ARGUMENTS) - 1; i++) {
        if (c == APP_ARGUMENTS[i]) {
            return true;
        }
    }

    return false;
}

static void app_config_load_default(app_config_t *config) {
    /* filters */
    config->filter_return = false;
    config->filter_delete = false;
    config->filter_iboot = false;
    config->filter_lolcat = false;

    /* state */
    config->wait = 0;

    /* miscallenous */
    config->logging_disabled = false;
}

int app_config_load(int argc, const char *argv[], app_config_t *config) {
    app_config_load_default(config);

    opterr = 0;
    optind = 1;
    optreset = 1;

    char c;
    while ((c = getopt(argc, (char *const *)argv, APP_ARGUMENTS)) != -1) {
        switch (c) {
            case 'n': {
                config->filter_return = true;
                break;
            }

            case 'k': {
                config->filter_delete = true;
                break;
            }

            case 'i': {
                config->filter_iboot = true;
                break;
            }

            case 'w': {
                uint64_t tmp = 0;
                if (parse_numeric_arg(optarg, 10, &tmp, 0, UINT64_MAX) != 0) {
                    ERROR("invalid wait period");
                    return -1;
                }

                config->wait = (useconds_t)tmp;
                break;
            }

            case 'l': {
                config->filter_lolcat = true;
                break;
            }

            case 'g': {
                config->logging_disabled = true;
                break;
            }

            case 'h': {
                return -1;
            }

            case ':': {
                WARNING("-%c needs argument", optopt);
                return -1;
            }
            
            case '?': {
                /* might be consumed by drivers */
                break;
            }

            default:
                abort();
        }
    }

    return 0;
}

void app_config_print(app_config_t *config) {
    INFO_NO_BREAK("return: ");
    printf("%s", bool_on_off(config->filter_return));

    INFO_NO_BREAK(" delete: ");
    printf("%s", bool_on_off(config->filter_delete));

    INFO_NO_BREAK(" iBoot: ");
    printf("%s", bool_on_off(config->filter_iboot));

    INFO_NO_BREAK(" \x1B[38;5;154ml\x1B[39m\x1B[38;5;214mo\x1B[39m\x1B[38;5;198ml\x1B[39m\x1B[38;5;164mc\x1B[39m\x1B[38;5;63ma\x1B[39m\x1B[38;5;39mt\x1B[39m\x1B[38;5;49m\x1B[39m: ");
    printf("%s", bool_on_off(config->filter_lolcat));

    INFO_NO_BREAK(" logging: ");
    printf("%s", bool_on_off(!config->logging_disabled));

    INFO_NO_BREAK(" wait: ");
    printf("%d", config->wait);

    printf("\n");
}
