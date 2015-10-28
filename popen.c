/*
 * 01/2011: Modified by B. Eugley as part of pcron port to HP-UX.
 *
 * Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software written by Ken Arnold and
 * published in UNIX Review, Vol. 6, No. 8.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

/* this came out of the ftpd sources; it's been modified to avoid the
 * globbing stuff since we don't need it.  also execvp instead of execv.
 */

#include "pcron.h"
#include <sys/signal.h>
#include <fcntl.h>


#define MAX_ARGS 100
#define WANT_GLOBBING 0

/*
 * Special version of popen which avoids call to shell.  This insures noone
 * may create a pipe to a hidden program as a side effect of a list or dir
 * command.
 */
static PID_T *pids;
static int fds;

FILE *
cron_popen(program, type, e)
	char *program, *type;
	entry *e;
{
	register char *cp;
	FILE *iop;
	int argc, pdes[2];
	PID_T pid;
	char *usernm;
	char *argv[MAX_ARGS + 1];
#if WANT_GLOBBING
	char **pop, *vv[2];
	int gargc;
	char *gargv[1000];
	extern char **glob(), **copyblk();
#endif

	if ((*type != 'r' && *type != 'w') || type[1])
		return(NULL);

	if (!pids) {
		if ((fds = getdtablesize()) <= 0)
			return(NULL);
		if (!(pids = (PID_T *)malloc((u_int)(fds * sizeof(PID_T)))))
			return(NULL);
		bzero((char *)pids, fds * sizeof(PID_T));
	}
	if (pipe(pdes) < 0)
		return(NULL);

	/* break up string into pieces */
	for (argc = 0, cp = program; argc < MAX_ARGS; cp = NULL)
		if (!(argv[argc++] = strtok(cp, " \t\n")))
			break;
	argv[MAX_ARGS] = NULL;

#if WANT_GLOBBING
	/* glob each piece */
	gargv[0] = argv[0];
	for (gargc = argc = 1; argv[argc]; argc++) {
		if (!(pop = glob(argv[argc]))) {	/* globbing failed */
			vv[0] = argv[argc];
			vv[1] = NULL;
			pop = copyblk(vv);
		}
		argv[argc] = (char *)pop;		/* save to free later */
		while (*pop && gargc < 1000)
			gargv[gargc++] = *pop++;
	}
	gargv[gargc] = NULL;
#endif

	iop = NULL;
	switch(pid = vfork()) {
	case -1:			/* error */
		(void)close(pdes[0]);
		(void)close(pdes[1]);
		goto pfree;
		/* NOTREACHED */
	case 0:				/* child */
		Debug(DPROC, ("[%d] mail_process Vfork()'ed\n", getpid()))
		if (e != NULL) {
			/* get new pgrp, void tty, etc.
			 */
			(void) setsid();
		}
		if (*type == 'r') {
			/* Do not share our parent's stdin */
			(void)close(0);
			(void)open("/dev/null", O_RDWR);
			if (pdes[1] != 1) {
				dup2(pdes[1], 1);
				dup2(pdes[1], 2);	/* stderr, too! */
				(void)close(pdes[1]);
			}
			(void)close(pdes[0]);
		} else {
			if (pdes[0] != 0) {
				dup2(pdes[0], 0);
				(void)close(pdes[0]);
			}
			/* Hack: stdout gets revoked */
			(void)close(1);
			(void)open("/dev/null", O_RDWR);
			(void)close(2);
			(void)open("/dev/null", O_RDWR);
			(void)close(pdes[1]);
		}
		if (e != NULL) {
			/* Set user's entire context, but skip the environment
			 * as cron provides a separate interface for this
			 */
			usernm = env_get("LOGNAME", e->envp);
// BSE: pcron doesn't need this code because it is already executing as
// the user.  This code only applies to a cron instance that is running as
// root and which will need to change the uid/gid of the process.
#ifdef ONLY_NEEDED_FOR_ROOT_EXECUTION
				/* set our directory, uid and gid.
				 */
				if (setgid(e->gid) != 0)
					_exit(ERROR_EXIT);
				if (initgroups(usernm, e->gid) != 0)
					_exit(ERROR_EXIT);
				if (setuid(e->uid) != 0)
					_exit(ERROR_EXIT);
#endif
			chdir(env_get("HOME", e->envp));
		}
#if WANT_GLOBBING
		execvp(gargv[0], gargv);
#else
		execvp(argv[0], argv);
#endif
		_exit(1);
	}
	/* parent; assume fdopen can't fail...  */
	if (*type == 'r') {
		iop = fdopen(pdes[0], type);
		(void)close(pdes[1]);
	} else {
		iop = fdopen(pdes[1], type);
		(void)close(pdes[0]);
	}
	pids[fileno(iop)] = pid;

pfree:
#if WANT_GLOBBING
	for (argc = 1; argv[argc] != NULL; argc++) {
/*		blkfree((char **)argv[argc]);	*/
		free((char *)argv[argc]);
	}
#endif
	return(iop);
}

int
cron_pclose(iop)
	FILE *iop;
{
	register int fdes;
	int omask;
	sigset_t signal_mask;
	sigset_t osignal_mask;
	WAIT_T stat_loc;
	PID_T pid;

	/*
	 * pclose returns -1 if stream is not associated with a
	 * `popened' command, or, if already `pclosed'.
	 */
	if (pids == 0 || pids[fdes = fileno(iop)] == 0)
		return(-1);
	(void)fclose(iop);
//BSE: sigblock is deprecated for Linux
//  Original BSD code: omask = sigblock(sigmask(SIGINT)|sigmask(SIGQUIT)|sigmask(SIGHUP));
	sigemptyset(&signal_mask);
	sigaddset(&signal_mask, SIGINT);
	sigaddset(&signal_mask, SIGQUIT);
	sigaddset(&signal_mask, SIGHUP);
	sigprocmask(SIG_BLOCK, &signal_mask, &osignal_mask);
// BSE end
	while ((pid = wait(&stat_loc)) != pids[fdes] && pid != -1) {
		Debug(DPROC, ("[%d] child %d returned, status %d\n",
			getpid(), pid, stat_loc))
		}
	Debug(DPROC, ("[%d] child %d returned, status %d\n",
		getpid(), pid, stat_loc))
//BSE: sigsetmask is deprecated for Linux
//  Original BSD code: (void)sigsetmask(omask);
	sigprocmask(SIG_SETMASK, &osignal_mask, NULL);
// BSE end
	pids[fdes] = 0;
	return (pid == -1 ? -1 : WEXITSTATUS(stat_loc));
}
