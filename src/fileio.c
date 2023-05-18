
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


#include <stdlib.h>
#include <string.h>

#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "strlib.h"


#define	MAX_FD		256		/* Maximum number of files */
#define	FD_BUFSIZE	(4096 + 2)	/* Buffer size for file input */

typedef struct _fd {
    char	*buffer;
    int		here, len;
    } fd_t;

static fd_t *fdesc = NULL;


int close_fd(int fd)
{
	if (fdesc == NULL)
		fdesc = allocate(MAX_FD * sizeof(fd_t));

	if (fdesc[fd].buffer != NULL) {
		free(fdesc[fd].buffer);
		fdesc[fd].buffer = NULL;
		}

	return (close(fd));
}

int init_fd(int fd)
{
	if (fdesc[fd].buffer == NULL)
		fdesc[fd].buffer = allocate(FD_BUFSIZE + 2);

	fdesc[fd].len  = 0;
	fdesc[fd].here = 0;

	return (0);
}

int open_fd(char *filename)
{
	int	fd;

	if (fdesc == NULL)
		fdesc = allocate(MAX_FD * sizeof(fd_t));

	if ((fd = open(filename, O_RDONLY)) < 0) {
		fprint (2, "can't open file: %s\n", filename);
		return (-1);
		}

	init_fd(fd);
	return (fd);
}

int getc_fd(int fd)
{
	int	c, n;
	fd_t	*fdp;

	fdp = &fdesc[fd];
	if (fdp->here >= fdp->len) {
		fdp->len = fdp->here = 0;
again:
		n = rc_read(fd, fdp->buffer, FD_BUFSIZE - 2);
		if (n == 0)
			return (-1);
		else if (n < 0) {
			if (errno == EINTR)
				goto again;

			return (-1);
			}

		fdp->len  = n;
		fdp->here = 0;
		}

	if (fdp->here >= fdp->len)
		return (-1);

	c = (unsigned char) fdp->buffer[fdp->here++];
	return (c);
}

char *readline_fd(int fd)
{
	int	c, i, k;
	static int linesize = 0;
	static char *line = NULL;

	if (fdesc == NULL)
		fdesc = allocate(MAX_FD * sizeof(fd_t));

	if (fdesc[fd].buffer == 0)
		init_fd(0);
		

	if (line == NULL)
		line = realloc(line, (linesize = 2048));

	c = getc_fd(fd);
	if (c < 0)
		return (NULL);

	k = 0;
	while (c > 0  &&  c != '\n'  &&  c != 0) {
		if (k >= linesize)
			line = realloc(line, (linesize += 2048));

		line[k++] = c;
		c = getc_fd(fd);
		}

	line[k] = 0;
	for (i=k-1; i>=0; i--) {
		if ((c = line[i]) != 0  &&  c <= ' ')
			line[i] = 0;
		else
			break;
		}
		
	return (line);
}

int flush_fd(int fd)
{
	int	n;
	fd_t	*fdp;

	if (fdesc == NULL)
		fdesc = allocate(MAX_FD * sizeof(fd_t));

	fdp = &fdesc[fd];
	if (fdp->here < fdp->len) {
		n = fdp->len - fdp->here;
		write(1, &fdp->buffer[fdp->here], n);
		}
	
	while ((n = read(fd, fdp->buffer, FD_BUFSIZE)) > 0)
		write(1, fdp->buffer, n);
	
	return (0);
}

