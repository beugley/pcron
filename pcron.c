/* Copyright 1988,1990,1993,1994 by Paul Vixie
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

#include "pcron.h"
#include "libutil.h"
#include <sys/signal.h>
#include <time.h>

static	void	usage (void),
		cron_tick (cron_db *),
		cron_sync (void),
		cron_sleep (cron_db *),
		cron_clean (cron_db *),
		sigchld_handler (int),
		sigterm_handler (int),
		parse_args (int c, char *v[]);

static time_t	last_time = 0;
static int	dst_enabled = 1;
static struct passwd *pw;
struct pidfh *pfh;

static void
usage() {
	fprintf(stderr, "usage: pcron [-s] [-o] [-x debugflag[,...]]\n");
	exit(ERROR_EXIT);
}

static void
open_pidfile()
{
	char	pidfile[MAX_FNAME];
	char	buf[MAX_TEMPSTR];
	int	otherpid;

	(void) snprintf(pidfile, sizeof(pidfile), PID_FILE);
	pfh = pidfile_open(pidfile, 0600, &otherpid);
	if (pfh == NULL) {
		if (errno == EEXIST) {
			snprintf(buf, sizeof(buf),
			    "pcron already running, pid: %d", otherpid);
		} else {
			snprintf(buf, sizeof(buf),
			    "can't open or create %s: %s", pidfile,
			    strerror(errno));
		}
		log_it("PCRON", getpid(), "DEATH", buf);
		err(ERROR_EXIT, ("%s", buf))
	}
}

int
main(argc, argv)
	int	argc;
	char	*argv[];
{
	cron_db	database;

	ProgramName = argv[0];

	parse_args(argc, argv);

	(void) signal(SIGCHLD, sigchld_handler);
	(void) signal(SIGTERM, sigterm_handler);

	set_cron_cwd(pw);
	open_pidfile();
	pidfile_write(pfh);

	/* Open log file now so that all child processes will inherit the
	 * open file descriptor.
	 */
	{
	char detail[100];
	sprintf(detail, "started; dst_enabled is %d", dst_enabled);
	log_it("PCRON", getpid(), "INIT", detail);
	}

	database.head = NULL;
	database.tail = NULL;
	database.mtime = (time_t) 0;
	load_database(&database);
	cron_sync();
	warning_message();

	while (TRUE) {
		cron_sleep(&database);

		load_database(&database);

		/* do this iteration
		 */
		cron_tick(&database);

		/* sleep 1 minute
		 */
		TargetTime += 60;
	}
}


static void
cron_tick(db)
	cron_db	*db;
{
	static struct tm	lasttm;
	static long last_timezone, bse_timezone;
	static time_t	diff = 0, /* time difference in seconds from the last offset change */
	difflimit = 0; /* end point for the time zone correction */
	struct tm	otztm; /* time in the old time zone */
	int		otzminute, otzhour, otzdom, otzmonth, otzdow;
 	register struct tm	*tm = localtime(&TargetTime);
	register int		minute, hour, dom, month, dow;
	register user		*u;
	register entry		*e;

	/* make 0-based values out of these so we can use them as indicies
	 */
	minute = tm->tm_min -FIRST_MINUTE;
	hour = tm->tm_hour -FIRST_HOUR;
	dom = tm->tm_mday -FIRST_DOM;
	month = tm->tm_mon +1 /* 0..11 -> 1..12 */ -FIRST_MONTH;
	dow = tm->tm_wday -FIRST_DOW;

	Debug(DSCH, ("[%d] tick(%d,%d,%d,%d,%d)\n",
		getpid(), minute, hour, dom, month, dow))

// BSE: The "timezone" variable in HP-UX doesn't change value when DST starts
//      or ends (it apparently does in BSD Unix).  So, I added code to mimic
//      the timezone value change that this code expects.
	bse_timezone = timezone - (tm->tm_isdst ? 3600 : 0);

	if (dst_enabled && last_time != 0 
	&& TargetTime > last_time /* exclude stepping back */
// BSE: tm->tm_gmtoff is not defined in HP-UX's tm structure definition.
//      Replaced it with "-bse_timezone".  That's because tm_gmtoff is
//      negative (e.g. for EST, tm_gmtoff is -18000, which is -5*60*60)
//	     ORIGINAL BSD CODE: && tm->tm_gmtoff != lasttm.tm_gmtoff
   && -bse_timezone != -last_timezone
	) {

//	ORIGINAL BSD CODE: diff = tm->tm_gmtoff - lasttm.tm_gmtoff;
		diff = (-bse_timezone) - (-last_timezone);
		Debug(DMISC, ("[%d] DST: TargetTime %d, last_time %d, "
			"last_timezone %d, bse_timezone %d, diff %d\n",
			getpid(), TargetTime, last_time, last_timezone, bse_timezone, diff))

		if ( diff > 0 ) { /* ST->DST */
			/* mark jobs for an earlier run */
			log_it("PCRON", getpid(), "TIMEZONE CHANGE", "ST->DST");
			difflimit = TargetTime + diff;
			for (u = db->head;  u != NULL;  u = u->next) {
				for (e = u->crontab;  e != NULL;  e = e->next) {
					e->flags &= ~NOT_UNTIL;
					if ( e->lastrun >= TargetTime )
						e->lastrun = 0;
					/* not include the ends of hourly ranges */
					if ( e->lastrun < TargetTime - 3600 )
						e->flags |= RUN_AT;
					else
						e->flags &= ~RUN_AT;
				}
			}
		} else { /* diff < 0 : DST->ST */
			/* mark jobs for skipping because the hour is being repeated;
				the jobs already ran at the first occurrence of the hour,
				so skip them to prevent a second run */
			log_it("PCRON", getpid(), "TIMEZONE CHANGE", "DST->ST");
			difflimit = TargetTime - diff;
			for (u = db->head;  u != NULL;  u = u->next) {
				for (e = u->crontab;  e != NULL;  e = e->next) {
					e->flags |= NOT_UNTIL;
					e->flags &= ~RUN_AT;
				}
			}
		}
	}

	if (diff != 0) {
		/* if the time was reset of the end of special zone is reached */
		if (last_time == 0 || TargetTime >= difflimit) {
			/* disable the TZ switch checks */
			diff = 0;
			difflimit = 0;
			for (u = db->head;  u != NULL;  u = u->next) {
				for (e = u->crontab;  e != NULL;  e = e->next) {
					e->flags &= ~(RUN_AT|NOT_UNTIL);
				}
			}
		} else {
			/* get the time in the old time zone */
// BSE: tm->tm_gmtoff is not defined in HP-UX's tm structure definition.
//      Replaced it with "-timezone".  That's because tm_gmtoff is
//      negative (e.g. for EST, tm_gmtoff is -18000, which is -5*60*60)
//	     ORIGINAL BSD CODE: time_t difftime = TargetTime + tm->tm_gmtoff - diff;
			time_t difftime = TargetTime - bse_timezone - diff;
			gmtime_r(&difftime, &otztm);

			/* make 0-based values out of these so we can use them as indicies
			 */
			otzminute = otztm.tm_min -FIRST_MINUTE;
			otzhour = otztm.tm_hour -FIRST_HOUR;
			otzdom = otztm.tm_mday -FIRST_DOM;
			otzmonth = otztm.tm_mon +1 /* 0..11 -> 1..12 */ -FIRST_MONTH;
			otzdow = otztm.tm_wday -FIRST_DOW;
		}
	}

	/* the dom/dow situation is odd.  '* * 1,15 * Sun' will run on the
	 * first and fifteenth AND every Sunday;  '* * * * Sun' will run *only*
	 * on Sundays;  '* * 1,15 * *' will run *only* the 1st and 15th.  this
	 * is why we keep 'e->dow_star' and 'e->dom_star'.  yes, it's bizarre.
	 * like many bizarre things, it's the standard.
	 */
	for (u = db->head;  u != NULL;  u = u->next) {
		for (e = u->crontab;  e != NULL;  e = e->next) {
			Debug(DSCH|DEXT, ("user [%s:%d:%d:...] cmd=\"%s\"\n",
					  env_get("LOGNAME", e->envp),
					  e->uid, e->gid, e->cmd))

			if ( diff != 0 && (e->flags & (RUN_AT|NOT_UNTIL)) ) {
				if (bit_test(e->minute, otzminute)
				 && bit_test(e->hour, otzhour)
				 && bit_test(e->month, otzmonth)
				 && ( ((e->flags & DOM_STAR) || (e->flags & DOW_STAR))
					  ? (bit_test(e->dow,otzdow) && bit_test(e->dom,otzdom))
					  : (bit_test(e->dow,otzdow) || bit_test(e->dom,otzdom))
					)
				   ) {
					if ( e->flags & RUN_AT ) {
						e->flags &= ~RUN_AT;
						e->lastrun = TargetTime;
						job_add(e, u);
						continue;
					} else 
						e->flags &= ~NOT_UNTIL;
				} else if ( e->flags & NOT_UNTIL )
					continue;
			}

			if (bit_test(e->minute, minute)
			 && bit_test(e->hour, hour)
			 && bit_test(e->month, month)
			 && ( ((e->flags & DOM_STAR) || (e->flags & DOW_STAR))
			      ? (bit_test(e->dow,dow) && bit_test(e->dom,dom))
			      : (bit_test(e->dow,dow) || bit_test(e->dom,dom))
			    )
			   ) {
				e->flags &= ~RUN_AT;
				e->lastrun = TargetTime;
				job_add(e, u);
			}
		}
	}

	last_time = TargetTime;
	lasttm = *tm;
	last_timezone = bse_timezone;
}


/* the task here is to figure out how long it's going to be until :00 of the
 * following minute and initialize TargetTime to this value.  TargetTime
 * will subsequently slide 60 seconds at a time, with correction applied
 * implicitly in cron_sleep().  it would be nice to let cron execute in
 * the "current minute" before going to sleep, but by restarting cron you
 * could then get it to execute a given minute's jobs more than once.
 * instead we have the chance of missing a minute's jobs completely, but
 * that's something sysadmin's know to expect what with crashing computers..
 */
static void
cron_sync() {
 	register struct tm	*tm;

	TargetTime = time((time_t*)0);
	tm = localtime(&TargetTime);
	TargetTime += (60 - tm->tm_sec);
}


static void
cron_sleep(db)
	cron_db	*db;
{
	int	seconds_to_wait = 0;

	/*
	 * Loop until we reach the top of the next minute, sleep when possible.
	 */

	for (;;) {
		seconds_to_wait = (int) (TargetTime - time((time_t*)0));

		/*
		 * If the seconds_to_wait value is insane, jump the cron
		 */

		if (seconds_to_wait < -600 || seconds_to_wait > 600) {
			cron_clean(db);
			cron_sync();
			continue;
		}

		Debug(DSCH, ("[%d] TargetTime=%ld, sec-to-wait=%d\n",
			getpid(), (long)TargetTime, seconds_to_wait))

		/*
		 * If we've run out of wait time or there are no jobs left
		 * to run, break
		 */

		if (seconds_to_wait <= 0)
			break;
		if (job_runqueue() == 0) {
			Debug(DSCH, ("[%d] sleeping for %d seconds\n",
				getpid(), seconds_to_wait))

			sleep(seconds_to_wait);
		}
	}
}


/* if the time was changed abruptly, clear the flags related
 * to the daylight time switch handling to avoid strange effects
 */

static void
cron_clean(db)
	cron_db	*db;
{
	user		*u;
	entry		*e;

	last_time = 0;

	for (u = db->head;  u != NULL;  u = u->next) {
		for (e = u->crontab;  e != NULL;  e = e->next) {
			e->flags &= ~(RUN_AT|NOT_UNTIL);
		}
	}
}

static void
sigchld_handler(x) {
	WAIT_T		waiter;
	PID_T		pid;

	Debug(DPROC, ("[%d] SIGCHLD received\n", getpid()))
	for (;;) {
		pid = waitpid(-1, &waiter, WNOHANG);
		switch (pid) {
		case -1:
			Debug(DPROC,
				("[%d] sigchld...no children\n", getpid()))
			return;
		case 0:
			Debug(DPROC,
				("[%d] sigchld...no dead kids\n", getpid()))
			return;
		default:
			Debug(DPROC,
				("[%d] sigchld...pid #%d died, stat=%d\n",
				getpid(), pid, WEXITSTATUS(waiter)))
		}

	/* Catching the signal causes the action for SIGCHLD to be reset to
	 * SIGDFL.  Set the signal handler again so that the next SIGCHLD
	 * can be caught.
	 */
	(void) signal(SIGCHLD, sigchld_handler);
	}
}


static void
sigterm_handler(x) {
	log_it("PCRON", getpid(), "DEATH", "terminated");
	log_close();
	exit(0);
}


static void
parse_args(argc, argv)
	int	argc;
	char	*argv[];
{
	int	argch;
	char	*endp;

	if (!(pw = getpwuid(getuid())))
		err(ERROR_EXIT, ("your UID isn't in the passwd file, bailing out"))
	endpwent();
	(void) strncpy(User, pw->pw_name, (sizeof User)-1);
	User[(sizeof User)-1] = '\0';

	while ((argch = getopt(argc, argv, "osx:")) != -1) {
		switch (argch) {
		case 'o':
			dst_enabled = 0;
			break;
		case 's':
			dst_enabled = 1;
			break;
		case 'x':
			if (!set_debug_flags(optarg))
				usage();
			break;
		default:
			usage();
		}
	}
}

