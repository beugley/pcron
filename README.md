pcron
=====

Pcron (personal cron) is a cron-like scheduling tool available on Linux and HP-UX

pcron/pcrontab was ported from BSD Unix cron/crontab.  Original BSD code can be found at http://src.gnu-darwin.org/src/usr.sbin/cron


User Guide
==========

**Overview**
 
pcron is short for “personal cron”.  pcron is a job execution tool for Linux/Unix servers.  pcrontab is the utility that is used to define the job schedule that is executed by pcron.
 
pcron/pcrontab are similar to cron/crontab, which are the standard execution/scheduling tools for Linux/Unix.  pcron/pcrontab were created because cron/crontab usage is not allowed for most Linux/Unix users at Capital One.  Anyone can use pcron/pcrontab.
 
NOTE: pcron is not an Enterprise-Level utility and is not approved for mission-critical tasks.  pcron has no capability to restart failed jobs and will not run jobs that were skipped due to server downtime.  Please use Control-M for all mission-critical tasks.
 
 
**pcrontab**
 
pcrontab is the interface for defining a job schedule.  It works nearly the same as does the standard Linux/Unix tool crontab.  pcrontab allows a user to schedule a job to the minute level.  A job can be scheduled for any combination of minute, hour, day, month, and/or weekday.
pcrontab can be invoked in the following 4 ways:
  * pcrontab –l <p>lists the user's pcrontab file.</p>
  * pcrontab -r <p>remove the user's pcrontab file.</p>
  * pcrontab -e <p>edit the user's pcrontab file or create an empty file to edit if the pcrontab file does not exist.  pcrontab invokes the “vi” editor to edit the file.</p>
  * pcrontab file <p>create or replace the user’s pcrontab file with the specified file.</p>
 
 
Each line of the pcrontab must include six fields, separated by spaces, having the following format:
minute  hour  monthday  month  weekday  command
 
The first five fields specify the run frequency of the sixth field, the command.  The command field specifies the command to run and can include file redirection (e.g. “>output.dat”) or piping to other commands.  It should not include "nohup" or “&” to run the job in the background because your pcron server will already be running in the background (see the next section).
 
The syntax of the pcrontab file is identical to that of a crontab file.  The following lines were copied from the cron/crontab manual and describe the format of the first 5 fields of each line in the pcrontab file:

                        field | allowed values
                        ----- | --------------
                        minute | 0-59
                        hour | 0-23
                        day of month | 1-31
                        month | 1-12
                        day of week | 0-7 (0 or 7 is Sun)
 

A field may be an asterisk (\*), which always stands for "first-last".
Ranges of numbers are allowed.  Ranges are two numbers separated with a hyphen.   The specified range is
inclusive.  For example, 8-11 for an "hours" entry specifies execution at hours 8, 9, 10 and 11.
Lists are allowed.  A list is a set of numbers (or ranges) separated by commas.  Examples: "1,2,5,9", "0-4,8-12".
Step  values can be used in conjunction with ranges.  Following a range with "<number>" specifies skips ofthe
number's value through the range.  For example, "0-23/2" can be used in the hours field to specify command
execution every other hour.  Steps are also permitted after an asterisk, so if you want to say "every two hours",
just use "*/2".
 
 
The user’s pcrontab file is stored in your $HOME/pcron directory.  That directory is created by pcrontab if it does not already exist.  The pcrontab file has the user ID as its name.  The pcrontab file should never be edited directly by the user; always use “pcrontab –e” to modify it.  There is a good reason for this; the pcrontab executable includes syntax checking and will not let you create an invalid pcrontab file.  Direct editing of your pcrontab file could result in invalid commands that are not executed by pcron or may even produce undesirable results.  In short, pcrontab performs full syntax checking of the pcrontab file and should be used.
 
pcrontab only facilitates creation of the pcrontab file.  It does not execute any of the commands.  See the next section on pcron for execution.
 
 
**pcron**
 
pcron is the utility that executes the commands that are scheduled in the pcrontab file.  Each user must run their own instance of pcron.  pcron will read the pcrontab file that is in the $HOME/pcron directory.  To run pcron, execute “nohup pcron &”.  Your pcron process will then continue to execute after you log off and will run the scheduled tasks as defined in your pcrontab file.  You should not, and can not, run more than 1 instance of pcron.
 
pcron includes logic to read changes to the pcrontab file that are made while pcron is running.  In other words, if you are running pcron you can use pcrontab to modify your pcrontab file (add new jobs, remove jobs, or change the schedule of jobs) without having to stop and restart pcron.  pcron will notice when a pcrontab file is changed and will read the new job schedule.
 
Linux/Unix servers are periodically rebooted to perform various system maintenance tasks.  When this happens, your pcron process will be killed.  Each user will have to restart their pcron process after the Linux/Unix server has been restarted.  Jobs that were scheduled to be executed during system-down time will not be executed.  pcron has no knowledge of the past.  Missed jobs must be manually executed after Linux/Unix has restarted.
 
pcron includes logic to prohibit multiple instances of itself being run by the same user.  The “ps” command will display a list of your current processes.  It can be used to verify if pcron is running.  For example “ps –fu abc123” will display all running processes for user “abc123”.  If pcron is not in the list, then you will need to start it.
 
pcron writes to a log file named pcron.log in your $HOME/pcron directory.  It includes a line when it starts, a line for each job that it executes, and a line for each time it reloads the pcrontab file due to modification.
 
Some jobs that pcron executes may write text to stdout and/or stderr.  By default, pcron will ignore all such output.  But, you can optionally include a line in your pcrontab file that will direct pcron to mail this output to your Outlook mailbox.  Output from each job execution will be mailed separately.  The line must look as follows, with your name substitued in place of First.Last:
MAILTO=First.Last@capitalone.com
 
 
**Daily notification that your pcron server is running**
 
It's nice to know when your pcron server has stopped running.  Unfortunately, you will not be notified that your pcron process has been killed when the Linux/Unix server is shutdown.  So, the next best thing is to have pcron tell you, every day, that it is still running.  Do the following 2 steps:
  1. Add a MAILTO line to the top of your pcrontab file as described in the prior section
  2. Schedule this command in your pcrontab:     0 9 * * * echo "pcron is running"
The echo command will run every morning at 9AM.  Since the output is going to stdout and uncaptured (there is no > or >> to redirect it), pcron will send the text "pcron is running" to the address specified by the MAILTO setting.  So, if you don't get an email every morning at 9AM, then that means that your pcron server process is not running.
 
 
**Technical Information**
 
Pcron/pcrontab were created by porting the C source for cron/crontab from BSD (Berkeley Software Distribution) Unix.  The C source for pcron and pcrontab has been modified to compile on both HP-UX and Linux.  The code has also been modified to remove all logic that will enable pcron to register itself as a daemon.  pcron has no capability to launch processes as other users.  Therefore, each user must run their own instance of pcron.  pcron uses the standard fork() and exec() system calls to execute child processes as scheduled in the pcrontab file.
There is one significant difference between pcron and cron.  Cron launches processes with no environment.  Pcron inherits the users environment (set by your .profile upon login) and passes that along to its child processes (those that are scheduled with pcrontab).  This means that all of your scheduled processes will already have your environment when they start.  Specifically, this includes the PATH environment variable which specifies a list of directories to search for any executable.
 
Why pcron/pcrontab?  Why not?  They make use of scheduling logic that already existed in cron/crontab rather than writing our own.  The logic is solid because it’s been in use for a few decades, so why not leverage it?  It’s completely legal; BSD Unix is free and anyone can take the source code and modify it for their own use.  The only caveat is that the source code must continue to include the original headers and give credit to the original authors.  pcron/pcrontab source does include that information.  Credit goes to Paul Vixie for an excellent implementation of cron/crontab.



Compilation Instructions
========================
pcron/pcrontab has been modified to compile and run on HP-UX and Red Hat Linux.  Compilation on other *nix platforms may require additional modifications.  If you modify the code, please try to ensure that it will still compile and run correctly on both HP-UX and Red Hat Linux.

  1. Copy the pcron source code to your Unix host.  The source code contains
     a main directory with several C source files and 2 subdirectories, lib
     and include, that contain additional C source files.
  2. set your working directory to the top-level pcron source directory.
  3. Run "make clean all" to recompile the pcron and pcrontab binaries.  The
     "make" process should complete in no more than 30 seconds.


