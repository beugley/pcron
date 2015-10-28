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

/*
 * these are site-dependent
 */

			/*
			 * choose one of these MAILCMD commands.  I use
			 * /bin/mail for speed; it makes biff bark but doesn't
			 * do aliasing.  /usr/lib/sendmail does aliasing but is
			 * a hog for short messages.  aliasing is not needed
			 * if you make use of the MAILTO= feature in crontabs.
			 * (hint: MAILTO= was added for this reason).
			 */

#define MAILCMD "/usr/sbin/sendmail", mailto
#define MAILARGS "%s -FPCron -odi -oem -oi %s"
			/* -Fx	 = set full-name of sender
			 * -odi	 = Option Deliverymode Interactive
			 * -oem	 = Option Errors Mailedtosender
			 * -oi   = Option dot message terminator
			 * -t    = read recipients from header of message
			 */

/*
#define MAILCMD "/bin/mail", mailto
#define MAILARGS "%s -d %s"
*/
			/* -d = undocumented but common flag: deliver locally?
			 */

/* #define MAILCMD "/usr/mmdf/bin/submit" */	/*-*/
/* #define MAILARGS "%s -mlrxto %s" */		/*-*/

#define MAIL_DATE 			/*-*/
			/* should we include an ersatz Date: header in
			 * generated mail?  if you are using sendmail
			 * for MAILCMD, it is better to let sendmail
			 * generate the Date: header.
			 */

