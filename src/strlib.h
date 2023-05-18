
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

#ifndef _STRLIB_DEFINED
#define	_STRLIB_DEFINED

typedef struct _filelist {
    char	filename[200];
    struct _filelist *next;
    } filelist_t;


void *allocate(size_t size);
void *reallocate(void *ptr, size_t size);


char *skip_ws(char *string);
char *noctrl(char *string);
char *get_word(char **ptr, char *word, int size);
char *copy_string(char *y, char *x, int size);

char *readline_fd(int fd);

int append_file(filelist_t **list, char *filename);
int erase_filelist(filelist_t *list);
int remove_filelist(filelist_t *list);

long a2l(char *from, char **ptr, int base);

#endif
