#ifndef SERIAL_CONFIG_H
#define SERIAL_CONFIG_H

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
    const char *device;
    uint32_t baudrate;
    uint8_t data_bits;
    uint8_t stop_bits;
    parity_t parity;
    flow_control_t flow_control;
} serial_config_t;

int  serial_config_load(int argc, const char *argv[], serial_config_t *config);
void serial_config_print(serial_config_t *config);
void serial_help();

#endif
