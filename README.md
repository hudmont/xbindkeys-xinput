

# Overview
This is a fork of Philippe Brochard's [xbindkeys](http://www.nongnu.org/xbindkeys/xbindkeys.html).
See other contributors in that xbindkeys' AUTHORS file.
With this program, you can create your own shortcuts on X11-based graphical environments.

# Requirements

- X.Org
- Xinput ( if it's a separate package )
- GNU Guile

Additional requirements to compile from source:
    
- The X11 include files and libraries. (Not the X server development packages)
- GNU Autotools
- GNU Make

# Installation

  configure --help
  configure (and any options you want to set)
  make
  make install


# Configuration
The configuration is taken care of in the file called 
	"$HOME/.xbindkeysrc.scm"

You can have a default file with the --defaults option:

	xbindkeys --defaults

The semicolon(;) symbol may be used anywhere for comments. 

	(xbindkey '(modifier modifier key) "Command to start &")

Where modifier are:

Control, Shift, Alt or Mod1, Mod2 (numlock), Mod3, Mod4, Mod5 (modifiers are not case sensitive).

By defaults, xbindkeys does not pay attention with the modifiers
NumLock, CapsLock and ScrollLock.
Add the lines above in the config file, if you want to pay attention to them.
	(set-numlock! #t)
	(set-scrolllock! #t)
	(set-capslock! #t)


Use 'xev' or 'xbindkeys --key' or 'xbindkeys --multikey' to know modifier 
and keycode or keysym.

Example:

        # control+alt+mod2 + d (it's a comment)
          "xterm &"
          control+alt+mod2 + d

        # control+alt+mod2 + f (it's a comment)
           "rxvt &"
           m:0x1c + c:41

Here, pushing control+alt+mod2 (numlock enabled) and d starts  an xterm.
And pushing control+alt+mod2 and f starts rxvt.

Please, don't forget the '&' at the end of the command,
if not xbindkeys will launch only one command at the same time

You have a full access to the xbindkeys internals from guile scheme. This allow some more complicated configuration file and prevent the need of a shell script (like double click, timed double click or keys combinations). See xbindkeysrc.scm or xbindkeysrc-combo.scm in the source directory for some
examples, or print the defaults with xbindkeys.
Note: There is no need to hack the xbindkeys
source code just to achieve these compilcated configuration. All can be done with the guile file.

[For more details, please see here](http://www.gnu.org/software/guile/guile.html)

#CLI Options

Use the -h or --help option for all available options.

  -f
  --file
                use an alternative xbindkeysrc.scm (default is $HOME/.xbindkeysrc.scm)

  -h
--help
                help for xbindkeys.

  -v
  --verbose
                Verbose mode. Print more information when
                the program is running.
         
  -X <display>
  --display <display>
                use XServer at a specified display in standard X form.
                using this mode allows for multiple displays.

  -d
  --defaults
                Show a default configuration file. You can use it to create the file
                $HOME/.xbindkeysrc.scm like this:

                      xbindkeys --defaults > $HOME/.xbindkeysrc.scm

  -k
  --key
                Identify key pressed. This option is usefull to fill
                $HOME/.xbindkeysrc. You have just to press a key to
                know what to put in the config file.

  -m
  --multikey
                Identify key pressed like with --key option, but you can 
		press modifier and key, and you can make multi tries.
		You have just to press a key to know what to put in the 
		config file.

  -g
  --geometry
                Set size and position of the window open with the 
		--key / --multikey options.

  -n
  --nodaemon
                Don't start as daemon. By default, xbindkeys starts in
                background, this option prevents this feature.

# Usage

  xbindkeys

If you want, you can start it automatically with X or your preferred desktop environment.

For example, edit your .xsession or .XClients or .xinitrc (if any) and add a line like this:

  xbindkeys

# Current known bugs and improvements, will make issues out of them later

- When there is more than one button in a key combination. [ref](http://lists.nongnu.org/archive/html/xbindkeys-devel/2009-05/msg00001.html)  Update: Only one button is bound at a time (the last defined in the configuration file). And only one button is grabbed even when there is more than one button in the configuration file. => probably need more info.

- Replace the XNextEvent with a XPeekEvent/XNextEvent in the main loop to let the event pass to the underlying X application on some
    criteria.

- Make the documentation more easy to understand :)

- convert ChangeLog file to the standard ChangeLog file format

- create a nice printable readme for printing (LaTeX or DocBook?)

- create a web readme ( probably a github wiki page )

# Contact Info
[Zsombor Barna](mailto:unibro@mailo.com)
