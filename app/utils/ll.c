#include <app/ll.h>

void _ll_add_element(ll_t *head, ll_t element) {
    if (*head) {
        ll_t last = *head;

        ll_iterate(*head, ll_t, curr, {
            last = curr;
        });

        last->next = element;
    } else {
        *head = element;
    }
}

void _ll_destroy(ll_t *head, ll_destroy_callback_t cb) {
    ll_t curr = *head;

    while (curr) {
        ll_t next = curr->next;

        if (cb) {
            cb(curr);
        }

        free(curr);
        
        curr = next;
    }

    *head = NULL;
}
