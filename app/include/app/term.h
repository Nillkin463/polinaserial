#ifndef APP_TERM_H
#define APP_TERM_H

int app_term_save_attrs();
int app_term_restore_attrs();

int  app_term_scroll();
void app_term_clear_page();
void app_term_clear_line();

void app_term_hide_cursor();
void app_term_show_cursor();

#endif
