#ifndef APP_LL_H
#define APP_LL_H

#include <stdlib.h>

struct ll {
    struct ll *next;
};

typedef struct ll * ll_t;

typedef void (*ll_destroy_callback_t)(ll_t element);

void _ll_add_element(ll_t *head, ll_t element);
void _ll_destroy(ll_t *head, ll_destroy_callback_t callback);

#define ll_add_element(head, element) _ll_add_element((ll_t *)head, (ll_t)element)
#define ll_destroy(head, callback) _ll_destroy((ll_t *)head, (ll_destroy_callback_t)callback)

#define ll_iterate(head, type, name, code) \
    do { \
        for (type name = head; name; name = (type)((ll_t)name)->next) \
            code \
    } while (0);

#endif
