
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


void *allocate(size_t size)
{
	void	*p;

	if ((p = malloc(size)) == NULL) {
		fprint (2, "memory allocation error\n");
		exit (-1);
		}

	memset(p, 0, size);
	return (p);
}

void *reallocate(void *p, size_t size)
{
	if ((p = realloc(p, size)) == NULL) {
		fprint (2, "memory allocation error\n");
		exit (-1);
		}

	return (p);
}


char *skip_ws(char *string)
{
	unsigned int c;

	while ((c = *string) == ' '  ||  c == '\t')
		string++;

	return (string);
}

char *noctrl(char *buffer)
{
	int	len, i;
	unsigned char *p;

	if ((p = buffer) == NULL)
		return (NULL);

	len = strlen(p);
        for (i=len-1; i>=0; i--) {
		if (p[i] <= 32)
			p[i] = '\0';
		else
			break;
		}

	return (p);
}

char *get_word(char **from, char *to, int maxlen)
{
	unsigned int c;
	unsigned char *p;
	int	k;

	maxlen -= 2;
	while ((c = **from) != 0  &&  c <= 32)
		*from += 1;

	*(p = to) = k = 0;
	while ((c = **from) != 0) {
		if (c == ' '  ||  c == '\t'  ||  c < 32)
			break;

		*from += 1;
		if (k < maxlen)
			p[k++] = c;
		}

	p[k] = 0;
	return (to);
}

char *copy_string(char *y, char *x, int len)
{
	x = skip_ws(x);
	noctrl(x);

	len -= 2;
	if (strlen(x) >= len)
		x[len] = 0;

	if (y != x)
		strcpy(y, x);
		
	return (y);
}



int _xx_getc_fd(int fd)
{
	int	c, n;
	static int here = 0, len = 0, bufsize = 0;
	static char *buffer = NULL;

	if (buffer == NULL)
		buffer = allocate(bufsize = 4096 + 2);

	if (here >= len) {
		len = here = 0;
again:
		n = rc_read(fd, buffer, bufsize - 2);
		if (n == 0)
			return (-1);
		else if (n < 0) {
			if (errno == EINTR)
				goto again;

			return (-1);
			}

		len  = n;
		here = 0;
		}

	if (here >= len)
		return (-1);

	c = (unsigned char) buffer[here++];
	return (c);
}

char *_xx_readline_fd(int fd)
{
	int	c, i, k;
	static int linesize = 0;
	static char *line = NULL;

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



int append_file(filelist_t **list, char *filename)
{
	filelist_t *x;

	x = allocate(sizeof(filelist_t));
	copy_string(x->filename, filename, sizeof(x->filename));
	x->next = *list;
	*list = x;

	return (0);
}

int erase_filelist(filelist_t *list)
{
	filelist_t *this, *next;

	this = list;
	while (this != NULL) {
		next = this->next;
		free(this);

		this = next;
		}

	return (0);
}

int remove_filelist(filelist_t *list)
{
	filelist_t *this, *next;
	struct stat sbuf;

	this = list;
	while (this != NULL) {
		next = this->next;
		if (*this->filename == 0)
			/* nothing */ ;
		else if (stat(this->filename, &sbuf) != 0)
			/* nothing */ ;
		else if (unlink(this->filename) != 0)
			fprint (2, "can't unlink %s\n", this->filename);

		free(this);
		this = next;
		}

	return (0);
}


long a2l(char *from, char **ptr, int base)
{
	int	c, i, sign;
	long	val;
	static char odigits[] = "0123456789ABCDDEFGHIJKLMNOQRSTUVWXYZ";
	static char ldigits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	
	sign = 0;
	val  = 0;
	if (base > strlen(odigits))
		base = strlen(odigits);

	while ((c = *from) == '+'  ||  c == '-') {
		from++;
		if (c == '-')
			sign = 1 - sign;
		}

	while ((c = *from) != 0) {
		for (i=0; i<base; i++) {
			if (c == odigits[i]  ||  c == ldigits[i])
				break;
			}

		if (i >= base)
			break;

		from++;
		val = val * base + i;
		}
		
	if (sign != 0)
		val = -val;

	if (ptr != NULL)
		*ptr = from;

	return (val);
}

