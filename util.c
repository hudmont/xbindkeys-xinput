/***************************************************************************
        xbindkeys : a program to bind keys to commands under X11.
                           -------------------
    begin                : Sat Oct 13 14:11:34 CEST 2001
    copyright            : (C) 2001 by Philippe Brochard
    email                : hocwp@free.fr
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "xbindkeys.h"
#include "keys.h"
#include "options.h"
#include "get_key.h"
#include "grab_key.h"
#include <X11/XKBlib.h>


#include "util.h"

extern void
show_version (void)
{
  fprintf (stderr, "xbindkeys %s by Philippe Brochard & @Hudmont\n", PACKAGE_VERSION);
}

extern void
show_options (char *display_name, char *rc_guile_file)
{
  if (verbose)
    {
      printf ("displayName = %s\n", display_name);

      printf ("rc guile file = %s\n", rc_guile_file);

    }
}

extern int
rc_file_exist (char *rc_guile_file)
{
  FILE * stream;

  if ((stream = fopen (rc_guile_file, "r")) == NULL) {
    
    fprintf (stderr, "Error : %s not found or reading not allowed.\n",
	     rc_guile_file);
    fprintf (stderr,
	     "please, create one with 'xbindkeys --defaults-guile > %s'.\n",
	     rc_guile_file);
    return 0;
      
  } else {
    fclose (stream);
    return 1;
  }
}



void
end_it_all (Display * d)
{
  ungrab_all_keys (d);

  close_keys ();
  XCloseDisplay (d);
}

extern Display *
start (char *display)
{
  Display *d;
  int screen;

  d = XOpenDisplay (display);
  if (!d)
    {
      fprintf (stderr,
	       "Could not open display, check shell DISPLAY variable, \
and export or setenv it!\n");
      exit (1);
    }


  XAllowEvents (d, AsyncBoth, CurrentTime);

  for (screen = 0; screen < ScreenCount (d); screen++)
    {
      XSelectInput (d, RootWindow (d, screen),
		    KeyPressMask | KeyReleaseMask | PointerMotionMask);
    }

  return (d);
}


extern void
adjust_display (XAnyEvent * xany)
{
  size_t envstr_size = strlen(DisplayString(xany->display)) + 8 + 1;
  char* envstr = malloc ( (envstr_size + 2) * sizeof (char) );
  XWindowAttributes attr;
  char* ptr;
  char buf[16];

  snprintf (envstr, envstr_size, "DISPLAY=%s", DisplayString(xany->display));

  XGetWindowAttributes (xany->display, xany->window,  &attr);

  if (verbose)
    printf ("got screen %d for window %x\n", XScreenNumberOfScreen(attr.screen), (unsigned int)xany->window );

  ptr = strchr (strchr (envstr, ':'), '.');
  if (ptr)
    *ptr = '\0';

  snprintf (buf, sizeof(buf), ".%i", XScreenNumberOfScreen(attr.screen));
  strncat (envstr, buf, 16);

  putenv (envstr);
}

extern int
null_X_error (Display * d, XErrorEvent * e)
{
  static int already = 0;

  /* The warning is displayed only once */
  if (already != 0)
    return 0;
  already = 1;

  printf ("\n*** Warning ***\n");
  printf ("Please verify that there is not another program running\n");
  printf ("which captures one of the keys captured by xbindkeys.\n");
  printf ("It seems that there is a conflict, and xbindkeys can't\n");
  printf ("grab all the keys defined in its configuration file.\n");

/*   end_it_all (d); */

/*   exit (-1); */
  #ifdef AVOID_KNOWN_HARMLESS_WARNINGS
  d=d; e=e;
  #endif
  return 0;
  //(NULL);
}



extern void
reload_rc_file (char *rc_guile_file)
{
  int min, max;
  int screen;

  XDisplayKeycodes (current_display, &min, &max);

  if (verbose)
    printf ("Reload RC file\n");

  for (screen = 0; screen < ScreenCount (current_display); screen++)
    {
      XUngrabKey (current_display, AnyKey, AnyModifier, RootWindow (current_display, screen));
    }
  close_keys ();


  if (get_rc_guile_file (rc_guile_file) != 0)
    {

      end_it_all (current_display);
      exit (-1);


    }


  grab_keys (current_display);
}

/*
extern void
catch_HUP_signal (int sig, char *rc_guile_file)
{
  reload_rc_file (rc_guile_file);
  #ifdef AVOID_KNOWN_HARMLESS_WARNINGS
  //sig=sig; // to avoid warnings
#endif
}
*/

extern void
catch_CHLD_signal (int sig)
{
  pid_t child;

  /*   If more than one child exits at approximately the same time, the signals */
  /*   may get merged. Handle this case correctly. */
  while ((child = waitpid(-1, NULL, WNOHANG)) > 0)
    {
      if (verbose)
	printf ("Catch CHLD signal -> pid %i terminated\n", child);
    }
  #ifdef AVOID_KNOWN_HARMLESS_WARNINGS
  sig=sig;
  #endif
}

extern void
start_as_daemon (void)
{
  pid_t pid;
  int i;

  pid = fork ();
  if (pid < 0)
    {
      perror ("Could not fork");
    }
  if (pid > 0)
    {
      exit (EXIT_SUCCESS);
    }

  setsid ();

  pid = fork ();
  if (pid < 0)
    {
      perror ("Could not fork");
    }
  if (pid > 0)
    {
      exit (EXIT_SUCCESS);
    }

  for (i = getdtablesize (); i >= 0; i--)
    {
      close (i);
    }

  i = open ("/dev/null", O_RDWR);
  dup (i);
  dup (i);

/*   umask (022); */
/*   chdir ("/tmp"); */
}

#define EQ_button (e.xbutton.button == keys[i].key.button)
#define EQ_key (e.xkey.keycode == XKeysymToKeycode (d, keys[i].key.sym)

#define PLACEHOLDER_NAME(TYPE, CAPITALIZED_TYPE_NAME, EVENT_TYPE)		\
if (keys[i].type == CAPITALIZED_TYPE && keys[i].event_type == EVENT_TYPE)	\
		{								\
		  if (EQ_##TYPE							\
		      && e.x##TYPE.state == keys[i].modifier)			\
		    {								\
		     handle(d, &e);						\
		    } 								\
		}

#define keyMask ~(numlock_mask | capslock_mask | scrolllock_mask)
#define buttonMask 0x1FFF & ~(numlock_mask | capslock_mask | scrolllock_mask	\
			       | Button1Mask | Button2Mask | Button3Mask	\
			       | Button4Mask | Button5Mask)
			       
#define inner_loop_button PLACEHOLDER_NAME(TYPE, BUTTON, EVENT_TYPE)
#define inner_loop_key(TYPE, CAPITALIZED_TYPE, EVENT_TYPE)			\
	      PLACEHOLDER_NAME(TYPE, SYM, EVENT_TYPE)				\
	      else								\
	      PLACEHOLDER_NAME(TYPE, CODE, EVENT_TYPE)

#define buttonPrint printf("e.xbutton.button=%d\n", e.xbutton.button)
#define keyPrint printf ("e.xkey.keycode=%d\n", e.xkey.keycode)

#define EVENT_HANDLE(TYPE, EVENT_TYPE)						\
 if (verbose)									\
	    {									\
	      printf (#TYPE #EVENT_TYPE "!\n");					\
	      TYPE##Print;							\
	      printf ("e.x" #TYPE ".state=%d\n", e.x##TYPE.state);		\
	    } 	     			      					\
										\
	  e.x##TYPE.state &= TYPE##Mask;					\
			       	 	       					\
	  for (i = 0; i < nb_keys; i++)						\
	    { 	      	  	   						\
	      inner_loop_##TYPE(TYPE, CAPITALIZED_TYPE, EVENT_TYPE);		\
	    }
