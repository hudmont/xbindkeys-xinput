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

//#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
//#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <fcntl.h>
#include "xbindkeys.h"
#include "keys.h"
#include "options.h"
#include "get_key.h"
#include "grab_key.h"


#include <libguile.h>

#include <X11/XKBlib.h>
#include <popt.h>
#include "util.h"

static void inner_main (void *, int, char **);
Display *current_display;  // The current display

//static char rc_guile_file[512];
//static Display *d;

//extern int poll_rc;



int
main (const int argc, const char** argv)
{
  
  //guile shouldn't steal our arguments! we already parse them!
  //so we put them in temporary variables.
  
  char c;
  char *home;

  char default_file[513];

  home = getenv ("HOME");

  strncpy (default_file, home, sizeof (default_file) - 21);
  strncat (default_file, "/.xbindkeysrc.scm", sizeof (default_file)-1);

   // Option definitions
  verbose = 0;
  char *rc_guile_file = NULL;
  int have_to_show_binding = 0;
  int have_to_get_binding = 0;
  int have_to_start_as_daemon = 1;
  int detectable_ar=0;
  int poll_rc;
  char *display_name=NULL;
  Display *d;
  
  struct poptOption optionsTable[] =
    {     {"version",  'V',  0, NULL, 'V',
	   "prints version and exit", NULL},
	  
          {"display",  'X', POPT_ARG_STRING, display_name,  0,
	   "Set X display to use", NULL},
	  
          {"file",     'f', POPT_ARG_STRING, &rc_guile_file, 0,
	   "Use an alternative rc file", NULL},
	  
	  {"geometry", 'g', POPT_ARG_STRING, &geom, 0,
	   "size and position of window open with -k or-m option", NULL},

	  {"verbose",  'v',  0, &verbose, 'v',
	   "be verbose for debugging purposes", NULL},
	  
          {"poll_rc",  'p',  0, &poll_rc, 0,
	   "Poll the config for updates", NULL},
	  
	  {"show",     's',  0, &have_to_show_binding, 0,
	   "Show the actual keybindings", NULL},
	  
	  {"key",      'k',  0, &have_to_get_binding , 0,
	   "Identify one keypress", NULL},

	  {"multikey", 'm',   0 , NULL , 'm',
	   "Identify multiple keypresses", NULL},
	  
	  {"nodaemon", 'n',  0, NULL, 'n',
	   "don't start as daemon", NULL},
	  
	  {"detectable-ar", 'a', 0, &detectable_ar, 'a', 
	   "something something autorepeat", NULL},
	  
	  POPT_AUTOHELP
          {NULL, 0, 0,NULL, 0, NULL, NULL} };

  poptContext optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
  poptSetOtherOptionHelp(optCon, "[OPTIONS]* <port>");
    
  while((c = poptGetNextOpt(optCon))>= 0)
    {
    switch(c)
      {
      case 'V':
	show_version ();
	exit (1);
	break;
      case 'v':
      case 'n':
	have_to_start_as_daemon = 0;
        break;
      case 'm':
	have_to_get_binding=2;
	break;
      }
  }
  // Just to make sure it's 0-terminated
  //rc_guile_file[512]='\0';
  
  if (rc_guile_file == NULL)
    {
      rc_guile_file = default_file;
    }
  
  if (!rc_file_exist (rc_guile_file))
    exit (-1);

  if (have_to_start_as_daemon && !have_to_show_binding && !have_to_get_binding)
    start_as_daemon ();

  if (!display_name)
    display_name = XDisplayName (NULL);

  show_options (display_name, rc_guile_file);

  d = start (display_name);
  current_display = d;

  if (detectable_ar)
    {
      Bool supported_rtrn;
      XkbSetDetectableAutoRepeat(d, True, &supported_rtrn);

      if (!supported_rtrn)
	{
	  fprintf (stderr, "Could not set detectable autorepeat\n");
	}
    }

  get_offending_modifiers (d);

  if (have_to_get_binding)
    {
      get_key_binding (d, have_to_get_binding);
      end_it_all (d);
      exit (0);
    }
  struct passed_data to_pass = {.rc_guile_file=rc_guile_file,
				.have_to_show_binding=have_to_show_binding,
				.poll_rc = poll_rc,
				.d = d };
    
  //printf("Starting in guile mode...\n"); //debugery! (void *)
    scm_boot_guile(0,(char**)NULL,inner_main,(void *)(&to_pass));
  return 0; /* not reached ...*/
}

/*void handle(Display *d, XEvent *e)
{
	print_key (d, &keys[i]);
	adjust_display(&e.xany);
	start_command_key (&keys[i]);			
}
*/

void init_finalize(Display *d, char *rc_guile_file, int have_to_show_binding)
{
   // Config loading needs to be done in guilemode, therefore it's here
  if (get_rc_guile_file (rc_guile_file) != 0)
    {
	  exit (-1);
    }
  // this option requires the preloaded conf file
  if (have_to_show_binding)
    {
      show_key_binding (d);
      end_it_all (d);
      exit (0);
    }

  grab_keys (d);

  /* This: for restarting reading the RC file if get a HUP signal */
  // signal (SIGHUP, catch_HUP_signal);
  /* and for reaping dead children */
  signal (SIGCHLD, catch_CHLD_signal);
}

void
inner_main (void *passed_data, int argc, char **argv)
{
  struct passed_data *params = (struct passed_data *) passed_data;
  struct passed_data p = *params;

  init_finalize(p.d, p.rc_guile_file, p.have_to_show_binding);

  if (verbose)
    printf ("starting loop...\n");
  //event_loop (p.d, p.rc_guile_file, p.poll_rc);

  XEvent e;

  time_t rc_guile_file_changed = 0;
  struct stat rc_guile_file_info;

  XSetErrorHandler ((XErrorHandler) &null_X_error);

  if (p.poll_rc)
    {

      stat (p.rc_guile_file, &rc_guile_file_info);
      rc_guile_file_changed = rc_guile_file_info.st_mtime;

    }

  while (True)
    {
      while(p.poll_rc && !XPending(p.d))
	{

	  // if the rc guile file has been modified, reload it
	  stat (p.rc_guile_file, &rc_guile_file_info);


	  if (rc_guile_file_info.st_mtime != rc_guile_file_changed)
	    {
	      reload_rc_file (p.d, p.rc_guile_file);
	      if (verbose)
		{
		  printf ("The configuration file has been modified, reload it\n");
		}
	      rc_guile_file_changed = rc_guile_file_info.st_mtime;

	    }

	  usleep(SLEEP_TIME*1000);
	}

      XNextEvent (p.d, &e);

      switch (e.type)
	{
	case KeyPress:
	  if (verbose)
	    {
	      printf ("Key press !\n");
	      printf ("e.xkey.keycode=%d\n", e.xkey.keycode);
	      printf ("e.xkey.state=%d\n", e.xkey.state);
	    }

	  e.xkey.state &= ~(numlock_mask | capslock_mask | scrolllock_mask);

	  for (int i = 0; i < nb_keys; i++)
	    {
	      if (keys[i].type == SYM && keys[i].event_type == PRESS)
		{
		  if (e.xkey.keycode == XKeysymToKeycode (p.d, keys[i].key.sym)
		      && e.xkey.state == keys[i].modifier)
		    {
		      print_key (p.d, &keys[i]);
		      adjust_display(&e.xany);
		      start_command_key (&keys[i]);
		    }
		}
	      else
	      if (keys[i].type == CODE && keys[i].event_type == PRESS)
		{
		  if (e.xkey.keycode == keys[i].key.code
		      && e.xkey.state == keys[i].modifier)
		    {
		      print_key (p.d, &keys[i]);
		      adjust_display(&e.xany);
		      start_command_key (&keys[i]);
		    }
		}
	    }
	  break;

	case KeyRelease:
	  if (verbose)
	    {
	      printf ("Key release !\n");
	      printf ("e.xkey.keycode=%d\n", e.xkey.keycode);
	      printf ("e.xkey.state=%d\n", e.xkey.state);
	    }

	  e.xkey.state &= ~(numlock_mask | capslock_mask | scrolllock_mask);

	  for (int i = 0; i < nb_keys; i++)
	    {
	      if (keys[i].type == SYM && keys[i].event_type == RELEASE)
		{
		  if (e.xkey.keycode == XKeysymToKeycode (p.d, keys[i].key.sym)
		      && e.xkey.state == keys[i].modifier)
		    {
		      print_key (p.d, &keys[i]);
		      adjust_display(&e.xany);
		      start_command_key (&keys[i]);
		    }
		}
	      else
	      if (keys[i].type == CODE && keys[i].event_type == RELEASE)
		{
		  if (e.xkey.keycode == keys[i].key.code
		      && e.xkey.state == keys[i].modifier)
		    {
		      print_key (p.d, &keys[i]);
		      adjust_display(&e.xany);
		      start_command_key (&keys[i]);
		    }
		}
	    }
	  break;

	case ButtonPress:
	  if (verbose)
	    {
	      printf ("Button press !\n");
	      printf ("e.xbutton.button=%d\n", e.xbutton.button);
	      printf ("e.xbutton.state=%d\n", e.xbutton.state);
	    }

	  e.xbutton.state &= 0x1FFF & ~(numlock_mask | capslock_mask | scrolllock_mask
			       | Button1Mask | Button2Mask | Button3Mask
			       | Button4Mask | Button5Mask);

	  for (int i = 0; i < nb_keys; i++)
	    {
	      if (keys[i].type == BUTTON && keys[i].event_type == PRESS)
		{
		  if (e.xbutton.button == keys[i].key.button
		      && e.xbutton.state == keys[i].modifier)
		    {
                      //printf("Replay!!!\n");
                      //ungrab_all_keys(p.d);
                      //XPutBackEvent(p.d, &e);
                      //sleep(1);
                      //grab_keys(p.d);
		      print_key (p.d, &keys[i]);
		      adjust_display(&e.xany);
		      start_command_key (&keys[i]);
		    }
		}
	    }
	  break;

	case ButtonRelease:
	  if (verbose)
	    {
	      printf ("Button release !\n");
	      printf ("e.xbutton.button=%d\n", e.xbutton.button);
	      printf ("e.xbutton.state=%d\n", e.xbutton.state);
	    }

	  e.xbutton.state &= 0x1FFF & ~(numlock_mask | capslock_mask | scrolllock_mask
			       | Button1Mask | Button2Mask | Button3Mask
			       | Button4Mask | Button5Mask);

	  for (int i = 0; i < nb_keys; i++)
	    {
	      if (keys[i].type == BUTTON && keys[i].event_type == RELEASE)
		{
		  if (e.xbutton.button == keys[i].key.button
		      && e.xbutton.state == keys[i].modifier)
		    {
		      print_key (p.d, &keys[i]);
		      adjust_display(&e.xany);
		      start_command_key (&keys[i]);
		    }
		}
	    }
	  break;

	default:
	  break;
	}
    }				/*  infinite loop */
  if (verbose)
    printf ("ending...\n");
  end_it_all (p.d);
  
  #ifdef AVOID_KNOWN_HARMLESS_WARNINGS
    argc=argc; argv=argv;
  #else
    return (0);
  #endif
}

