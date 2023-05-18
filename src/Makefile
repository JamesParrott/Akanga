# Makefile for rc.

# What are the binary files?
TARGETS = akanga

# Please check the configuration parameters in config.h (and if you want
# to make sure, the definitions in proto.h) to make sure they are correct
# for your system.

SHELL=/bin/sh

# Uncomment this line if you have defined the NOEXECVE macro in config.h
#EXECVE=execve.o

# Define this macro if you wish to extend rc via locally-defined builtins.
# An interface is provided in addon.[ch]. Note that the author does not
# endorse any such extensions, rather hopes that this way rc will become
# useful to more people.
#ADDON=addon.o

# Use an ANSI compiler (or at least one that groks prototypes and void *):
CC=gcc
##CFLAGS=-ggdb -DWITH_LOCKING -DWITH_TMPFILE -DWITH_VARSUP -O2 -fomit-frame-pointer -DSYSVR4
CFLAGS = -ggdb -DWITH_LOCKING -DWITH_TMPFILE -DWITH_VARSUP -DWITH_EXPR -DWITH_MISC -O2 -DSYSVR4
LDFLAGS = libedit.a

# You may substitute "bison -y" for yacc. (You want to choose the one that
# makes a smaller y.tab.c. Also see the README about Sun's yacc.)
YACC=bison -y

OBJS=$(ADDON) builtins.o except.o exec.o $(EXECVE) fn.o footobar.o getopt.o \
	glob.o glom.o hash.o heredoc.o input.o lex.o list.o main.o match.o \
	nalloc.o open.o print.o redir.o sigmsgs.o signal.o status.o tree.o \
	strlib.o lock.o tmpfile.o expr.o fileio.o \
	utils.o var.o version.o wait.o walk.o which.o y.tab.o

# If rc is compiled with READLINE defined, you must supply the correct
# arguments to ld on this line. Typically this would be something like:
#
#	$(CC) -o $@ $(OBJS) -lreadline -ltermcap

akanga:		$(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)
	ctags *.[ch]


sigmsgs.c: mksignal
	sh mksignal /usr/include/asm/signal.h

y.tab.c: parse.y
	$(YACC) -d parse.y

config.h: config.h-dist
	cp config.h-dist config.h

#
# trip not available -- there is no rc now.
#
#trip:		rc
#	./rc -p < trip.rc
#

history: force
	cd history; make CC="$(CC)" $(HISTORYMAKEFLAGS)

clean: force
	rm -f *.o *.tab.* $(TARGETS) tags

force:

# dependencies:

$(OBJS): config.h
sigmsgs.h: sigmsgs.c
lex.o y.tab.o: y.tab.c
builtins.c fn.c status.c hash.c: sigmsgs.h


