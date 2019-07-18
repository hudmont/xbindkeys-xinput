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
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "xbindkeys.h"
#include "keys.h"
#include "options.h"
#include "get_key.h"
#include "grab_key.h"


#include <libguile.h>

#include <X11/XKBlib.h>
#include <getopt.h>

void end_it_all (Display * d);

static Display *start (char *);
static void event_loop (Display *);
static int *null_X_error (Display *, XErrorEvent *);
static void reload_rc_file (void);
static void catch_HUP_signal (int sig);
static void catch_CHLD_signal (int sig);
static void start_as_daemon (void);


Display *current_display;  // The current display

static char rc_guile_file[512];

extern int poll_rc;

#define SLEEP_TIME 100

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

static void
show_version (void)
{
  fprintf (stderr, "xbindkeys %s by Philippe Brochard & @Hudmont\n", PACKAGE_VERSION);
}

void
show_options (char *display_name, char *rc_guile_file)
{
  if (verbose)
    {
      printf ("displayName = %s\n", display_name);

      printf ("rc guile file = %s\n", rc_guile_file);

    }
}


static void
show_help (void)
{
  show_version ();

  fprintf (stderr, "usage: xbindkeys [options]\n");
  fprintf (stderr, "  where options are:\n");

  fprintf (stderr, "  -V, --version           Print version and exit\n");

  fprintf (stderr, " -d, --print-defaults    Print a default guile configuration file\n");

  fprintf (stderr, "  -f, --file              Use an alternative rc file\n");

  fprintf (stderr, "  -p, --poll-rc           Poll the rc/guile configs for updates\n");
  fprintf (stderr, "  -h, --help              This help!\n");
  fprintf (stderr, "  -X, --display           Set X display to use\n");
  fprintf (stderr,
	   "  -v, --verbose           More information on xbindkeys when it run\n");
  fprintf (stderr, "  -s, --show              Show the actual keybinding\n");
  fprintf (stderr, "  -k, --key               Identify one key pressed\n");
  fprintf (stderr, " -m, --multikey          Identify multi key pressed\n");
  fprintf (stderr,
	   "  -g, --geometry          size and position of window open with -k|-mk option\n");
  fprintf (stderr, "  -n, --nodaemon          don't start as daemon\n");
}

int
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

int argc_t; char** argv_t;
void
inner_main (int argc, char **argv)
{
  Display *d;


  argc = argc_t;
  argv = argv_t;

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
      get_key_binding (d, argv, argc);
      end_it_all (d);
      exit (0);
    }


  if (get_rc_guile_file (rc_guile_file) != 0)
    {

	  exit (-1);

    }



  if (have_to_show_binding)
    {
      show_key_binding (d);
      end_it_all (d);
      exit (0);
    }

  grab_keys (d);

  /* This: for restarting reading the RC file if get a HUP signal */
  signal (SIGHUP, catch_HUP_signal);
  /* and for reaping dead children */
  signal (SIGCHLD, catch_CHLD_signal);

  if (verbose)
    printf ("starting loop...\n");
  event_loop (d);

  if (verbose)
    printf ("ending...\n");
  end_it_all (d);


  // return (0); // to avoid warnings

}


int
main (int argc, char** argv)
{
  //guile shouldn't steal our arguments! we already parse them!
  //so we put them in temporary variables.
  argv_t = argv;
  argc_t = argc;
  
  char c;
  char *home;

  strncpy (rc_guile_file, "", sizeof (rc_guile_file));

  verbose = 0;
  have_to_show_binding = 0;
  have_to_get_binding = 0;
  have_to_start_as_daemon = 1;
  
  static struct option long_options[] =
        { {"version",        no_argument, 0, 'V'},
	  {"help",           no_argument, 0, 'h'},
          {"display",  required_argument, 0, 'X'},
          {"file",     required_argument, 0, 'f'},
	  {"geometry",     required_argument, 0, 'g'},

	  {"verbose",        no_argument, &verbose, 1},
          {"poll_rc",        no_argument, &poll_rc, 1},
	  {"show",           no_argument, &have_to_show_binding, 1},
	  {"key",            no_argument, &have_to_get_binding , 1},
	  {"multikey",       no_argument, &have_to_get_binding , 2},
	  {"nodaemon",       no_argument, &have_to_start_as_daemon, 0},
	  {"detectable-ar",  no_argument, &detectable_ar, 1},
          {0, 0, 0, 0} };
  int option_index = 0;
  
  while((c = getopt_long(argc, argv,"VdhXfgvpskmnD" , long_options, &option_index)) != -1)
    {
    switch(c)
      {
      case 'V':
	show_version ();
	exit (1);
	break;
      case 'X':
	display_name = optarg;
	break;
      case 'f':
	strncpy (rc_guile_file, optarg, sizeof (rc_guile_file) - 1);
	break;
      case 'g':
	geom = optarg;
	break;
	// small Duff's device
      case 0:
	if(option_index == 6)
	  {
	  case 'v':
	    have_to_start_as_daemon = 0;
	  }
	break;
      case 'p':
	poll_rc=1;
	break;
      case 's':
	have_to_show_binding=1;
	break;
      case 'k':
	have_to_get_binding=1;
	break;
      case 'm':
	have_to_get_binding=2;
	break;
      case 'n':
	have_to_start_as_daemon=0;
	break;
      case 'D':
        detectable_ar=1;
	break;
      case 'h':
      default:
	show_help();
	exit(1);
      }
  }
  
  if (strcmp (rc_guile_file, "") == 0)
    {
      home = getenv ("HOME");

      if (rc_guile_file != NULL)
	{
	  strncpy (rc_guile_file, home, sizeof (rc_guile_file) - 20);
	  strncat (rc_guile_file, "/.xbindkeysrc.scm", sizeof (rc_guile_file));
	}
    }
  
  //printf("Starting in guile mode...\n"); //debugery!
  scm_boot_guile(0,(char**)NULL,(void *)inner_main,NULL);
  return 0; /* not reached ...*/
}


static Display *
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

void
end_it_all (Display * d)
{
  ungrab_all_keys (d);

  close_keys ();
  XCloseDisplay (d);
}

static void
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

/*void handle(Display *d, XEvent *e)
{
	print_key (d, &keys[i]);
	adjust_display(&e.xany);
	start_command_key (&keys[i]);			
}
*/


static void
event_loop (Display * d)
{
  XEvent e;
  int i;

  time_t rc_guile_file_changed = 0;
  struct stat rc_guile_file_info;

  XSetErrorHandler ((XErrorHandler) null_X_error);

  if (poll_rc)
    {

      stat (rc_guile_file, &rc_guile_file_info);
      rc_guile_file_changed = rc_guile_file_info.st_mtime;

    }

  while (True)
    {
      while(poll_rc && !XPending(d))
	{

	  // if the rc guile file has been modified, reload it
	  stat (rc_guile_file, &rc_guile_file_info);


	  if (rc_guile_file_info.st_mtime != rc_guile_file_changed)
	    {
	      reload_rc_file ();
	      if (verbose)
		{
		  printf ("The configuration file has been modified, reload it\n");
		}
	      rc_guile_file_changed = rc_guile_file_info.st_mtime;

	    }

	  usleep(SLEEP_TIME*1000);
	}

      XNextEvent (d, &e);

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

	  for (i = 0; i < nb_keys; i++)
	    {
	      if (keys[i].type == SYM && keys[i].event_type == PRESS)
		{
		  if (e.xkey.keycode == XKeysymToKeycode (d, keys[i].key.sym)
		      && e.xkey.state == keys[i].modifier)
		    {
		      print_key (d, &keys[i]);
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
		      print_key (d, &keys[i]);
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

	  for (i = 0; i < nb_keys; i++)
	    {
	      if (keys[i].type == SYM && keys[i].event_type == RELEASE)
		{
		  if (e.xkey.keycode == XKeysymToKeycode (d, keys[i].key.sym)
		      && e.xkey.state == keys[i].modifier)
		    {
		      print_key (d, &keys[i]);
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
		      print_key (d, &keys[i]);
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

	  for (i = 0; i < nb_keys; i++)
	    {
	      if (keys[i].type == BUTTON && keys[i].event_type == PRESS)
		{
		  if (e.xbutton.button == keys[i].key.button
		      && e.xbutton.state == keys[i].modifier)
		    {
                      //printf("Replay!!!\n");
                      //ungrab_all_keys(d);
                      //XPutBackEvent(d, &e);
                      //sleep(1);
                      //grab_keys(d);
		      print_key (d, &keys[i]);
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

	  for (i = 0; i < nb_keys; i++)
	    {
	      if (keys[i].type == BUTTON && keys[i].event_type == RELEASE)
		{
		  if (e.xbutton.button == keys[i].key.button
		      && e.xbutton.state == keys[i].modifier)
		    {
		      print_key (d, &keys[i]);
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
}



static int *
null_X_error (Display * d, XErrorEvent * e)
{
  static int already = 0;

  /* The warning is displayed only once */
  if (already != 0)
    return (NULL);
  already = 1;

  printf ("\n*** Warning ***\n");
  printf ("Please verify that there is not another program running\n");
  printf ("which captures one of the keys captured by xbindkeys.\n");
  printf ("It seems that there is a conflict, and xbindkeys can't\n");
  printf ("grab all the keys defined in its configuration file.\n");

/*   end_it_all (d); */

/*   exit (-1); */
  //d=d; e=e; // to avoid warnings
  return (NULL);
}



static void
reload_rc_file (void)
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


static void
catch_HUP_signal (int sig)
{
  reload_rc_file ();
  //sig=sig; // to avoid warnings
}


static void
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
  //sig=sig; // to avoid warnings
}





static void
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
