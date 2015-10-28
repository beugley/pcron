/*
 * 01/2011: Modified by B. Eugley as part of pcron port to HP-UX.
 *
 * Copyright 1988,1990,1993,1994 by Paul Vixie
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

#define	MAIN_PROGRAM

#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include "pcron.h"
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <time.h>
#include <utime.h>

#define NHEADER_LINES 5

enum opt_t	{ opt_unknown, opt_list, opt_delete, opt_edit, opt_replace };

#if DEBUGGING
static char	*Options[] = { "???", "list", "delete", "edit", "replace" };
#endif

static	pid_t		Pid;
static	char		Filename[MAX_FNAME];
static	FILE		*NewCrontab;
static	int		CheckErrorCount;
static	enum opt_t	Option;
static	struct passwd	*pw;
static	void		list_cmd();
static void			delete_cmd();
static void			edit_cmd();
static void			poke_pcron();
static void       check_error(char *);
static void			parse_args(int c, char *v[]);
static	int		replace_cmd();


static void
usage(msg)
	char *msg;
{
	fprintf(stderr, "pcrontab: usage error: %s\n", msg);
	fprintf(stderr, "%s\n%s\n",
		"usage: pcrontab file",
		"       pcrontab { -e | -l | -r }");
	exit(ERROR_EXIT);
}


int
main(argc, argv)
	int	argc;
	char	*argv[];
{
	int	exitstatus;

	Pid = getpid();
	ProgramName = argv[0];

	parse_args(argc, argv);		/* sets many globals, opens a file */
	set_cron_cwd(pw);

	exitstatus = OK_EXIT;
	switch (Option) {
	case opt_list:		list_cmd();
				break;
	case opt_delete:	delete_cmd();
				break;
	case opt_edit:		edit_cmd();
				break;
	case opt_replace:	if (replace_cmd() < 0)
					exitstatus = ERROR_EXIT;
				break;
	case opt_unknown:
				break;
	}
	exit(exitstatus);
}


static void
parse_args(argc, argv)
	int	argc;
	char	*argv[];
{
	int		argch;

	if (!(pw = getpwuid(getuid())))
		err(ERROR_EXIT, ("your UID isn't in the passwd file, bailing out"))
	(void) strncpy(User, pw->pw_name, (sizeof User)-1);
	User[(sizeof User)-1] = '\0';
 
	Filename[0] = '\0';
	Option = opt_unknown;
	while ((argch = getopt(argc, argv, "ler")) != -1) {
		switch (argch) {
		case 'l':
			if (Option != opt_unknown)
				usage("only one operation permitted");
			Option = opt_list;
			break;
		case 'r':
			if (Option != opt_unknown)
				usage("only one operation permitted");
			Option = opt_delete;
			break;
		case 'e':
			if (Option != opt_unknown)
				usage("only one operation permitted");
			Option = opt_edit;
			break;
		default:
			usage("unrecognized option");
		}
	}

	endpwent();

	if (Option != opt_unknown) {
		if (argv[optind] != NULL) {
			usage("no arguments permitted after this option");
		}
	} else {
		if (argv[optind] != NULL) {
			Option = opt_replace;
			(void) strncpy (Filename, argv[optind], (sizeof Filename)-1);
			Filename[(sizeof Filename)-1] = '\0';

		} else {
			usage("file name must be specified for replace");
		}
	}

	if (Option == opt_replace) {
		if (!strcmp(Filename, "-")) {
			NewCrontab = stdin;
		} else if (!(NewCrontab = fopen(Filename, "r")))
			err(ERROR_EXIT, ("%s", Filename))
	}

	Debug(DMISC, ("user=%s, file=%s, option=%s\n",
		      User, Filename, Options[(int)Option]))
}

static void
copy_file(FILE *in, FILE *out) {
	int	x, ch;

	Set_LineNum(1)
	/* ignore the top few comments since we probably put them there.
	 */
	for (x = 0;  x < NHEADER_LINES;  x++) {
		ch = get_char(in);
		if (EOF == ch)
			break;
		if ('#' != ch) {
			putc(ch, out);
			break;
		}
		while (EOF != (ch = get_char(in)))
			if (ch == '\n')
				break;
		if (EOF == ch)
			break;
	}

	/* copy the rest of the pcrontab (if any) to the output file.
	 */
	if (EOF != ch)
		while (EOF != (ch = get_char(in)))
			putc(ch, out);
}

static void
list_cmd() {
	FILE	*f;

	if (!(f = fopen(User, "r"))) {
		if (errno == ENOENT)
			err(ERROR_EXIT, ("no pcrontab for %s", User))
		else
			err(ERROR_EXIT, ("%s", User))
	}

	/* file is open. copy to stdout, close.
	 */
	copy_file(f, stdout);
	fclose(f);
}


static void
delete_cmd() {
	int ch, first;

	if (isatty(STDIN_FILENO)) {
		(void)fprintf(stderr, "remove pcrontab for %s? ", User);
		first = ch = getchar();
		while (ch != '\n' && ch != EOF)
			ch = getchar();
		if (first != 'y' && first != 'Y')
			return;
	}

	if (unlink(User)) {
		if (errno == ENOENT)
			err(ERROR_EXIT, ("no pcrontab for %s", User))
		else
			err(ERROR_EXIT, ("%s", User))
	}
	poke_pcron();
}


static void
check_error(msg)
	char	*msg;
{
	CheckErrorCount++;
	fprintf(stderr, "\"%s\":%d: %s\n", Filename, LineNumber-1, msg);
}


static void
edit_cmd() {
	char		q[MAX_TEMPSTR], *editor;
	FILE		*f;
	int		t;
	struct stat	statbuf, fsbuf;
	int		waiter;
	pid_t		pid, xpid;
	mode_t		um;
	int		syntax_error = 0;

	if (!(f = fopen(User, "r"))) {
		if (errno != ENOENT)
			err(ERROR_EXIT, ("%s", User))
		warn(("no pcrontab for %s - using an empty one", User))
		if (!(f = fopen("/dev/null", "r")))
			err(ERROR_EXIT, ("/dev/null"))
	}

	um = umask(027);
	(void) sprintf(Filename, "/tmp/pcrontab.XXXXXX");
	if ((t = mkstemp(Filename)) == -1) {
		warn(("%s", Filename))
		(void) umask(um);
		goto fatal;
	}
	(void) umask(um);
	if (!(NewCrontab = fdopen(t, "r+"))) {
		warn(("fdopen"))
		goto fatal;
	}

	copy_file(f, NewCrontab);
	fclose(f);
	if (fflush(NewCrontab))
		err(ERROR_EXIT, ("%s", Filename))
	if (fstat(t, &fsbuf) < 0) {
		warn(("unable to fstat temp file"))
		goto fatal;
	}
 again:
	if (stat(Filename, &statbuf) < 0) {
		warn(("stat"))
 fatal:		unlink(Filename);
		exit(ERROR_EXIT);
	}
	if (statbuf.st_dev != fsbuf.st_dev || statbuf.st_ino != fsbuf.st_ino)
		err(ERROR_EXIT, ("temp file must be edited in place"))

	if (!(editor = getenv("EDITOR"))) {
		editor = EDITOR;
	}

	/* we still have the file open.  editors will generally rewrite the
	 * original file rather than renaming/unlinking it and starting a
	 * new one; even backup files are supposed to be made by copying
	 * rather than by renaming.  if some editor does not support this,
	 * then don't use it.  the security problems are more severe if we
	 * close and reopen the file around the edit.
	 */

	switch (pid = fork()) {
	case -1:
		warn(("fork"))
		goto fatal;
	case 0:
		/* child */
		if (chdir("/tmp") < 0)
			err(ERROR_EXIT, ("chdir(/tmp)"))
		if (strlen(editor) + strlen(Filename) + 2 >= MAX_TEMPSTR)
			err(ERROR_EXIT, ("editor or filename too long"))
		execlp(editor, editor, Filename, (char *)NULL);
		err(ERROR_EXIT, ("%s", editor))
		/*NOTREACHED*/
	default:
		/* parent */
		break;
	}

	/* parent */
	{
	void (*f[4])();
	f[0] = signal(SIGHUP, SIG_IGN);
	f[1] = signal(SIGINT, SIG_IGN);
	f[2] = signal(SIGTERM, SIG_IGN);
	xpid = wait(&waiter);
	signal(SIGHUP, f[0]);
	signal(SIGINT, f[1]);
	signal(SIGTERM, f[2]);
	}
	if (xpid != pid) {
		warn(("wrong PID (%d != %d) from \"%s\"", xpid, pid, editor))
		goto fatal;
	}
	if (WIFEXITED(waiter) && WEXITSTATUS(waiter)) {
		warn(("\"%s\" exited with status %d", editor, WEXITSTATUS(waiter)))
		goto fatal;
	}
	if (WIFSIGNALED(waiter)) {
		warn(("\"%s\" killed; signal %d (%score dumped)",
			editor, WTERMSIG(waiter), WCOREDUMP(waiter) ?"" :"no "))
		goto fatal;
	}
	if (stat(Filename, &statbuf) < 0) {
		warn(("stat"))
		goto fatal;
	}
	if (statbuf.st_dev != fsbuf.st_dev || statbuf.st_ino != fsbuf.st_ino)
		err(ERROR_EXIT, ("temp file must be edited in place"))
	warn(("installing new pcrontab"))
	warning_message();

	switch (replace_cmd()) {
	case 0:			/* Success */
		break;
	case -1:		/* Syntax error */
		for (;;) {
			printf("Do you want to retry the same edit? ");
			fflush(stdout);
			q[0] = '\0';
			(void) fgets(q, sizeof q, stdin);
			switch (islower(q[0]) ? q[0] : tolower(q[0])) {
			case 'y':
				syntax_error = 1;
				goto again;
			case 'n':
				goto abandon;
			default:
				fprintf(stderr, "Enter Y or N\n");
			}
		}
		/*NOTREACHED*/
	case -2:		/* Install error */
	abandon:
		warn(("edits left in %s", Filename))
		goto done;
	default:
		warn(("panic: bad switch() in replace_cmd()"))
		goto fatal;
	}

 done:
 unlink(Filename);
}


/* returns	0	on success
 *		-1	on syntax error
 *		-2	on install error
 */
static int
replace_cmd() {
	char	envstr[MAX_ENVSTR], tn[MAX_FNAME];
	FILE	*tmp;
	int	ch, eof;
	entry	*e;
	time_t	now = time(NULL);
	char	**envp = env_init();

	if (envp == NULL) {
		warn(("cannot allocate memory"))
		return (-2);
	}

	(void) sprintf(tn, "tmp.%d", Pid);
	if (!(tmp = fopen(tn, "w+"))) {
		warn(("%s", tn))
		return (-2);
	}

	/* write a signature at the top of the file.
	 *
	 * VERY IMPORTANT: make sure NHEADER_LINES agrees with this code.
	 */
	fprintf(tmp, "# Pcron; adapted from BSD-cron by SAS Platform, Brian Eugley\n");
	fprintf(tmp, "# DO NOT EDIT THIS FILE - instead use 'pcrontab -e' which checks syntax.\n");
	fprintf(tmp, "# See the crontab manpage for scheduling info (run 'man 5 crontab').\n");
	fprintf(tmp, "# Use 'nohup pcron &' to run your pcrontab.\n");
	fprintf(tmp, "# (%s installed on %-24.24s)\n", Filename, ctime(&now));

	/* copy the pcrontab to the tmp
	 */
	rewind(NewCrontab);
	Set_LineNum(1)
	while (EOF != (ch = get_char(NewCrontab)))
		putc(ch, tmp);
	ftruncate(fileno(tmp), ftell(tmp));
	fflush(tmp);  rewind(tmp);

	if (ferror(tmp)) {
		warn(("error while writing new pcrontab to %s", tn))
		fclose(tmp);  unlink(tn);
		return (-2);
	}

	/* check the syntax of the file being installed.
	 */

	/* BUG: was reporting errors after the EOF if there were any errors
	 * in the file proper -- kludged it by stopping after first error.
	 *		vix 31mar87
	 */
	Set_LineNum(1 - NHEADER_LINES)
	CheckErrorCount = 0;  eof = FALSE;
	while (!CheckErrorCount && !eof) {
		switch (load_env(envstr, tmp)) {
		case ERR:
			eof = TRUE;
			break;
		case FALSE:
			e = load_entry(tmp, check_error, pw, envp);
			if (e)
				free(e);
			break;
		case TRUE:
			break;
		}
	}

	if (CheckErrorCount != 0) {
		warn(("errors in pcrontab file, can't install"))
		fclose(tmp);  unlink(tn);
		return (-1);
	}

	if (chmod(tn, 0640) < OK)
	{
		warn(("chmod"))
		fclose(tmp);  unlink(tn);
		return (-2);
	}

	if (fclose(tmp) == EOF) {
		warn(("fclose"))
		unlink(tn);
		return (-2);
	}

	if (rename(tn, User)) {
		warn(("error renaming %s to %s", tn, User))
		unlink(tn);
		return (-2);
	}

	poke_pcron();

	return (0);
}


static void
poke_pcron() {
	if (utime(".", NULL) < OK) {
		warn(("can't update mtime on pcrontab dir %s", CRONTAB_DIR))
		return;
	}
}
