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
#include <string.h>
#include <ctype.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <libguile.h>
#include <ffi.h>

#include "grab_key.h"
#include "keys.h"
#include "options.h"

int init_xbk_guile_fns(Display *d, int verbose);
SCM set_numlock_wrapper(SCM x);
SCM set_scrolllock_wrapper(SCM x);
SCM set_capslock_wrapper(SCM x);
SCM xbindkey_wrapper(SCM key, SCM cmd);
SCM xbindkey_function_wrapper(SCM key, SCM fun);
SCM remove_xbindkey_wrapper(SCM key);
SCM run_command_wrapper(SCM command);

SCM grab_all_keys_wrapper(Display *, int);
SCM ungrab_all_keys_wrapper(Display *);

SCM remove_all_keys_wrapper(void);
SCM debug_info_wrapper(void);

// data struct and bindings for closures in libffi

struct grab_params {
  Display *d;
  int verbose;
};

void grab_binding(ffi_cif *cif, void *ret, void *args[], void *data) {
  struct grab_params p = *(struct grab_params *)data;
  *(ffi_arg *)ret = (ffi_arg)grab_all_keys_wrapper(p.d, p.verbose);
#ifdef AVOID_KNOWN_HARMLESS_WARNINGS
  cif = cif;
  args = args;
#endif
}

void ungrab_binding(ffi_cif *cif, void *ret, void *args[], void *d) {
  *(ffi_arg *)ret = (ffi_arg)ungrab_all_keys_wrapper((Display *)d);
#ifdef AVOID_KNOWN_HARMLESS_WARNINGS
  cif = cif;
  args = args;
#endif
}
// We need the closures and their data to be static so they don't
// expire and cause segfaults
// The closures are needed to remove the pain of having
// a lot of globals
static struct grab_params P;
static ffi_cif cif;
static ffi_type *args[0];
static ffi_closure *grab_closure, *ungrab_closure;
static void *bound_grab, *bound_ungrab;

int init_xbk_guile_fns(Display *d, int verbose) {
#ifdef DEBUG
  printf("initializing guile fns...\n");
#endif

  P.d = d;
  P.verbose = verbose;

  // adapted from the examples provided in the docs of libffi
  grab_closure = ffi_closure_alloc(sizeof(ffi_closure), &bound_grab);
  ungrab_closure = ffi_closure_alloc(sizeof(ffi_closure), &bound_ungrab);

  if (!grab_closure || !ungrab_closure) {
    fprintf(stderr, "Couldn't allocate space for the closures!\n");
    exit(-1);
  }

  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 0, &ffi_type_pointer, args) !=
      FFI_OK) {
    fprintf(stderr, "Something something error in generating closures!\n");
    exit(-1);
  }
  if (ffi_prep_closure_loc(grab_closure, &cif, grab_binding, &P, bound_grab) !=
      FFI_OK) {
    fprintf(stderr, "Error in prepping closure for grab!\n");
    exit(-1);
  }
  if (ffi_prep_closure_loc(ungrab_closure, &cif, ungrab_binding, d,
                           bound_ungrab) != FFI_OK) {
    fprintf(stderr, "Error in prepping closure for ungrab!\n");
    exit(-1);
  }

  scm_c_define_gsubr("set-numlock!", 1, 0, 0, set_numlock_wrapper);
  scm_c_define_gsubr("set-scrolllock!", 1, 0, 0, set_scrolllock_wrapper);
  scm_c_define_gsubr("set-capslock!", 1, 0, 0, set_capslock_wrapper);
  scm_c_define_gsubr("xbindkey", 2, 0, 0, xbindkey_wrapper);
  scm_c_define_gsubr("xbindkey-function", 2, 0, 0, xbindkey_function_wrapper);
  scm_c_define_gsubr("remove-xbindkey", 1, 0, 0, remove_xbindkey_wrapper);
  scm_c_define_gsubr("run-command", 1, 0, 0, run_command_wrapper);

  scm_c_define_gsubr("grab-all-keys", 0, 0, 0, bound_grab);
  scm_c_define_gsubr("ungrab-all-keys", 0, 0, 0, bound_ungrab);

  scm_c_define_gsubr("remove-all-keys", 0, 0, 0, remove_all_keys_wrapper);
  scm_c_define_gsubr("debug", 0, 0, 0, debug_info_wrapper);
  return 0;
}

extern int get_rc_guile_file(Display *d, char *rc_guile_file, int verbose) {
  FILE *stream;

#ifdef DEBUG
  printf("getting rc guile file %s.\n", rc_guile_file);
#endif

  if (init_keys() != 0)
    return (-1);

  /* Open RC File */
  if ((stream = fopen(rc_guile_file, "r")) == NULL) {
    fprintf(stderr, "ERROR : %s not found or reading not allowed.\n",
            rc_guile_file);
    return (-1);
  }
  fclose(stream);

  init_xbk_guile_fns(d, verbose);
  scm_primitive_load(scm_from_locale_string(rc_guile_file));
  return 0;
}

/* Taken out from the following macro
  #ifdef DEBUG		   \
    printf("Running mask cmd!\n"); \
    #endif \*/
#define MAKE_MASK_WRAPPER(name, mask_name)                                     \
  SCM name(SCM val) {                                                          \
    mask_name = SCM_FALSEP(val);                                               \
    return SCM_UNSPECIFIED;                                                    \
  }

MAKE_MASK_WRAPPER(set_numlock_wrapper, numlock_mask);
MAKE_MASK_WRAPPER(set_scrolllock_wrapper, scrolllock_mask);
MAKE_MASK_WRAPPER(set_capslock_wrapper, capslock_mask);

SCM extract_key(SCM key, KeyType_t *type, EventType_t *event_type,
                KeySym *keysym, KeyCode *keycode, unsigned int *button,
                unsigned int *modifier) {
  char *str;
  int len;

  while (SCM_CONSP(key)) { // Iterate through the list (If it is a list)
    if (!SCM_CONSP(SCM_CDR(key))) { // if this is the last item
      key = SCM_CAR(key);           // go to that
      break;                        // and continue
    }
    // Otherwise, this is a modifier.

    // So copy it:
    // Guile strings are not \0 terminated. hence we must copy.
    if (scm_is_true(scm_symbol_p(SCM_CAR(key)))) {
      SCM newkey = scm_symbol_to_string(SCM_CAR(key));
      str = scm_to_locale_string(newkey);
    } else {
      str = scm_to_locale_string(SCM_CAR(key));
    }
    len = strlen(str);

    /*str = scm_to_locale_string(SCM_CAR(key));*/

#ifdef DEBUG // extra verbosity here.
    printf("xbindkey_wrapper debug: modifier = %s.\n", str);
#endif

    // copied directly with some substitutions. ie. line2 -> str
    // Do whatever needs to be done with modifiers.
    if (strncasecmp(str, "control", len) == 0)
      *modifier |= ControlMask;
    else if (strncasecmp(str, "shift", len) == 0)
      *modifier |= ShiftMask;
    else if (strncasecmp(str, "mod1", len) == 0 ||
             strncasecmp(str, "alt", len) == 0)
      *modifier |= Mod1Mask;
    else if (strncasecmp(str, "mod2", len) == 0)
      *modifier |= Mod2Mask;
    else if (strncasecmp(str, "mod3", len) == 0)
      *modifier |= Mod3Mask;
    else if (strncasecmp(str, "mod4", len) == 0)
      *modifier |= Mod4Mask;
    else if (strncasecmp(str, "mod5", len) == 0)
      *modifier |= Mod5Mask;
    else if (strncasecmp(str, "release", len) == 0)
      *event_type = RELEASE;
    else if (strlen(str) > 2 && str[0] == 'm' && str[1] == ':') {
      *modifier |= strtol(str + 2, (char **)NULL, 0);
      // this break have nothing to do here!
      // break
    } else {
      printf("Bad modifier:\n%s\n", str); // or error
      return SCM_BOOL_F;                  // and return false
    }
    free(str); // we copied, so we must destroy this
    str = NULL;
    key = SCM_CDR(key); // and go a step down the list
  }
  // So this was either the only or last item of the 1st arg
  // Hence it is the key

  // So copy it:
  // Guile strings are not \0 terminated. hence we must copy.
  if (scm_is_true(scm_symbol_p(key))) {
    SCM newkey = scm_symbol_to_string(key);
    str = scm_to_locale_string(newkey);
  } else {
    str = scm_to_locale_string(key);
  }
  len = strlen(str);

#ifdef DEBUG
  printf("xbindkey_wrapper debug: key = %s\n", str);
#endif

  // Check for special numeric stuff.
  // This way is really far nicer looking and more efficient than
  // having three copies of the code.
  if (strlen(str) > 2 && str[1] == ':' && isdigit(str[2])) {
    switch (str[0]) {
    case 'b':
      *type = BUTTON;
      *button = strtol(str + 2, (char **)NULL, 0);
      break;
    case 'c':
      *type = CODE;
      *keycode = strtol(str + 2, (char **)NULL, 0);
      break;
    case 'm': // is a modifier so it is in the other part.
      printf("bad modifier: %s.", str);
      printf("m: modifiers need be applied to keys\n");
      return SCM_BOOL_F;
    default:
      printf("bad modifier: %c: shoud be b:, c: or m: .\n", str[0]);
      return SCM_BOOL_F;
    }
  } else // regular key
  {
    *type = SYM;
    *keysym = XStringToKeysym(str);
    if (*keysym == 0) {
      printf("No keysym for key: %s\n", str);
      return SCM_BOOL_F;
    }
  }

  free(str); // these were used by add key and copied.

  return SCM_BOOL_T;
}

SCM xbindkey_wrapper(SCM key, SCM cmd) {
  KeyType_t type = SYM;
  EventType_t event_type = PRESS;
  KeySym keysym = 0;
  KeyCode keycode = 0;
  unsigned int button = 0;
  unsigned int modifier = 0;
  char *cmdstr;

  // Guile strings are not \0 terminated. hence we must copy.
  cmdstr = scm_to_locale_string(cmd);
#ifdef DEBUG
  printf("xbindkey_wrapper debug: cmd=%s.\n", cmdstr);
#endif

  if (extract_key(key, &type, &event_type, &keysym, &keycode, &button,
                  &modifier) == SCM_BOOL_F) {
    return SCM_BOOL_F;
  }

  if (add_key(type, event_type, keysym, keycode, button, modifier, cmdstr, 0) !=
      0) {
    printf("add_key didn't return 0!!!\n");
    return SCM_BOOL_F;
  }

  free(cmdstr); // we may get rid of them!

  return SCM_UNSPECIFIED;
}

SCM tab_scm[2];

SCM xbindkey_function_wrapper(SCM key, SCM fun) {
  KeyType_t type = SYM;
  EventType_t event_type = PRESS;
  KeySym keysym = 0;
  KeyCode keycode = 0;
  unsigned int button = 0;
  unsigned int modifier = 0;

  if (extract_key(key, &type, &event_type, &keysym, &keycode, &button,
                  &modifier) == SCM_BOOL_F) {
    return SCM_BOOL_F;
  }

  tab_scm[0] = fun;

  if (add_key(type, event_type, keysym, keycode, button, modifier, NULL,
              tab_scm[0]) != 0) {
    printf("add_key didn't return 0!!!\n");
    return SCM_BOOL_F;
  } else {
    printf("add_key ok!!!  fun=%d\n", (scm_procedure_p(fun) == SCM_BOOL_T));
  }

  // scm_permanent_object (tab_scm[0]);
  scm_remember_upto_here_1(tab_scm[0]);

  return SCM_UNSPECIFIED;
}

SCM remove_xbindkey_wrapper(SCM key) {
  KeyType_t type = SYM;
  EventType_t event_type = PRESS;
  KeySym keysym = 0;
  KeyCode keycode = 0;
  unsigned int button = 0;
  unsigned int modifier = 0;

  if (extract_key(key, &type, &event_type, &keysym, &keycode, &button,
                  &modifier) == SCM_BOOL_F) {
    return SCM_BOOL_F;
  }

  if (remove_key(type, event_type, keysym, keycode, button, modifier) != 0) {
    printf("remove_key didn't return 0!!!\n");
    return SCM_BOOL_F;
  }

  return SCM_UNSPECIFIED;
}

SCM run_command_wrapper(SCM command) {
  char *cmdstr;

  cmdstr = scm_to_locale_string(command);

  run_command(cmdstr);

  free(cmdstr);

  return SCM_UNSPECIFIED;
}

SCM grab_all_keys_wrapper(Display *d, int verbose) {
#ifdef DEBUG
  printf("Called grab wrapper callback\n");
#endif
  grab_keys(d, verbose);
  return SCM_UNSPECIFIED;
}

SCM ungrab_all_keys_wrapper(Display *d) {
#ifdef DEBUG
  printf("Called ungrab wrapper callback\n");
#endif
  ungrab_all_keys(d);
  return SCM_UNSPECIFIED;
}

SCM remove_all_keys_wrapper(void) {
  close_keys();

  return SCM_UNSPECIFIED;
}

SCM debug_info_wrapper(void) {
  printf("\nKeys = %p\n", keys);
  printf("nb_keys = %d\n", nb_keys);

  return SCM_UNSPECIFIED;
}
