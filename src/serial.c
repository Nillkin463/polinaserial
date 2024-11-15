#include <serial.h>
#include <utils.h>

#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

int serial_loop(int device_fd, serial_callback_t in_callback, serial_callback_t out_callback) {
    struct kevent ke;
    
    int kq = kqueue();
    
    if (kq == -1) {
        ERROR_PRINT("kqueue() failed");
        return -1;
    }
    
    char buf[BUFFER_SIZE];
    
    EV_SET(&ke, device_fd, EVFILT_READ, EV_ADD, 0, 5, NULL);
    kevent(kq, &ke, 1, NULL, 0, NULL);

    EV_SET(&ke, device_fd, EVFILT_VNODE, EV_ADD, NOTE_DELETE, 5, NULL);
    kevent(kq, &ke, 1, NULL, 0, NULL);

    EV_SET(&ke, STDIN_FILENO, EVFILT_READ, EV_ADD, 0, 5, NULL);
    kevent(kq, &ke, 1, NULL, 0, NULL);
    
    BOLD_PRINT("[connected - use CTRL+] to disconnect]");
    
    while (true) {         
        memset(&ke, 0, sizeof(ke));
        int i = kevent(kq, NULL, 0, &ke, 1, NULL);
        
        if (i == 0) {
            continue;
        }
        
        if (ke.ident == device_fd) {
            if (ke.filter == EVFILT_VNODE && ke.fflags & NOTE_DELETE) {
                BOLD_PRINT("\r\n[disconnected - device disappeared]");
                return 0;

            } else {
                ssize_t r = read(device_fd, buf, sizeof(buf));
            
                if (r > 0) {
                    if (out_callback(STDOUT_FILENO, buf, r) != 0) {
                        return -1;
                    }
                }
            }

        } else {
            if (ke.ident == STDIN_FILENO) {
                ssize_t r = read(STDIN_FILENO, buf, 1);
                
                if (buf[0] == 0x1D) {
                    BOLD_PRINT("\r\n[disconnected]");
                    return 0;    
                }
                
                if (in_callback(device_fd, buf, r) != 0) {
                    return -1;
                } 
            }
        }
    }
}
