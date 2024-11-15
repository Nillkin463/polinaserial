#include <configuration.h>
#include <baudrate_presets.h>
#include <utils.h>

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#if WITH_UART_EXTRA
#define ARGUMENTS   "d:b:c:s:p:f:nkilgeh"
#else
#define ARGUMENTS   "d:b:nkilgeh"
#endif

#define ENV_CONFIG      "POLINASERIAL"
#define ENV_MAX_SIZE    1024
#define ENV_MAX_ARGS    20
#define ENV_DELIMETER   " "

#define BAUDRATE_MIN    300
#define BAUDRATE_MAX    3000000

#define DATA_BITS_MIN   5
#define DATA_BITS_MAX   8

#define STOP_BITS_MIN   1
#define STOP_BITS_MAX   2


void configuration_load_default(configuration_t *configuration) {
    /* UART configuration */
    configuration->device = NULL;
    configuration->baudrate = 115200;
    configuration->data_bits = 8;
    configuration->stop_bits = 1;
    configuration->parity = PARITY_NONE;
    configuration->flow_control = FLOW_CONTROL_NONE;

    /* filter set */
    configuration->filter_return = false;
    configuration->filter_delete = false;
    configuration->filter_iboot = false;
    configuration->filter_lolcat = false;

    /* miscallenous */
    configuration->logging_disabled = false;
    configuration->env_config = false;
    configuration->help_needed = false;
    configuration->help_reason = NULL;
}

int parse_numeric_arg(const char *arg, int base, uint64_t *val, uint64_t min_val, uint64_t max_val) {
    char *stop;
    uint64_t result = strtoull(arg, &stop, base);
    if (*stop || result > max_val || result < min_val) {
        return -1;
    }
    
    *val = result;
    
    return 0;
}

int32_t find_baudrate_from_preset(const char *arg) {
    for (size_t i = 0; i < BAUDRATE_PRESET_COUNT; i++) {
        const struct baudrate_preset *curr = &baudrate_presets[i];
        if (strcmp(curr->name, arg) == 0) {
            return curr->baudrate;
        }
    }
    
    return -1;
}

int environment_tokenize(char *env, int *argc, char *argv[ENV_MAX_ARGS]) {
    int args_found = 0;
    char *pch = strtok(env, ENV_DELIMETER);
    
    do {
        args_found++;
        if (args_found > ENV_MAX_ARGS) {
            return -1;
        }

        argv[args_found - 1] = pch;

        pch = strtok(NULL, ENV_DELIMETER);
    } while (pch != NULL);

    *argc = args_found;

    return 0;
}

void configuration_load_internal(int argc, const char *argv[], configuration_t *configuration, bool from_env) {
    int _argc;
    const char **_argv;
    const char *help_reason = NULL;
    char env_copy[ENV_MAX_SIZE];
    int env_argc = 0;
    char *env_argv[ENV_MAX_ARGS];
    
    opterr = 0;

    configuration_load_default(configuration);

    if (!from_env) {
        _argc = argc;
        _argv = argv;
    } else {
        const char *env = getenv(ENV_CONFIG);
        if (!env) {
            help_reason = ENV_CONFIG " var wasn't found";
            goto help_needed;
        }

        size_t env_size = strlen(env);

        if (!env_size) {
            help_reason = ENV_CONFIG " var is empty";
            goto help_needed;
        }

        if (env_size > ENV_MAX_SIZE - 1) {
            help_reason = ENV_CONFIG " var is too big";
            goto help_needed;
        }

        strcpy(env_copy, env);

        if (environment_tokenize(env_copy, &env_argc, env_argv) != 0) {
            help_reason = ENV_CONFIG " var is invalid";
            goto help_needed;
        }

        optind = 0;
        optreset = 1;

        _argc = env_argc;
        _argv = (const char **)env_argv;
    }

    char c;
    while ((c = getopt(_argc, (char *const *)_argv, ARGUMENTS)) != -1) {
        switch (c) {
            case 'd': {
                if (!from_env) {
                    configuration->device = optarg;
                } else {
                    help_reason = "specifying device is not allowed when loading args from environment";
                    goto help_needed;
                }

                break;
            }

            case 'b': {
                int32_t baudrate_preset = find_baudrate_from_preset(optarg);

                if (baudrate_preset != -1) {
                    configuration->baudrate = baudrate_preset;
                } else {
                    uint64_t baudrate;
                    if (parse_numeric_arg(optarg, 10, &baudrate, BAUDRATE_MIN, BAUDRATE_MAX) != 0){
                        help_reason = "baudrate must be a decimal number not smaller than " STR(BAUDRATE_MIN) " and not bigger than " STR(BAUDRATE_MAX);
                        goto help_needed;
                    }

                    configuration->baudrate = (uint32_t)baudrate;
                }

                break;
            }

#if WITH_UART_EXTRA
            case 'c': {
                uint64_t data_bits;

                if (parse_numeric_arg(optarg, 10, &data_bits, DATA_BITS_MIN, DATA_BITS_MAX) != 0){
                    help_reason = "data bits must be a decimal number not smaller than " STR(DATA_BITS_MIN) " and not bigger than " STR(DATA_BITS_MAX);
                    goto help_needed;
                }

                configuration->data_bits = (uint32_t)data_bits;

                break;
            }

            case 's': {
                uint64_t stop_bits;

                if (parse_numeric_arg(optarg, 10, &stop_bits, STOP_BITS_MIN, STOP_BITS_MAX) != 0){
                    help_reason = "stop bits must be a decimal number not smaller than " STR(STOP_BITS_MIN) " and not bigger than " STR(STOP_BITS_MAX);
                    goto help_needed;
                }

                configuration->stop_bits = (uint32_t)stop_bits;

                break;
            }

            case 'p': {
                if (strcmp(optarg, "none") == 0) {
                    configuration->parity = PARITY_NONE;
                } else if (strcmp(optarg, "odd") == 0) {
                    configuration->parity = PARITY_ODD;
                } else if (strcmp(optarg, "even") == 0) {
                    configuration->parity = PARITY_EVEN;
                } else {
                    help_reason = "parity must be either none or odd or even";
                    goto help_needed;
                }

                break;
            }

            case 'f': {
                if (strcmp(optarg, "none") == 0) {
                    configuration->flow_control = FLOW_CONTROL_NONE;
                } else if (strcmp(optarg, "sw") == 0) {
                    configuration->flow_control = FLOW_CONTROL_SW;
                } else if (strcmp(optarg, "hw") == 0) {
                    configuration->flow_control = FLOW_CONTROL_HW;
                } else {
                    help_reason = "flow control must be either none or sw or hw";
                    goto help_needed;
                }

                break;
            }
#endif

            case 'n': {
                configuration->filter_return = true;
                break;
            }

            case 'k': {
                configuration->filter_delete = true;
                break;
            }

            case 'i': {
                configuration->filter_iboot = true;
                break;
            }

            case 'l': {
                configuration->filter_lolcat = true;
                break;
            }

            case 'g': {
                configuration->logging_disabled = true;
                break;
            }

            case 'e': {
                if (from_env) {
                    help_reason = "cannot have -e in environment configuration";
                    goto help_needed;
                }

                if (argc > 2) {
                    WARNING_PRINT("warning: environment configuration will override\n");
                }

                configuration_load_internal(argc, argv, configuration, true);

                configuration->env_config = true;

                return;
            }

            case 'h': {
                goto help_needed;
            }
            
            case '?': {
                help_reason = "invalid args";
                goto help_needed;
            }

            default:
                abort();
                break;
        }
    }

    return;

help_needed:
    configuration->help_needed = true;
    configuration->help_reason = help_reason;
}

void configuration_load(int argc, const char *argv[], configuration_t *configuration) {
    configuration_load_internal(argc, argv, configuration, false);
}

const char *bool_on_off(bool status) {
    return status ? "on" : "off";
}

void configuration_print(configuration_t *configuration) {
    BOLD_PRINT_NO_RETURN("device: ");
    if (configuration->device) {
        printf("%s", configuration->device);
    } else {
        printf("menu");
    }

    BOLD_PRINT_NO_RETURN(" baud: ");
    printf("%d", configuration->baudrate);

    BOLD_PRINT_NO_RETURN(" data: ");
    printf("%d", configuration->data_bits);

    BOLD_PRINT_NO_RETURN(" stop: ");
    printf("%d", configuration->stop_bits);

    BOLD_PRINT_NO_RETURN(" parity: ");
    switch (configuration->parity) {
        case PARITY_NONE:
            printf("none");
            break;

        case PARITY_EVEN:
            printf("even");
            break;

        case PARITY_ODD:
            printf("odd");
            break;
    }

    BOLD_PRINT_NO_RETURN(" flow: ");
    switch (configuration->flow_control) {
        case FLOW_CONTROL_NONE:
            printf("none");
            break;

        case FLOW_CONTROL_HW:
            printf("hw");
            break;

        case FLOW_CONTROL_SW:
            printf("sw");
            break;
    }

    printf("\n");

    BOLD_PRINT_NO_RETURN("return: ");
    printf("%s", bool_on_off(configuration->filter_return));

    BOLD_PRINT_NO_RETURN(" delete: ");
    printf("%s", bool_on_off(configuration->filter_delete));

    BOLD_PRINT_NO_RETURN(" iBoot: ");
    printf("%s", bool_on_off(configuration->filter_iboot));

    BOLD_PRINT_NO_RETURN(" \x1B[38;5;154ml\x1B[39m\x1B[38;5;214mo\x1B[39m\x1B[38;5;198ml\x1B[39m\x1B[38;5;164mc\x1B[39m\x1B[38;5;63ma\x1B[39m\x1B[38;5;39mt\x1B[39m\x1B[38;5;49m\x1B[39m: ");
    printf("%s", bool_on_off(configuration->filter_lolcat));

    BOLD_PRINT_NO_RETURN(" logging: ");
    printf("%s", bool_on_off(!configuration->logging_disabled));

    BOLD_PRINT_NO_RETURN(" env: ");
    printf("%s", bool_on_off(configuration->env_config));

    printf("\n\n");
}
