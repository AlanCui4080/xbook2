#ifndef __TERMINAL_WINDOW_H__
#define __TERMINAL_WINDOW_H__

#include <sgi/sgi.h>
#include <stddef.h>

int con_open_window();
int con_close_window();
int con_event_loop(char *buf, int count);
int con_event_poll(char *buf, int pid);

#endif  /* __TERMINAL_WINDOW_H__ */