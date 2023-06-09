/* Copy config.h-dist to config.h and edit config.h, don't edit this file */

/*
 * Configuration parameters for rc. Suggested defaults are at the bottom
 * of this file (you should probably look at those first to see if your
 * system matches one of them; you can search for the beginning of the
 * defaults section by looking for the string "#ifndef CUSTOM"). If you
 * want to override the suggested defaults, define the macro CUSTOM.
#define CUSTOM
 */

/*
 * (Note that certain default settings redefine this macro)
 * DEFAULTPATH the default search path that rc uses when it is started
 * without either a $PATH or $path environment variable. You must pick
 * something sensible for your system if you don't like the path shown
 * below.
 */
#define DEFAULTPATH "/usr/local/bin", "/usr/bin", "/usr/X11/bin", "/bin"

/*
 * Define the macro NODIRENT if your system has <sys/dir.h> but not
 * <dirent.h>. (e.g., NeXT-OS and RISCos)
#define NODIRENT
 */

/*
 * Define the macro SVSIGS if your system has System V signal semantics,
 * i.e., if "slow" system calls are interrupted rather than resumed
 * after returning from an interrupt handler. (If you are not sure what
 * this means, see the man page for signal(2). In any case, it is probably
 * safe to leave this macro undefined.)
#define SVSIGS
 */

/*
 * Define the macro NOCMDARG if you do not have /dev/fd or fifos on your
 * system. You may also want to define this if you have broken fifos.
#define NOCMDARG
 */

/*
 * Define TMPDIR if you need to have rc create its fifos in a directory
 * other than /tmp. For example, if you have a Sun with /tmp mounted
 * as a ramdisk (type "tmpfs") then you cannot use fifos in /tmp (sigh).
#define TMPDIR "/var/tmp"
 */

/*
 * Define the macro DEVFD if your system supports /dev/fd.
 */
#define DEVFD

/*
 * Define the macro NOLIMITS if your system does not support Berkeley
 * limits.
#define NOLIMITS
 */

/*
 * Define the macro NOSIGCLD if your system uses SIGCLD in the System
 * V way. (e.g., sgi's Irix)
#define NOSIGCLD
 */

/*
 * Define the macro READLINE if you want rc to call GNU readline
 * instead of read(2) on interactive shells.
 */
#define READLINE

/*
 * Define the macro NOEXECVE if your Unix does not interpret #! in the
 * kernel, and uncomment the EXECVE variable in the Makefile.
#define NOEXECVE
 */

/*
 * If you want rc to default to some interpreter for files which don't
 * have a legal #! on the first line, define the macro DEFAULTINTERP.
 */
#define DEFAULTINTERP "/bin/sh"

/*
 * If your /bin/sh (or another program you care about) rejects environment
 * variables with special characters in them (such as ':' or '-'), rc can
 * put out ugly variable names using [_0-9a-zA-Z] that encode the real name;
 * define PROTECT_ENV for this hack. (Known offenders: every sh I have tried;
 * SunOS (silently discards), NeXT (aborts with error), SGI (aborts with
 * error), Ultrix (sh seems to work, sh5 aborts with error))
#define PROTECT_ENV
 */

/*
 * Define the macro NOECHO if you wish to omit rc's echo builtin from the
 * compile.
#define NOECHO
 */

/*
 * Define the NOJOB if you do *not* wish rc to perform backgrounding
 * as if it were a job-control shell; that is, if you do *not* wish
 * it to put a command spawned in the background into a new process
 * group. Since most systems support job control and since there are
 * many broken programs that do not behave correctly when backgrounded
 * in a v7 non-job-control fashion, rc by default performs a job-
 * control-like backgrounding.
#define NOJOB
 */

/* Beginning of defaults section: */

#ifndef CUSTOM

/*
 * Suggested settings for Sun, NeXT and sgi (machines here at TAMU):
 */

#ifdef NeXT		/* Used on NextOS 2.1 */
#define NODIRENT
#define	PROTECT_ENV
#define NOCMDARG
#endif

#ifdef sgi		/* Used on Irix 3.3.[12] */
#define SVSIGS
#define NOSIGCLD
#define	PROTECT_ENV
#undef DEFAULTPATH
#define DEFAULTPATH "/usr/bsd", "/usr/sbin", "/usr/bin", "/bin", "."
#endif

#ifdef sun		/* Used on SunOS 4.1.1 */
#define PROTECT_ENV
#undef DEFAULTPATH
#define DEFAULTPATH "/usr/ucb", "/usr/bin", "."
#endif

/*
 * Suggested settings for HP300 running 4.3BSD-utah (DWS):
 */

#if defined(hp300) && !defined(hpux)
#define NODIRENT
#define NOCMDARG
#define DEFAULTINTERP "/bin/sh"
#define PROTECT_ENV
#endif

/*
 * Suggested settings for Ultrix
 */

#ifdef ultrix
#define PROTECT_ENV
#define DEFAULTINTERP "/bin/sh"	/* so /bin/true can work */
#endif

/*
 * Suggested settings for RISCos 4.52
 */

/*
   This doesn't work without interfering with other MIPS-based
   systems' configuration. Please do it by hand.
*/

#if defined(host_mips) && defined(MIPSEB) && defined(SYSTYPE_BSD43)
#define NODIRENT
#define PROTECT_ENV
#endif

/*
 * Suggested settings for AIX
 */

#ifdef _AIX
#define PROTECT_ENV
#endif

/*
 * Suggested settings for OSF/1 1.0
 */

#ifdef OSF1
#define PROTECT_ENV
#endif

/*
 * Suggested settings for Unicos XXX
 */

#ifdef cray
#define PROTECT_ENV
#define NOLIMITS
#define word _word
#define DEFAULTINTERP "/bin/sh"
#endif

#endif /* CUSTOM */

#ifndef TMPDIR
#define TMPDIR "/tmp"
#endif
