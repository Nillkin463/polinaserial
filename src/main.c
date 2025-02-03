//
//  main.m
//  polinaserial
//
//  Created by noone on 11/5/21.
//

#include <stdio.h>
#include <assert.h>
#include <dlfcn.h>
#include <mach-o/getsect.h>

#include <configuration.h>
#include <baudrate_presets.h>
#include <menu.h>
#include <device.h>
#include <serial.h>
#include <log.h>
#include <lolcat.h>
#include <iboot.h>
#include <string.h>
#include <utils.h>

void version() {
    unsigned long size;
    uint8_t *data = getsectiondata(dlsym(RTLD_MAIN_ONLY, "_mh_execute_header"), "__TEXT", "__build_tag", &size);
    if (data) {
        char tag[size + 1];
        memcpy(tag, data, size);
        tag[size] = '\0';
        BOLD_PRINT("%s", tag);
    } else {
        WARNING_PRINT("failed to get embedded build tag");
        BOLD_PRINT(PRODUCT_NAME "-unknown");
    }


    BOLD_PRINT("made by john (@nyan_satan)");
    printf("\n");
}

void help(const char *program_name, const char *reason) {
    if (reason) {
        WARNING_PRINT("%s", reason);
        printf("\n");
    }

    printf("usage: %s <options>\n", program_name);
    printf("available UART options:\n");
    printf("\t-d <device>\tpath to device (default - shows menu)\n");
    printf("\t-b <baudrate>\tbaudrate to use (default - ios/115200)\n");
    printf("\n");
    printf("\tavailable baudrate presets:\n");

    for (int i = 0; i < BAUDRATE_PRESET_COUNT; i++) {
        const struct baudrate_preset *curr = &baudrate_presets[i];
        printf("\t\t%-12.12s- %s (%d)\n", curr->name, curr->description, curr->baudrate);
    }

    printf("\n");

#if WITH_UART_EXTRA
    printf("\t-c <bits>\tdata bits - from 5 to 8 (default - 8)\n");
    printf("\t-s <bits>\tstop bits - 1 or 2 (default - 1)\n");
    printf("\t-p <parity>\tparity - none, even or odd (default - none)\n");
    printf("\t-f <control>\tflow control - none, sw or hw (default - none)\n");
#else
    printf("\tthe rest will default to 8N1 (no flow control),\n");
    printf("\tas the author of this tool cannot currently test other configurations\n");
#endif

    printf("\n");
    
    printf("available filter options:\n");
    printf("\t-n\tadd \\r to lines without it, good for diags/probe debug logs/etc.\n");
    printf("\t-k\treplace delete keys (0x7F) with backspace (0x08), good for diags\n");
    printf("\t-i\ttry to identify filenames in obfuscated iBoot output\n");
    printf("\n");
    
    printf("available miscallenous options:\n");
    printf("\t-l\tlolcat the output, good for screenshots\n");
    printf("\t-g\tdisable logging\n");
    printf("\t-e\tload ARGV from POLINASERIAL env var\n");
    printf("\t-h\tshow this help menu\n");
    printf("\n");
    
    printf("logs are collected to ~/Library/Logs/" PRODUCT_NAME "/\n");
}

configuration_t configuration;

int serial_input_callback(int target_fd, char *buffer, size_t length) {
    assert(length == 1);

    if (configuration.filter_delete && *buffer == 0x7F) {
        *buffer = 0x08;
    }

    write(target_fd, buffer, length);

    return 0;
}

int serial_output_callback(int target_fd, char *buffer, size_t length) {
    if (configuration.filter_lolcat || configuration.filter_iboot) {
        for (size_t i = 0; i < length; i++) {
            if (configuration.filter_iboot) {
                iboot_push_char(target_fd, buffer[i]);
            }

            if (configuration.filter_lolcat) {
                lolcat_print(target_fd, buffer[i]);
            } else {
                write(target_fd, buffer + i, 1);
            }
        }

    } else {
        write(target_fd, buffer, length);
    }

    if (!configuration.logging_disabled) {
        if (log_push(buffer, length) != 0) {
            return -1;
        }
    }

    return 0;
}

int main(int argc, const char *argv[]) {    
    menu_serial_item_t *picked = NULL;
    int device_fd = -1;
    struct termios device_original, device_new;
    struct termios terminal_original, terminal_new;
    bool device_termios_inited = false, terminal_termios_inited = false;
    int ret = -1;

    /* print version */
    version();

    /* load configuration from args */
    configuration_load(argc, argv, &configuration);

    /* invalid configuration? Print about it */
    if (configuration.help_needed) {
        help(argv[0], configuration.help_reason);
        goto out;
    }

    /* print final configuration */
    configuration_print(&configuration);

    /* initialize menu - list of all serial ports in the system */
    if (menu_find_devices() != 0) {
        ERROR_PRINT("failed to find serial devices");
        goto out;
    }

    /*
     * if menu is requested - show it up,
     * if not - check device list for
     * the requested device
     */

    picked = NULL;

    if (!configuration.device) {
        picked = menu_pick();
    } else {
        picked = menu_find_by_callout(configuration.device);

        if (!picked) {
            ERROR_PRINT("couldn't find such device");
            goto out;
        }
    }


    /* open requested device */
    const char *callout = menu_get_callout_from_item(picked);

    device_fd = device_open_with_callout(callout);

    if (device_fd < 0) {
        goto out;
    }

    /* if logging is enabled - init it now */
    if (!configuration.logging_disabled) {
        if (log_init(picked) != 0) {
            ERROR_PRINT("failed to start logging");
            goto out;
        }
    }

    /* get termios struct for both device and terminal */
    if (device_get_attributes(device_fd, &device_original) != 0) {
        ERROR_PRINT("failed to get termios attributes for device");
        goto out;
    }

    memcpy(&device_new, &device_original, sizeof(struct termios));

    if (device_get_attributes(STDOUT_FILENO, &terminal_original) != 0) {
        ERROR_PRINT("failed to get termios attributes for terminal");
        goto out;
    }

    memcpy(&terminal_new, &terminal_original, sizeof(struct termios));

    /* modify termios structs according to configuration */
    attributes_set_from_configuration(&device_new, &configuration, false);
    attributes_set_from_configuration(&terminal_new, &configuration, true);


    /* set it */
    if (device_set_attributes(device_fd, &device_new) != 0) {
        ERROR_PRINT("failed to set termios attributes for device");
        goto out;
    }

    if (device_set_speed(device_fd, configuration.baudrate) != 0) {
        ERROR_PRINT("failed to set speed for device");
        goto out;
    }

    device_termios_inited = true;

    if (device_set_attributes(STDOUT_FILENO, &terminal_new) != 0) {
        ERROR_PRINT("failed to set termios attributes for terminal");
        goto out;
    }

    terminal_termios_inited = true;

    if (configuration.filter_lolcat) {
        lolcat_init();
    }

    /* serial loop! */
    ret = serial_loop(device_fd, serial_input_callback, serial_output_callback);

    /*
     * cleanup - destroy menu, close device,
     * restore original termios structs if needed
     */

out:
    if (terminal_termios_inited) {
        if (device_set_attributes(STDOUT_FILENO, &terminal_original) != 0) {
            ERROR_PRINT("failed to set original termios attributes for terminal - you might want to restart it");
            ret = -1;
        }
    }

    if (device_fd != -1 && access(callout, F_OK) == 0) {
        if (device_termios_inited) {
            if (device_set_attributes(device_fd, &device_original) != 0) {
                ERROR_PRINT("failed to set original termios attributes for device");
                ret = -1;
            }
        }

        close(device_fd);
    }

    if (!configuration.logging_disabled) {
        log_queisce();
    }

    menu_destroy();
    
    return ret;
}
