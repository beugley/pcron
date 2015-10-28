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

# include <sys/param.h>
# include <sys/wait.h>
# include <stdlib.h>
# include <unistd.h>
# include <string.h>
# include <dirent.h>
# define DIR_T	struct dirent
# define WAIT_T	int
# define WAIT_IS_INT 1
extern char *tzname[2];
# define TZONE(tm) tzname[(tm).tm_isdst]

# define SIG_T sig_t
# define TIME_T time_t
# define PID_T pid_t

# ifndef WEXITSTATUS
#  define WEXITSTATUS(x) (((x) >> 8) & 0xff)
# endif
# ifndef WTERMSIG
#  define WTERMSIG(x)	((x) & 0x7f)
# endif
# ifndef WCOREDUMP
#  define WCOREDUMP(x)	((x) & 0x80)
# endif

#ifndef WIFSIGNALED
#define WIFSIGNALED(x)	(WTERMSIG(x) != 0)
#endif
#ifndef WIFEXITED
#define WIFEXITED(x)	(WTERMSIG(x) == 0)
#endif

extern	int		flock (int, int);
# define LOCK_SH 1
# define LOCK_EX 2
# define LOCK_NB 4
# define LOCK_UN 8

