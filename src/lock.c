
/*
 *  Copyright (C) 1999  Wolfgang Zekoll  <wzk@quietsche-entchen.de.de>
 *
 *  This software is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#ifdef WITH_LOCKING


#include <string.h>

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "strlib.h"


#define	LOCK_STALEAGE		300

static int pid =		0;
filelist_t *locklist =		NULL;


void clean_locklist(void)
{
	if (pid == 0  ||  pid != getpid())
		return;

	remove_filelist(locklist);
	return;
}

init_locking()
{
	if (pid == 0) {
		pid = getpid();
		atexit(clean_locklist);
		}
	else if (pid != getpid()) {
		erase_filelist(locklist);
		pid = getpid();
		}

	return (0);
}

int create_lock(char *lockfile, int wait, int stale)
{
	int	fno, delay, stale_tested;
	unsigned long now;
	char	buffer[500];

	init_locking();
	stale_tested = 0;
	now = time(NULL);
/*	snprintf (buffer, sizeof(buffer) - 2, "%lu %u\n", now, getpid()); */
	snprint (buffer, sizeof(buffer) - 2, "%d %d\n", now, getpid());
	
again:
	if ((fno = open(lockfile, O_CREAT | O_EXCL | O_WRONLY, 0666)) >= 0)
		goto end;


	if (errno != EEXIST) {
		fprint (2, "can't create lock %s: errno= %d\n", lockfile, errno);
		return (-2);
		}
		
	if (stale_tested == 0) {
		struct stat sbuf;
		unsigned long mtime;

		mtime = (stat(lockfile, &sbuf) == 0)? sbuf.st_mtime: 0;
		if ((now - mtime) >= stale) {
			fprint (2, "found stale lock %s, age= %d\n", lockfile, now - mtime);
			if (unlink(lockfile) != 0)
				fprint (2, "can't remove stale lock %s\n", lockfile);
			else
				goto again;
			}

		stale_tested = 1;
		}

	if (wait == 0) {
		return (-1);
		}

	delay = 0;
	while ((fno = open(lockfile, O_CREAT | O_EXCL | O_WRONLY, 0666)) == -1  &&  errno == EEXIST) {
		if (delay > wait)
			break;
			
		sleep(2);
		delay = delay + 2;
		}

	if (fno == -1) {
		fprint (1, "can't create lock %s: errno= %d\n", lockfile, errno);
		return (-2);
		}

end:
	write(fno, buffer, strlen(buffer));
	close(fno);
	append_file(&locklist, lockfile);
	
	return (0);
}

remove_lock(char *filename)
{
	filelist_t *this;
	struct stat sbuf;
	
	init_locking();		
	this = locklist;

	while (this != NULL) {
		if (*this->filename == 0)
			/* nothing */ ;
		else if (strcmp(this->filename, filename) == 0) {
			*this->filename = 0;
			if (stat(filename, &sbuf) != 0) {
				fprint (2, "no lock present: %s\n", filename);
				return (-1);
				}
			else if (unlink(filename) != 0) {
				fprint (2, "can't remove file: %s\n", filename);
				return (-1);
				}

			return (0);
			}

		this = this->next;
		}

	fprint (2, "lockfile not found: %s\n", filename);
	return (-2);
}


#endif

