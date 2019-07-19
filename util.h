
#ifndef __UTIL_H
#define __UTIL_H

#define SLEEP_TIME 100

#include <X11/Xlib.h>

extern void end_it_all(Display *d);

extern int rc_file_exist(char *rc_guile_file);

extern Display *start(char *display);

extern void adjust_display(XAnyEvent *xany);

extern int null_x_error(Display *d, XErrorEvent *e);

extern void reload_rc_file(Display *d, char *rc_guile_file, int verbose);

extern void catch_hup_signal(int sig);

extern void catch_chld_signal(int sig);

extern void start_as_daemon(void);

struct passed_data {
  Display *d;
  int poll_rc;
  int have_to_show_binding;
  int verbose;
  char *rc_guile_file;
};

#endif /* __UTIL_H */
