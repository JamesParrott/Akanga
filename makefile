# Makefile for rc.

# This is for creating a new release tar.gz archive with ``make tar''.
TAR = akanga-1.0.7
DIR = akanga-1.0.7


TARGETS =	bin/akanga


all:		bin/akanga

bin/akanga:
	cd src/editline;  make  &&  cp libedit.a ..
	cd src;  make  &&  cp akanga ../bin
	strip bin/akanga

install:	bin/akanga
	@bin/akanga copyfiles

clean:
	cd src; make clean
	cd src/editline; make clean
	-rm -f $(TARGETS)

tar:
	cd src; make clean
	cd src/editline; make clean
	-rm -f $(TAR).tar.gz $(TARGETS) doc/.mancc doc/*.html
	cd ..; tar cvf $(TAR).tar $(DIR); gzip $(TAR).tar
	mv ../$(TAR).tar.gz .

