#include <string.h>
#include <errno.h>

#include <app/tty.h>
#include <app/misc.h>

int tty_get_attrs(int fd, struct termios *attrs) {
    if (tcgetattr(fd, attrs) != 0) {
        ERROR("couldn't get termios attributes from fd %d - %s", fd, strerror(errno));
        return -1;
    }

    return 0;
}

int tty_set_attrs(int fd, struct termios *attrs) {
    if (tcsetattr(fd, TCSANOW, attrs) != 0) {
        ERROR("couldn't set termios attributes to fd %d - %s", fd, strerror(errno));
        return -1;
    }

    return 0;
}
