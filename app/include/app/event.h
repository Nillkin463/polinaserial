#ifndef APP_EVENT_H
#define APP_EVENT_H

typedef enum {
    APP_EVENT_NONE = 0,
    APP_EVENT_DISCONNECT_USER,
    APP_EVENT_DISCONNECT_DEVICE,
    APP_EVENT_ERROR
} app_event_t;

int app_event_signal(app_event_t event);

#endif
