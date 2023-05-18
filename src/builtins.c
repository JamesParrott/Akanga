/* builtins.c: the collection of rc's builtin commands */

/*
	NOTE: rc's builtins do not call "rc_error" because they are
	commands, and rc errors usually arise from syntax errors. e.g.,
	you probably don't want interpretation of a shell script to stop
	because of a bad umask.
*/

#include <sys/ioctl.h>
#include <setjmp.h>
#include <errno.h>
#include "rc.h"
#include "jbwrap.h"
#include "sigmsgs.h"
#ifndef NOLIMITS
#include <sys/time.h>
#include <sys/resource.h>
#endif
#ifdef WITH_VARSUP
#include <sys/fcntl.h>
#include <ctype.h>
#endif
#include "addon.h"

extern int umask(int);

static void b_break(char **), b_cd(char **), b_eval(char **), b_exit(char **),
	b_newpgrp(char **), b_return(char **), b_shift(char **), b_umask(char **),
	b_wait(char **), b_whatis(char **);

#ifndef NOLIMITS
static void b_limit(char **);
#endif
#ifndef NOECHO
static void b_echo(char **);
#endif

#ifdef WITH_TMPFILE
extern char *get_tmpfile(int *error);
extern char *get_localtmp(int *error);

static void b_mktemp(char **);
#endif

#ifdef WITH_VARSUP
extern char *skip_ws(char *string);
extern char *noctrl(char *string);
extern char *readline_fd(int fd);
extern void *allocate(int size);

static void b_initvars(char **);
static void b_readvars(char **);
static void b_read(char **);
static void b_readconf(char **);
static void b_parseopt(char **);
#endif

#ifdef WITH_LOCKING
static void b_lock(char **);
static void b_unlock(char **);
#endif

#ifdef WITH_EXPR
static void b_expr(char **);
#endif

#ifdef WITH_MISC
#endif



static struct {
	builtin_t *p;
	char *name;
} builtins[] = {
	{ b_break,	"break" },
	{ b_builtin,	"builtin" },
	{ b_cd,		"cd" },
#ifndef NOECHO
	{ b_echo,	"echo" },
#endif
	{ b_eval,	"eval" },
	{ b_exec,	"exec" },
	{ b_exit,	"exit" },
#ifndef NOLIMITS
	{ b_limit,	"limit" },
#endif
	{ b_newpgrp,	"newpgrp" },
	{ b_return,	"return" },
	{ b_shift,	"shift" },
	{ b_umask,	"umask" },
	{ b_wait,	"wait" },
	{ b_whatis,	"whatis" },
	{ b_dot,	"." },
	
#ifdef WITH_TMPFILE
	{ b_mktemp,	"mktemp" },
#endif

#ifdef WITH_VARSUP
	{ b_initvars,	"initvars" },
	{ b_readvars,	"readvars" },
	{ b_read,	"read" },
	{ b_readconf,	"readconf" },
	{ b_parseopt,	"parseopt" },
#endif

#ifdef WITH_LOCKING
	{ b_lock,	"lock" },
	{ b_unlock,	"unlock" },
#endif

#ifdef WITH_EXPR
	{ b_expr,	"let" },
	{ b_expr,	"expr" },
#endif

#ifdef WITH_MISC
#endif

	ADDONS
};

extern builtin_t *isbuiltin(char *s) {
	int i;
	for (i = 0; i < arraysize(builtins); i++)
		if (streq(builtins[i].name, s))
			return builtins[i].p;
	return NULL;
}

/* funcall() is the wrapper used to invoke shell functions. pushes $*, and "return" returns here. */

extern void funcall(char **av) {
	Jbwrap j;
	Estack e1, e2;
	Edata jreturn, star;
	if (setjmp(j.j))
		return;
	starassign(*av, av+1, TRUE);
	jreturn.jb = &j;
	star.name = "*";
	except(eReturn, jreturn, &e1);
	except(eVarstack, star, &e2);
	walk(treecpy(fnlookup(*av), nalloc), TRUE);
	varrm("*", TRUE);
	unexcept(); /* eVarstack */
	unexcept(); /* eReturn */
}

static void arg_count(char *name) {
	fprint(2, "too many arguments to %s\n", name);
	set(FALSE);
}

static void badnum(char *num) {
	fprint(2, "%s is a bad number\n", num);
	set(FALSE);
}

/* a dummy command. (exec() performs "exec" simply by not forking) */

extern void b_exec(char **av) {
}

#ifndef NOECHO
/* echo -n omits a newline. echo -- -n echos '-n' */

static void b_echo(char **av) {
	char *format = "%A\n";
	if (*++av != NULL) {
		if (streq(*av, "-n"))
                	format = "%A", av++;
		else if (streq(*av, "--"))
			av++;
	}
	fprint(1, format, av);
	set(TRUE);
}
#endif

/* cd. traverse $cdpath if the directory given is not an absolute pathname */

static void b_cd(char **av) {
	List *s, nil;
	char *path = NULL;
	SIZE_T t, pathlen = 0;
	if (*++av == NULL) {
		s = varlookup("home");
		*av = (s == NULL) ? "/" : s->w;
	} else if (av[1] != NULL) {
		arg_count("cd");
		return;
	}
	if (isabsolute(*av) || streq(*av, ".") || streq(*av, "..")) { /* absolute pathname? */
		if (chdir(*av) < 0) {
			set(FALSE);
			uerror(*av);
		} else
			set(TRUE);
	} else {
		s = varlookup("cdpath");
		if (s == NULL) {
			s = &nil;
			nil.w = "";
			nil.n = NULL;
		}
		do {
			if (s != &nil && *s->w != '\0') {
				t = strlen(*av) + strlen(s->w) + 2;
				if (t > pathlen)
					path = nalloc(pathlen = t);
				strcpy(path, s->w);
				if (!streq(s->w, "/")) /* "//" is special to POSIX */
					strcat(path, "/");
				strcat(path, *av);
			} else {
				pathlen = 0;
				path = *av;
			}
			if (chdir(path) >= 0) {
				set(TRUE);
				if (interactive && *s->w != '\0' && !streq(s->w, "."))
					fprint(1, "%s\n", path);
				return;
			}
			s = s->n;
		} while (s != NULL);
		fprint(2, "couldn't cd to %s\n", *av);
		set(FALSE);
	}
}

static void b_umask(char **av) {
	int i;
	if (*++av == NULL) {
		set(TRUE);
		i = umask(0);
		umask(i);
		fprint(1, "0%o\n", i);
	} else if (av[1] == NULL) {
		i = o2u(*av);
		if ((unsigned int) i > 0777) {
			fprint(2, "bad umask\n");
			set(FALSE);
		} else {
			umask(i);
			set(TRUE);
		}
	} else {
		arg_count("umask");
		return;
	}
}

static void b_exit(char **av) {
	if (*++av != NULL)
		ssetstatus(av);
	rc_exit(getstatus());
}

/* raise a "return" exception, i.e., return from a function. if an integer argument is present, set $status to it */

static void b_return(char **av) {
	if (*++av != NULL)
		ssetstatus(av);
	rc_raise(eReturn);
}

/* raise a "break" exception for breaking out of for and while loops */

static void b_break(char **av) {
	if (av[1] != NULL) {
		arg_count("break");
		return;
	}
	rc_raise(eBreak);
}


#ifdef WITH_VARSUP

/* shift for arbitrary variables */

static void b_shift2(char **av)
{
	int	shift;
	char	*var;
	List	*s;
	
	if ((var = av[1]) == NULL  ||  strchr("0123456789", *var) != NULL) {
		arg_count("shift");
		return;
		}

	shift = (av[2] == NULL)? 1: a2u(av[2]);
	if (shift < 0) {
		badnum(av[1]);
		return;
		}

	s = varlookup(var);
	while (s != NULL  &&  shift != 0) {
		s = s->n;
		--shift;
		}
		
	if (s == NULL  &&  shift != 0) {
		fprint(2, "cannot shift\n");
		set(FALSE);
		}
	else {
		varassign(var, s, FALSE);
		set(TRUE);
		}

	return;
}

#endif


/* shift $* n places (default 1) */

static void b_shift(char **av) {
	int shift = (av[1] == NULL ? 1 : a2u(av[1]));
	List *s, *dollarzero;

#ifdef WITH_VARSUP
	if ((av[1] != NULL  &&  strchr("0123456789", *av[1]) == NULL)  ||
	    (av[1] != NULL  &&  av[2] != NULL)) {
	    	b_shift2(av);
		return;
		}
#endif

	if (av[1] != NULL && av[2] != NULL) {
		arg_count("shift");
		return;
	}
	if (shift < 0) {
		badnum(av[1]);
		return;
	}
	s = varlookup("*")->n;
	dollarzero = varlookup("0");
	while (s != NULL && shift != 0) {
		s = s->n;
		--shift;
	}
	if (s == NULL && shift != 0) {
		fprint(2, "cannot shift\n");
		set(FALSE);
	} else {
		varassign("*", append(dollarzero, s), FALSE);
		set(TRUE);
	}
}

/* dud function */

extern void b_builtin(char **av) {
}

/* wait for a given process, or all outstanding processes */

static void b_wait(char **av) {
	int stat, pid;
	if (av[1] == NULL) {
		waitforall();
		return;
	}
	if (av[2] != NULL) {
		arg_count("wait");
		return;
	}
	if ((pid = a2u(av[1])) < 0) {
		badnum(av[1]);
		return;
	}
	if (rc_wait4(pid, &stat, FALSE) > 0)
		setstatus(pid, stat);
	else
		set(FALSE);
	sigchk();
}

/*
   whatis without arguments prints all variables and functions. Otherwise, check to see if a name
   is defined as a variable, function or pathname.
*/

#define not(b)	((b)^TRUE)
#define show(b)	(not(eff|vee|pee|bee|ess)|(b))

static bool issig(char *s) {
	int i;
	for (i = 0; i < NUMOFSIGNALS; i++)
		if (streq(s, signals[i].name))
			return TRUE;
	return FALSE;
}

static void b_whatis(char **av) {
	bool ess, eff, vee, pee, bee;
	bool f, found;
	int i, ac, c;
	List *s;
	Node *n;
	char *e;
	for (rc_optind = ac = 0; av[ac] != NULL; ac++)
		; /* count the arguments for getopt */
	ess = eff = vee = pee = bee = FALSE;
	while ((c = rc_getopt(ac, av, "sfvpb")) != -1)
		switch (c) {
		default: set(FALSE); return;
		case 's': ess = TRUE; break;
		case 'f': eff = TRUE; break;
		case 'v': vee = TRUE; break;
		case 'p': pee = TRUE; break;
		case 'b': bee = TRUE; break;
		}
	av += rc_optind;
	if (*av == NULL) {
		if (vee|eff)
			whatare_all_vars(eff, vee);
		if (ess)
			whatare_all_signals();
		if (bee)
			for (i = 0; i < arraysize(builtins); i++)
				fprint(1, "builtin %s\n", builtins[i].name);
		if (pee)
			fprint(2, "whatis -p: must specify argument\n");
		if (show(FALSE)) /* no options? */
			whatare_all_vars(TRUE, TRUE);
		set(TRUE);
		return;
	}
	found = TRUE;
	for (i = 0; av[i] != NULL; i++) {
		f = FALSE;
		errno = ENOENT;
		if (show(vee) && (s = varlookup(av[i])) != NULL) {
			f = TRUE;
			prettyprint_var(1, av[i], s);
		}
		if (((show(ess)&&issig(av[i])) || show(eff)) && (n = fnlookup(av[i])) != NULL) {
			f = TRUE;
			prettyprint_fn(1, av[i], n);
		} else if (show(bee) && isbuiltin(av[i]) != NULL) {
			f = TRUE;
			fprint(1, "builtin %s\n", av[i]);
		} else if (show(pee) && (e = which(av[i], FALSE)) != NULL) {
			f = TRUE;
			fprint(1, "%S\n", e);
		}
		if (!f) {
			found = FALSE;
			if (errno != ENOENT)
				uerror(av[i]);
			else
				fprint(2, "%s not found\n", av[i]);
		}
	}
	set(found);
}

/* push a string to be eval'ed onto the input stack. evaluate it */

static void b_eval(char **av) {
	bool i = interactive;
	if (av[1] == NULL)
		return;
	interactive = FALSE;
	pushstring(av + 1, i); /* don't reset line numbers on noninteractive eval */
	doit(TRUE);
	interactive = i;
}

/*
   push a file to be interpreted onto the input stack. with "-i" treat this as an interactive
   input source.
*/

extern void b_dot(char **av) {
	int fd;
	bool old_i = interactive, i = FALSE;
	Estack e;
	Edata star;
	av++;
	if (*av == NULL)
		return;
	if (streq(*av, "-i")) {
		av++;
		i = TRUE;
	}
	if (dasheye) { /* rc -i file has to do the right thing. reset the dasheye state to FALSE, though. */
		dasheye = FALSE;
		i = TRUE;
	}
	if (*av == NULL)
		return;
	fd = rc_open(*av, rFrom);
	if (fd < 0) {
		if (rcrc) /* on rc -l, don't flag nonexistence of .rcrc */
			rcrc = FALSE;
		else {
			uerror(*av);
			set(FALSE);
		}
		return;
	}
	rcrc = FALSE;
	starassign(*av, av+1, TRUE);
	pushfd(fd);
	interactive = i;
	star.name = "*";
	except(eVarstack, star, &e);
	doit(TRUE);
	varrm("*", TRUE);
	unexcept(); /* eVarstack */
	interactive = old_i;
}

/* put rc into a new pgrp. Used on the NeXT where the Terminal program is broken (sigh) */

static void b_newpgrp(char **av) {
	if (av[1] != NULL) {
		arg_count("newpgrp");
		return;
	}
	setpgrp(rc_pid, rc_pid);
#ifdef TIOCSPGRP
	ioctl(2, TIOCSPGRP, &rc_pid);
#endif
}

/* Berkeley limit support was cleaned up by Paul Haahr. */

#ifndef NOLIMITS
typedef struct Suffix Suffix;
struct Suffix {
	const Suffix *next;
	long amount;
	char *name;
};

static const Suffix
	kbsuf = { NULL, 1024, "k" },
	mbsuf = { &kbsuf, 1024*1024, "m" },
	gbsuf = { &mbsuf, 1024*1024*1024, "g" },
	stsuf = { NULL, 1, "s" },
	mtsuf = { &stsuf, 60, "m" },
	htsuf = { &mtsuf, 60*60, "h" };
#define	SIZESUF &gbsuf
#define	TIMESUF &htsuf
#define	NOSUF ((Suffix *) NULL)  /* for RLIMIT_NOFILE on SunOS 4.1 */

typedef struct {
	char *name;
	int flag;
	const Suffix *suffix;
} Limit;
static const Limit limits[] = {
	{ "cputime",		RLIMIT_CPU,	TIMESUF },
	{ "filesize",		RLIMIT_FSIZE,	SIZESUF },
	{ "datasize",		RLIMIT_DATA,	SIZESUF },
	{ "stacksize",		RLIMIT_STACK,	SIZESUF },
	{ "coredumpsize",	RLIMIT_CORE,	SIZESUF },
#ifdef RLIMIT_RSS /* SysVr4 does not have this */
	{ "memoryuse",		RLIMIT_RSS,	SIZESUF },
#endif
#ifdef RLIMIT_VMEM /* instead, they have this! */
	{ "vmemory",		RLIMIT_VMEM,	SIZESUF },
#endif
#ifdef RLIMIT_NOFILE  /* SunOS 4.1 adds a limit on file descriptors */
	{ "descriptors",	RLIMIT_NOFILE,	NOSUF },
#endif
	{ NULL, 0, NULL }
};

#ifndef SYSVR4
extern int getrlimit(int, struct rlimit *);
extern int setrlimit(int, struct rlimit *);
#endif

static void printlimit(const Limit *limit, bool hard) {
	struct rlimit rlim;
	long lim;
	getrlimit(limit->flag, &rlim);
	if (hard)
		lim = rlim.rlim_max;
	else
		lim = rlim.rlim_cur;
	if (lim == RLIM_INFINITY)
		fprint(1, "%s \tunlimited\n", limit->name);
	else {
		const Suffix *suf;
		for (suf = limit->suffix; suf != NULL; suf = suf->next)
			if (lim % suf->amount == 0 && (lim != 0 || suf->amount > 1)) {
				lim /= suf->amount;
				break;
			}
		fprint(1, "%s \t%d%s\n", limit->name, lim, (suf == NULL || lim == 0) ? "" : suf->name);
	}
}

static long parselimit(const Limit *limit, char *s) {
	char *t;
	int len = strlen(s);
	long lim = 1;
	const Suffix *suf = limit->suffix;
	if (streq(s, "unlimited"))
		return RLIM_INFINITY;
	if (suf == TIMESUF && (t = strchr(s, ':')) != NULL) {
		*t++ = '\0';
		lim = 60 * a2u(s) + a2u(t);
	} else {
		for (; suf != NULL; suf = suf->next)
			if (streq(suf->name, s + len - strlen(suf->name))) {
				s[len - strlen(suf->name)] = '\0';
				lim *= suf->amount;
				break;
			}
		lim *= a2u(s);
	}
	return lim;
}

static void b_limit(char **av) {
	const Limit *lp = limits;
	bool hard = FALSE;
	if (*++av != NULL && streq(*av, "-h")) {
		av++;
		hard = TRUE;
	}
	if (*av == NULL) {
		for (; lp->name != NULL; lp++)
			printlimit(lp, hard);
		return;
	}
	for (;; lp++) {
		if (lp->name == NULL) {
			fprint(2, "no such limit\n");
			set(FALSE);
			return;
		}
		if (streq(*av, lp->name))
			break;
	}
	if (*++av == NULL)
		printlimit(lp, hard);
	else {
		struct rlimit rlim;
		long pl;
		getrlimit(lp->flag, &rlim);
		if ((pl = parselimit(lp, *av)) < 0) {
			fprint(2, "bad limit\n");
			set(FALSE);
			return;
		}
		if (hard)
			rlim.rlim_max = pl;
		else
			rlim.rlim_cur = pl;
		if (setrlimit(lp->flag, &rlim) == -1) {
			uerror("setrlimit");
			set(FALSE);
		} else
			set(TRUE);
	}
}
#endif




#ifdef WITH_TMPFILE

/* Support for temporary files - 08AUG98wzk */

void b_mktemp(char *av[])
{
	int	i, error, errcount;
	char	*p, *filename, assign[300];
	List	l;

	l.m = NULL;
	l.n = NULL;
	errcount = 0;
	for (i=1; (p = av[i]) != NULL; i++) {
		if (*p != '-')
			filename = get_tmpfile(&error);
		else {
			filename = get_localtmp(&error);
			p++;
			}

		if (*p != 0) {
			l.w = filename;
			varassign(p, &l, FALSE);
			}

		errcount += (error != 0);
		}

	set((errcount == 0)? TRUE: FALSE);
	return;
}

#endif


#ifdef WITH_VARSUP

void b_initvars(char *av[])
{
	int	argp, setvar;
	char	*p, *var;
	List	l;

	argp = 1;
	if (av[argp] == NULL) {
		set(TRUE);
		return;
		}

	setvar = 0;
	if (strcmp(av[argp], "--") == 0)
		argp++;
	else if (strcmp(av[argp], "-s") == 0) {
		setvar = 1;
		argp++;
		}
		
	l.m = NULL;
	l.n = NULL;
	l.w = av[argp++];
	
	while (av[argp] != NULL) {
		var = av[argp++];
		if (setvar != 0  ||  varlookup(var) == NULL)
			varassign(var, &l, FALSE);
		}
		
	return;
}


void b_readvars(char *av[])
{
	int	fd, argp, c, len, asgn, asgnseen;
	int	blockmode, lines;
	char	*p, prefix[20], word[50], varname[80], *line;
	List	l;
	
	argp = 1;
	asgn = 0;
	if (av[argp] != NULL  &&  strcmp(av[argp], "-d") == 0) {
		argp++;
		if (av[argp] == NULL) {
			arg_count(av[0]);
			return;
			}

		asgn = *av[argp++];
		}
		
	copy_string(prefix, av[argp] == NULL? "rv.": av[argp++], sizeof(prefix));
	blockmode = 0;
	lines = 0;
	
	if (av[argp] == NULL)
		fd = 0;
	else if (strcmp(av[argp], "-") != 0) {
		if ((fd = open_fd(av[argp])) < 0) {
			set (FALSE);
			return;

/*		if ((fd = open(av[argp], O_RDONLY)) < 0) {
 *			fprint (2, "can't open %s\n", av[argp]);
 *			set (FALSE);
 *			return;
 */
			}
		}
	else {
		blockmode = 1;
		fd = 0;
		}
		
	set(TRUE);
	l.m = NULL;
	l.n = NULL;
	while (1) {
		if ((line = readline_fd(fd)) == NULL)
			break;
			
		p = skip_ws(line);
		if (*p == 0  ||  isalpha(*p) == 0) {
			if (blockmode  &&  lines > 0)
				break;
				
			continue;
			}

		lines++;
		p = skip_ws(line);

		get_word(&p, word, sizeof(word));
		if ((len = strlen(word)) > 1  &&
		    ((asgn == 0  &&  word[len-1] == '=')  ||  word[len-1] == asgn)) {
			word[len-1] = 0;
			noctrl(word);
			asgnseen = 1;
			}
			
		p = skip_ws(p);
		if (*p == 0)
			/* nothing */ ;
		else if ((asgn == 0  &&  *p == '=')  ||  *p == asgn) {
			p++;
			p = skip_ws(p);
			asgnseen = 1;
			}

		if (asgn != 0  &&  asgnseen == 0)
			break;

		snprint (varname, sizeof(varname) - 2, "%s%s", prefix, word);
		l.w = skip_ws(noctrl(p));
		varassign(varname, &l, FALSE);
		}
	
	if (fd != 0)
		close_fd(fd);
/*		close (fd); */

	if (blockmode != 0)
		set (lines > 0? TRUE: FALSE);

	return;
}


static void b_read(char **av)
{
	int	c, i, k, number, isifs[256];
	char	*p, *val, *line;
	List	*ifs;
	extern char *readline_fd(int fd);

	memset(isifs, 0, sizeof(isifs));
	ifs = varlookup("ifs");
	for (isifs['\0'] = TRUE; ifs != NULL; ifs = ifs->n)
		for (p = ifs->w; *p != '\0'; p++)
			isifs[*(unsigned char *) p] = TRUE;

	number = 0;
	k = 1;
	if (av[k][0] == '-'  &&  av[k][1] != 0) {
		if (av[k][1] == 'n') {
			number = 1;
			k++;
			}
		else {
			fprint(2, "unknown option\n");
			set(FALSE);
			return;
			}
		}

	if (av[k] == NULL) {
		pr_error("no variables");
		set(FALSE);
		return;
		}
	else if (av[k][0] == '-'  &&  av[k][1] == 0) {
		flush_fd(0);
		set (TRUE);
		return;
		}


	if ((line = readline_fd(0)) == NULL) {
		set(FALSE);
		return;
		}

	p = line;
	if (av[k] != NULL  &&  (number != 0  ||  av[k][1] == '-')) {
		char	*varname;
		List	*list, *last, *new;

		varname = av[k];
		if (*varname == '-')
			varname++;

		list = last = NULL;
		while ((c = *p) != 0) {
			while ((c = *p) != 0  &&  isifs[c] != 0)
				p++;

			val = p;
			while ((c = *p) != 0  &&  isifs[c] == 0)
				p++;

			if (c != 0)
				*p++ = 0;

			if (list == NULL)
				new = list = allocate(sizeof(List));
			else
				new = allocate(sizeof(List));


			new->w = ealloc(strlen(val) + 1);
			strcpy(new->w, val);

			if (last != NULL)
				last->n = new;

			last = new;
			}

		varassign(varname, list, FALSE);
		}
	else {
		List	l;

		l.m = NULL;
		l.n = NULL;

		while (av[k+1] != NULL) {
			while ((c = *p) != 0  &&  isifs[c] != 0)
				p++;

			val = p;
			while ((c = *p) != 0) {
				if (isifs[c] != 0) {
					*p++ = 0;
					break;
					}

				p++;
				}

			l.w = val;
			varassign(av[k++], &l, FALSE);
			}

		while ((c = *p) != 0  &&  isifs[c] != 0)
			p++;

		l.w = p;
		varassign(av[k], &l, FALSE);
		}

	set(TRUE);
	return;
}


static void missing_arg(char *name)
{
	fprint(2, "missing argument to %s\n", name);
	set(FALSE);
}

void b_readconf(char *av[])
{
	int	c, i, argp, fd, len, usesect, anysect, insect;
	int	echonames;
	char	*r, *p, prefix[20], word[40], varname[80], *line;
	char	section[30];
	List	l;
	
	argp = 1;
	if (av[argp] == NULL  ||  av[argp+1] == NULL) {
		missing_arg(av[0]);
		return;
		}
	
	copy_string(prefix, av[argp++], sizeof(prefix));
	if ((fd = open_fd(av[argp], O_RDONLY)) < 0) {
		set (FALSE);
		return;
		}


	argp++;
	anysect = usesect = 0;
	if (av[argp] == NULL) {
		anysect = 1;
		usesect = 1;
		}
	else if (av[argp+1] != NULL)
		usesect = 1;


	set(TRUE);
	l.m = NULL;
	l.n = NULL;

	strcpy(section, "global");
	insect = anysect;
	if (insect == 0) {
		for (i=argp; insect == 0  &&  av[i] != NULL; i++) {
			if (strcasecmp(av[i], section) == 0)
				insect = 1;
			}
		}
		
	while (1) {
		if ((line = readline_fd(fd)) == NULL)
			break;
			
		p = skip_ws(line);
		if (*p == 0  ||  *p == '#')
			continue;
		else if (*p == '[') {
			p++;
			get_word(&p, section, sizeof(section));
			if ((len = strlen(section)) > 1  &&  section[len-1] == ']')
				section[len-1] = 0;

			if (anysect == 0) {
				insect = 0;
				for (i=argp; insect == 0  &&  av[i] != NULL; i++) {
					if (strcasecmp(av[i], section) == 0)
						insect = 1;
					}
				}

			continue;
			}
		else if (insect == 0)
			continue;
		else if (isalpha(*p) == 0)
			continue;


		if ((r = strchr(p, '=')) == NULL)
			continue;	/* no assignment */

		*r++ = 0;

		get_word(&p, word, sizeof(word));
		if (*word == 0)		/* no variable name */
			continue;

		if (usesect == 0)
			snprint (varname, sizeof(varname) - 2, "%s%s", prefix, word);
		else
			snprint (varname, sizeof(varname) - 2, "%s%s.%s", prefix, section, word);
		
		l.w = skip_ws(noctrl(r));
		varassign(varname, &l, FALSE);
		}
	
	close_fd(fd);
	return;
}


static List *append_opt(List **first, List *here, int opt, char *arg)
{
	char	*p;
	List	*l;
	
	if (opt != 0) {
		p = allocate(3);
		p[0] = '-';
		p[1] = opt;
		p[2] = 0;
		
		l = allocate(sizeof(List));
		l->w = p;
		if (here != NULL)
			here->n = l;

		here = l;
		if (*first == NULL)
			*first = here;
		}

	if (arg != NULL) {
		p = allocate(strlen(arg) + 1);
		strcpy(p, arg);

		l = allocate(sizeof(List));
		l->w = p;
		if (here != NULL)
			here->n = l;

		here = l;
		if (*first == NULL)
			*first = here;
		}

	return (here);
}

static void b_parseopt(char *av[])
{
	int	c, i, k, argc, isopt[256];
	char	**r, *p, *name;
	List	*val, *last;

	set(TRUE);
	argc = 0;
	for (argc = 0; av[argc] != NULL; argc++)
		;

	k = 1;
	if (argc < 3) {
		fprint(2, "parseopt: need more arguments");
		set(FALSE);
		return;
		}

	name = av[k++];

	p = av[k++];
	memset(isopt, 0, sizeof(isopt));
	for (i=0; (c = p[i]) != 0; i++) {
		isopt[c] = 1;
		if (p[i+1] == ':') {
			isopt[c] = 2;
			i++;
			}
		}


	val  = NULL;
	last = NULL;
	if (name[0] == '*'  &&  name[1] == '\0') {
		List	*dollarzero;

		dollarzero = varlookup("0");
		last = append_opt(&val, last, 0, (dollarzero != NULL)? dollarzero->w: "");
		}

	while (k < argc  &&  av[k][0] == '-'  &&
	       strcmp(av[k], "-") != 0  &&  strcmp(av[k], "--") != 0) {
		p = av[k++];
		for (i=1; (c = p[i]) != 0; i++) {
			if (isopt[c] == 0) {
				fprint(2, "unknown option: -%c\n", c);
				set(FALSE);
				return;
				}
			else if (isopt[c] == 2) {
				if (k >= argc) {
					fprint(2, "missing argument: option -%c\n", c);
					set(FALSE);
					return;
					}

				last = append_opt(&val, last, c, av[k++]);
				}
			else
				last = append_opt(&val, last, c, NULL);
			}
		}

	last = append_opt(&val, last, 0, "--");
	if (k < argc  &&  strcmp(av[k], "--") == 0)
		k++;	/* skip this ``--'' */

	while (k < argc)
		last = append_opt(&val, last, 0, av[k++]);

	varassign(name, val, 0);
	return;
}

#endif



#ifdef WITH_LOCKING

void b_lock(char **av)
{
	int	c, i, k, wait, stale;
	char	opt[80], lockfile[200];

	wait  = 30;
	stale = 300;
	
	k = 1;
	set(TRUE);

	while (av[k] != NULL  &&  *av[k] == '-') {
		copy_string(opt, av[k++], sizeof(opt));
		for (i=1; (c = opt[i]) != 0; i++) {
			if (c != 's'  &&  c != 'w') {
				fprint(2, "lock: unknown option: %c\n", c);
				set(FALSE);
				return;
				}
			else if (av[k] == NULL) {
				fprint(2, "lock: missing argument: %c\n", c);
				set(FALSE);
				return;
				}
			
			if (c == 's')
				stale = a2u(av[k++]);
			else
				wait = a2u(av[k++]);
			}
		}

	if (av[k] == NULL) {
		fprint(2, "lock: missing filename\n");
		set(FALSE);
		return;
		}

	snprint (lockfile, sizeof(lockfile) - 2, "%s.lock", av[k++]);
	if (av[k] != NULL) {
		arg_count("lock");
		return;
		}
		
	if (create_lock(lockfile, wait, stale) != 0)
		set(FALSE);
		
	return;
}

void b_unlock(char **av)
{
	int	k;
	char	lockfile[200];
	
	set(TRUE);
	for (k=1; av[k] != NULL; k++) {
		snprint (lockfile, sizeof(lockfile) - 2, "%s.lock", av[k]);
		if (remove_lock(lockfile) != 0)
			set(FALSE);
		}
		
	return;
}

#endif


#ifdef WITH_EXPR

	/* If we include rc.h in expr.c for the List type we get
	 * prototype errors back for some functions from <unistd.h>.
	 * For this reason we define some helper functions.
	 */

int h_getcellcount(char *name)
{
	List	*l;

	if ((l = varlookup(name)) == 0)
		return (0);

	return (listnel(l));
}

static char *getsetcell(char *name, int pos, char *value)
{
	int	index;
	List	*l;

	index = pos;
	if (pos < 0) {
		fprint (2, "expr: index not allowed: %s[%d]\n", name, pos);
		return ("");
		}
		
	l = varlookup(name);
	if (l == NULL) {
		fprint (2, "expr: variable not found: %s\n", name);
		return ("");
		}
		
	while (pos > 1  &&  l != NULL) {
		l = l->n;
		pos--;
		}

	if (l == NULL) {
		fprint (2, "expr: variable index error: %s[%d]\n", name, index);
		return ("");	/* No jmp_buf available here. */
		}

	if (value == NULL) {
		if (l->w == NULL)
			return ("");

		return (l->w);
		}
	else {
		efree(l->w);
		if (l->m != NULL)
			efree(l->m);

		l->w = ealloc(strlen(value) + 1);
		strcpy(l->w, value);

		return (l->w);
		}

	return ("");
}

char *h_exprasgn(char *name, int index, char *value)
{
	List	l;
	
	if (index > 0)
		getsetcell(name, index, value);
	else {
		l.m = NULL;
		l.n = NULL;
		l.w = value;
		varassign(name, &l, FALSE);
		}

	return (0);
}

char *h_convert_list_to_string(List *l)
{
	int	c, i, k;
	static int size  = 0;
	static char *val = NULL;
	
	if (val == NULL  ||  size == 0)
		val = reallocate(val, size += 1024);

	k = 0;
	while (l != NULL) {
		if (k > 0)
			val[k++] = ' ';

		for (i=0; (c = l->w[i]) != 0; i++) {
			if (k + 4 >= size)
				val = reallocate(val, size += 512);

			val[k++] = c;
			}

		l = l->n;
		}

	val[k] = 0;
	return (val);
}

char *h_exprgetvar(char *name, int index)
{
	char	*val;
	List	*l;
	
	if (index > 0)
		val = getsetcell(name, index, NULL);
	else {
		l = varlookup(name);
		val = h_convert_list_to_string(l);
		}

	return (val);
}

char *h_glob(char *pattern)
{
	char	*r, *s, *val;
	List	*l, *g;

	l = ealloc(sizeof(List));
	l->n = NULL;
	l->w = ealloc(strlen(pattern) + 1);
	strcpy(l->w, pattern);

	l->m = ealloc(strlen(l->w) + 1);
	for (r = l->w, s = l->m; *r != 0; s++, r++) {
		*s = (*r == '*'  ||  *r == '['  ||  *r == '?');
		}

	g = glob(l);
	val = h_convert_list_to_string(g);

	listfree(l);
	return (val);
}

	/* Now the real expr builtin.
	 */

void b_expr(char *av[])
{
	int	c, i, k, echo, newlines, rc;
	char	opt[80], *result;

	k = 1;
	set(TRUE);

	rc = 0;
	echo = strcmp(av[0], "expr") == 0? 1: 0;
	newlines = 1;
	
	while (av[k] != NULL  &&  *av[k] == '-') {
		copy_string(opt, av[k++], sizeof(opt));
		for (i=1; (c = opt[i]) != 0; i++) {
			if (c == 'e')
				echo = 1;
			else if (c == 'n')
				newlines = 0;
			else {
				fprint(2, "%s: unknown option: %-c\n", av[0], c);
				set(FALSE);
				return;
				}
			}
		}

	for (i=k; av[i] != NULL; i++) {
		rc = do_expr(av[i], &result);
		if (rc == -1) {
			fprint (2, "expression error: %s\n", av[i]);
			continue;
			}

		if (echo)
			fprint (1, "%s%c", result, (newlines != 0)? '\n': ' ');
		}

	if (newlines == 0)
		fprint (1, "\n");

	set(rc != 0? TRUE: FALSE);
	return;
}

#endif


#ifdef WITH_MISC


#endif

