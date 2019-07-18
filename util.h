
#ifndef __UTIL_H
#define __UTIL_H

#define SLEEP_TIME 100

//extern void get_options (int argc, char **argv);
//extern void show_options (void);

//extern int rc_file_exist (void);

//extern int get_rc_file (void);

void end_it_all (Display * d);

extern int rc_file_exist (char *rc_guile_file);


extern Display *start (char *display);


extern void adjust_display (XAnyEvent * xany);


extern int null_X_error (Display * d, XErrorEvent * e);


extern void reload_rc_file (Display *d, char *rc_guile_file, int verbose);


extern void catch_HUP_signal (int sig);

extern void catch_CHLD_signal (int sig);

extern void start_as_daemon (void);

struct passed_data {
  Display *d;
  int poll_rc;
  int have_to_show_binding;
  int verbose;
  char *rc_guile_file;
};

#endif /* __UTIL_H */
