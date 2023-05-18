
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


#ifdef WITH_TMPFILE

#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#include "strlib.h"

#define	DEBUG(x)


static struct _tmpfile {
    char	filename[200];
    struct _tmpfile *next;
    } *lasttmp = NULL;

static int count =		0;
static int pid =		0;

static char prefix[200] =	"";


static char *get_prefix()
{
	char	*r, *p;
	extern char *varlookup_string();

	*prefix = 0;
	r = varlookup_string("tmpprefix");
	if (r == NULL  ||  (p = strchr(r, '=')) == NULL)
		return (prefix);
	
	copy_string(prefix, &p[1], sizeof(prefix));
	return (prefix);
}

static unlink_existing(char *filename)
{
	struct stat sbuf;

	if (stat(filename, &sbuf) == 0) {
		if (unlink(filename) != 0)
			return (1);
		}
	
	return (0);
}


static void cleanup(void)
{
	int	i;
	struct stat sbuf;
	struct _tmpfile *tmp;

	if (pid == 0  ||  pid != getpid())
		return;

	tmp = lasttmp;
	while (tmp != NULL) {
		if (stat(tmp->filename, &sbuf) == 0) {
			unlink(tmp->filename);
			}

		/* cleanup() is called on program termination
		 * so we do not need to free the memory.
		 */

		tmp = tmp->next;
		}

	return;
}

int init_tmplist()
{
	struct _tmpfile *tmp, *next;

	if (pid == 0) {
		pid = getpid();
		atexit(cleanup);
		}
	else if (pid != getpid()) {
DEBUG( fprint (2, "%d: erasing parent's tmpfile list\n", getpid()); )
		/*
		 * we are in a child process - let's erase the parent's
		 * tempfile list and create our own.
		 */

		pid     = getpid();
		count   = 0;

		tmp = lasttmp;
		while (tmp != NULL) {
			next = tmp->next;
			free(tmp);

			tmp = next;
			}
			
		lasttmp = NULL;
		if (atexit(cleanup) != 0)
			fprint (2, "%d: can't register cleaup()\n", getpid());
		}
		
	return (0);
}

char *get_tmpfile(int *error)
{
	int	k;
	struct _tmpfile *tmp;

	init_tmplist();
	if ((tmp = malloc(sizeof(struct _tmpfile))) == NULL) {
		fprint (2, "memory allocation error\n");
		exit (-1);
		}
		
/*	sprint (tmp->filename, "/tmp/%05d.%03d.tmp", pid, count++); */
	get_prefix();
	snprint (tmp->filename, sizeof(tmp->filename) - 2, "/tmp/%s%05d.%03d.tmp",
			prefix, pid, count++);
	
	tmp->next = lasttmp;
	lasttmp = tmp;

	*error = unlink_existing(tmp->filename);
	return (tmp->filename);
}

char *get_localtmp(int *error)
{
	int	k;
	char	pwd[200];
	struct _tmpfile *tmp, *next;

	init_tmplist();
	if ((tmp = malloc(sizeof(struct _tmpfile))) == NULL) {
		fprint (2, "memory allocation error\n");
		exit (-1);
		}

	getcwd(pwd, sizeof(pwd) - 2);
	get_prefix();
	snprint (tmp->filename, sizeof(tmp->filename) - 2, "%s/%s%05d.%03d.tmp",
			pwd, prefix, pid, count++);

	tmp->next = lasttmp;
	lasttmp = tmp;

	*error = unlink_existing(tmp->filename);
	return (tmp->filename);
}




#endif


