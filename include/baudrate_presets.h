#ifndef BAUDRATE_PRESETS_H
#define BAUDRATE_PRESETS_H

#include <stdint.h>


struct baudrate_preset {
    const char *name;
    uint32_t baudrate;
    const char *description;
};

static const struct baudrate_preset baudrate_presets[] = {
    {"ios", 115200, "iOS bootloader/kernel/diags"},
    {"airpods", 230400, "AirPods 1/2 case"},
    {"airpodspro", 921600, "AirPods Pro case"},
    {"airpower", 1250000, "AirPower"},
    {"kanzi", 1500000, "Kanzi debug"},
};

#define BAUDRATE_PRESET_COUNT ((sizeof(baudrate_presets)) / (sizeof(struct baudrate_preset)))

#endif
