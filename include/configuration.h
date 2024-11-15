#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    PARITY_NONE,
    PARITY_EVEN,
    PARITY_ODD
} parity_t;

typedef enum {
    FLOW_CONTROL_NONE,
    FLOW_CONTROL_HW,
    FLOW_CONTROL_SW
} flow_control_t;

typedef struct {
    /* UART configuration */
    const char *device;
    uint32_t baudrate;
    uint8_t data_bits;
    uint8_t stop_bits;
    parity_t parity;
    flow_control_t flow_control;

    /* filter set */
    bool filter_return;
    bool filter_delete;
    bool filter_iboot;
    bool filter_lolcat;

    /* miscallenous */
    bool logging_disabled;
    bool env_config;
    bool help_needed;
    const char *help_reason;
} configuration_t;

void configuration_load(int argc, const char *argv[], configuration_t *configuration);
void configuration_print(configuration_t *configuration);

#endif
