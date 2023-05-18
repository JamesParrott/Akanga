#include "sigmsgs.h"

Sigmsgs signals[] = {
	{"",		""},
	{"sighup",	"hangup"},
	{"sigint",	""},
	{"sigquit",	"quit"},
	{"sigill",	""},
	{"sigtrap",	""},
	{"sigabrt",	""},
	{"sigbus",	""},
	{"sigfpe",	""},
	{"sigkill",	"killed"},
	{"sigusr1",	""},
	{"sigsegv",	""},
	{"sigusr2",	""},
	{"sigpipe",	""},
	{"sigalrm",	""},
	{"sigterm",	"terminated"},
	{"sigstkflt",	""},
	{"sigchld",	"child stop or exit"},
	{"sigcont",	"continue"},
	{"sigstop",	"stop signal not from tty"},
	{"sigtstp",	"stopped"},
	{"sigttin",	"background tty read"},
	{"sigttou",	"background tty write"},
	{"sigurg",	"urgent condition on i/o channel"},
	{"sigxcpu",	""},
	{"sigxfsz",	""},
	{"sigvtalrm",	""},
	{"sigprof",	""},
	{"sigwinch",	""},
	{"sigio",	""},
	{"sigpwr",	""},
	{"sigunused",	""},
};
