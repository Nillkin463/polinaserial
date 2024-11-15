#include <device.h>
#include <utils.h>

#include <IOKit/serial/ioss.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

int device_open_with_callout(const char *callout) {
    int fd = open(callout, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
    if (fd < 0) {
        ERROR_PRINT("device %s is unavailable", callout);
        return -1;
    }

    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        ERROR_PRINT("device %s is locked", callout);
        return -1;
    }

    return fd;
}

int device_get_attributes(int fd, struct termios *attributes) {
    if (tcgetattr(fd, attributes) != 0) {
        ERROR_PRINT("failed to get termios attributes - %s", strerror(errno));
        return -1;
    }

    return 0;
}

int device_set_attributes(int fd, struct termios *attributes) {
    if (tcsetattr(fd, TCSANOW, attributes) != 0) {
        ERROR_PRINT("failed to set termios attributes - %s", strerror(errno));
        return -1;
    }

    return 0;
}

int device_set_speed(int fd, speed_t speed) {
    if (ioctl(fd, IOSSIOSPEED, &speed) == -1) {
        ERROR_PRINT("failed to set speed - %s", strerror(errno));
        return -1;
    }

    return 0;
}

void attributes_set_from_configuration(
    struct termios *attributes,
    configuration_t *configuration,
    bool is_terminal
) {
    //cfsetospeed(attributes, configuration->baudrate);
    //cfsetispeed(attributes, configuration->baudrate);

    attributes->c_cflag |= (CLOCAL | CREAD);
    attributes->c_iflag |= (IGNPAR);
    attributes->c_iflag &= ~(INLCR | IGNCR | ICRNL);
    attributes->c_oflag &= ~OPOST;
    
    /* data bits */
    attributes->c_cflag &= ~CSIZE;
    switch (configuration->data_bits) {
        case 8:
            attributes->c_cflag |= CS8;
            break;
        
        case 7:
            attributes->c_cflag |= CS7;
            break;

        case 6:
            attributes->c_cflag |= CS6;
            break;
        
        case 5:
            attributes->c_cflag |= CS5;
            break;
    }

    /* stop bits */
    if (configuration->stop_bits == 2) {
        attributes->c_cflag |= CSTOPB;
    } else {
        attributes->c_cflag &= ~CSTOPB;
    }

    /* parity */
    switch (configuration->parity) {
        case PARITY_NONE:
            attributes->c_cflag &= ~PARENB;
            break;

        case PARITY_EVEN:
            attributes->c_cflag |= PARENB;
            attributes->c_cflag &= ~PARODD;
            break;

        case PARITY_ODD:
            attributes->c_cflag |= (PARENB | PARODD);
            break;
    }

    /* flow control */
    switch (configuration->flow_control) {
        case FLOW_CONTROL_NONE:
            attributes->c_iflag &= ~(IXON | IXOFF);
            break;

        case FLOW_CONTROL_HW:
            attributes->c_cflag |= (CCTS_OFLOW | CRTS_IFLOW);
            //missing break is intentional

        case FLOW_CONTROL_SW:
            attributes->c_iflag |= (IXON | IXOFF);
            break;
    }

    /* return filter */
    if (configuration->filter_return) {
        attributes->c_oflag = OPOST;
        attributes->c_oflag |= ONLCR;
    }
    
    attributes->c_iflag &= ~INPCK;
    attributes->c_iflag |= INPCK;

    attributes->c_cc[VTIME] = is_terminal ? 1 : 5;
    attributes->c_cc[VMIN] = 0;

    attributes->c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
}
