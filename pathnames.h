/* Copyright 1993,1994 by Paul Vixie
 * All rights reserved
 *
 * Distribute freely, except: don't remove my name from the source or
 * documentation (don't take credit for my work), mark your changes (don't
 * get me blamed for your possible bugs), don't alter or remove this
 * notice.  May be sold if buildable source is provided to buyer.  No
 * warrantee of any kind, express or implied, is included with this
 * software; use at your own risk, responsibility for damages (if any) to
 * anyone resulting from the use of this software rests entirely with the
 * user.
 *
 * Send bug reports, bug fixes, enhancements, requests, flames, etc., and
 * I'll try to keep a version up to date.  I can be reached as follows:
 * Paul Vixie          <paul@vix.com>          uunet!decwrl!vixie!paul
 */

/* CRONTAB_DIR is where the crontab lives.
 * This directory will have its modtime updated
 * whenever pcrontab changes the crontab; this is
 * the signal for pcron to look at the
 * crontab file and reload if the modtime is
 * newer than it was last time around (or which
 * didn't exist last time around...)
 */
#define DEFAULT_CRONTAB_DIR "pcron"
#define LOG_FILE "pcron.log"
#define PID_FILE "pcron.pid"

			/* what editor to use if no EDITOR or VISUAL
			 * environment variable specified.
			 */
#if defined(_PATH_VI)
# define EDITOR _PATH_VI
#else
# define EDITOR "/bin/vi"
#endif

#ifndef _PATH_KSHELL
# define _PATH_KSHELL "/bin/ksh"
#endif

#ifndef _PATH_DEFPATH
# define _PATH_DEFPATH "/usr/bin:/bin"
#endif

