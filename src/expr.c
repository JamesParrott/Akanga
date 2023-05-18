
/*
 *  Copyright (C) 1999  Wolfgang Zekoll  <wzk@quietsche-entchen.de>
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

#ifdef WITH_EXPR


#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "expr.h"

#define	DEBUG(x)


	/* Functions for stack manipulation.
	 */

static int clear_stack(vm_t *vm)
{
	vm->stack.sp   = 0;
	vm->stack.heap = 0;
	return (0);
}

static int drop_tos(vm_t *vm)
{
	cell_t	*cell;
	
	cell = &vm->stack.cell[vm->stack.sp--];
	vm->stack.heap = cell->heap;

	return (0);
}

static int push_number(vm_t *vm, number_t val)
{
	int	size;
	cell_t	*cell;

	cell = &vm->stack.cell[++vm->stack.sp];
	cell->type = NUMBER;
	cell->heap = vm->stack.heap;
	cell->u.val = val;

	return (0);
}

static number_t pop_number(vm_t *vm)
{
	number_t val;
	cell_t	*cell;
	
	cell = &vm->stack.cell[vm->stack.sp--];
	vm->stack.heap = cell->heap;

	if (cell->type == NUMBER)
		val = cell->u.val;
	else if (cell->type == STRING)
		val = atol(cell->u.str.ptr);
	else
		val = 0;

	return (val);
}

static int push_string(vm_t *vm, char *string, int len)
{
	int	len0;
	cell_t	*cell;
	
	if (len <= 0)
		len = strlen(string);

	len0 = len  + 1;			/* abschliessendes '\0' */
	len0 = len0 + (len0 & 1);		/* Laenge ist immer eine gerade Zahl */

	if (vm->stack.heap + len0 >= vm->stack.max) {
		vm->stack.max += len + 4096;
		vm->stack.mem = reallocate(vm->stack.mem, vm->stack.max);
		}

	cell = &vm->stack.cell[++vm->stack.sp];
	cell->type = STRING;
	cell->heap = vm->stack.heap;
	cell->u.str.len = len;
	cell->u.str.ptr = &vm->stack.mem[vm->stack.heap];
	
	strcpy(cell->u.str.ptr, string);
	vm->stack.heap += len0;

	return (0);
}

static char *pop_string(vm_t *vm, char *string, int size)
{
	int	c, i, k;
	char	*p;
	cell_t	*cell;

	size = size - 2;
	cell = &vm->stack.cell[vm->stack.sp--];
	vm->stack.heap = cell->heap;
	
	if (cell->type == NUMBER) {
#ifdef LONG_ONLY
		snprint (string, sizeof(string) - 2, "%ld", (long) cell->u.val);
#else
		if ((long) cell->u.val == cell->u.val)
			snprint (string, sizeof(string) - 2, "%ld", (long) cell->u.val);
		else if ((unsigned long) cell->u.val == cell->u.val)
			snprint (string, sizeof(string) - 2, "%lu", (unsigned long) cell->u.val);
		else
			snprint (string, sizeof(string) - 2, "%.6lg", cell->u.val);
#endif
		}
	else if (cell->type == STRING) {
		p = cell->u.str.ptr;
		for (i=0; i < size  &&  (c = p[i]) != 0; i++)
			string[i] = c;

		string[i] = 0;
		}
	else
		*string = 0;

	return (string);
}

static char *pop_stringptr(vm_t *vm)
{
	char	*string;
	cell_t	*cell;
	static char buffer[40];

	cell = &vm->stack.cell[vm->stack.sp--];
	vm->stack.heap = cell->heap;
	
	if (cell->type == STRING)
		string = cell->u.str.ptr;
	else if (cell->type == NUMBER) {
#ifdef LONG_ONLY
		snprint (string = buffer, sizeof(buffer) - 2, "%ld", (long) cell->u.val);
#else
		if ((long) cell->u.val == cell->u.val)
			snprint (string = buffer, sizeof(buffer) - 2, "%ld", (long) cell->u.val);
		else if ((unsigned long) cell->u.val == cell->u.val)
			snprint (string = buffer, sizeof(buffer) - 2, "%lu", (unsigned long) cell->u.val);
		else
			snprint (string = buffer, sizeof(buffer) - 2, "%.6lg", cell->u.val);
#endif
		}
	else
		string = "";

	return (string);
}

static int push_varptr(vm_t *vm, void *var)
{
	cell_t	*cell;

	cell = &vm->stack.cell[++vm->stack.sp];
	cell->type = VARPTR;
	cell->heap = vm->stack.heap;
	cell->u.var = var;

	return (0);
}

static void *pop_varptr(vm_t *vm)
{
	void	*var;
	cell_t	*cell;
	
	cell = &vm->stack.cell[vm->stack.sp--];
	vm->stack.heap = cell->heap;

	if (cell->type == VARPTR)
		var = cell->u.var;
	else
		var = NULL;

	return (var);
}

static int push_ptr(vm_t *vm, void *ptr)
{
	cell_t	*cell;

	cell = &vm->stack.cell[++vm->stack.sp];
	cell->type = PTR;
	cell->heap = vm->stack.heap;
	cell->u.ptr = ptr;

	return (0);
}

static void *pop_ptr(vm_t *vm)
{
	void	*ptr;
	cell_t	*cell;
	
	cell = &vm->stack.cell[vm->stack.sp--];
	vm->stack.heap = cell->heap;

	if (cell->type == PTR)
		ptr = cell->u.ptr;
	else
		ptr = NULL;

	return (ptr);
}

static char *convert_cell_to_string(cell_t *cell)
{
	number_t val;
	static char buffer[40];

	if (cell->type == STRING)
		return (cell->u.str.ptr);
	else if (cell->type == NUMBER) {
#ifdef LONG_ONLY
		snprint (buffer, sizeof(buffer) - 2, "%ld", (long) cell->u.val);
#else
		if ((long) cell->u.val == cell->u.val)
			snprint (buffer, sizeof(buffer) - 2, "%ld", (long) cell->u.val);
		else if ((unsigned long) cell->u.val == cell->u.val)
			snprint (buffer, sizeof(buffer) - 2, "%lu", (unsigned long) cell->u.val);
		else
			snprint (buffer, sizeof(buffer) - 2, "%.6lg", cell->u.val);
#endif			
		return (buffer);
		}
	else
		return ("");
	
	return ("");
}

static char *convert_tos_to_string(vm_t *vm)
{
	char	*p;
	cell_t	*cell;
	
	cell = &vm->stack.cell[vm->stack.sp];
	p = convert_cell_to_string(cell);

	return (p);
}


	/* String buffer operations.
	 */

static char *setstring(vm_t *vm, char *string, int len)
{
	if (len < 0)
		len = strlen(string);

	len = len;			/* add one for the trailing '\0' */
	if (len + 4 >= vm->buffer.max)
		vm->buffer.ptr = reallocate(vm->buffer.ptr, vm->buffer.max = (len + 512));

	strcpy(vm->buffer.ptr, string);
	vm->buffer.here = len;		/* point to the end of the string */
	
	return (vm->buffer.ptr);
}

static char *appendstring(vm_t *vm, char *string, int len)
{
	if (len < 0)
		len = strlen(string);

	len = len;
	if (vm->buffer.here + len + 4 >= vm->buffer.max)
		vm->buffer.ptr = reallocate(vm->buffer.ptr, vm->buffer.max += (len + 512));

	strcpy(&vm->buffer.ptr[vm->buffer.here], string);
	vm->buffer.here += len;

	return (vm->buffer.ptr);
}



	/* Error handler.
	 */

static int error(vm_t *vm, symbol_t *this, char *message, char **here)
{
	fprint (2, "%s\n", message);
	longjmp(vm->onerror, 1);

	return (0);
}


	/* Opcode tables for operators and functions.
	 */

static opcode_t optab[] = {
    "=",	VAR_ASSIGN,

    "+",	NUM_PLUS,
    "-",	NUM_MINUS,
    "*",	NUM_MUL,
    "/",	NUM_DIV,
    "%",	NUM_MOD,
    
    ">",	CMP_GT,
    ">=",	CMP_GE,
    "==",	CMP_EQ,
    "!=",	CMP_NE,
    "<=",	CMP_LE,
    "<",	CMP_LT,
    
    "^",	STR_CAT,
    "^~",	STR_MATCH,
    "^==",	CMP_LEQ,
    "^!=",	CMP_LNE,
    "^>",	CMP_LGT,
    "^>=",	CMP_LGE,
    "^<=",	CMP_LLE,
    "^<",	CMP_LLT,

    "&&",	LOG_AND,
    "||",	LOG_OR,
    "!",	LOG_NOT,
    
    ",",	DROP_TOS,
    "?",	EXPR_IFTHEN,
    ":",	EXPR_ELSE,
    
    "<<",	SHIFT_LEFT,
    ">>",	SHIFT_RIGHT,
    "&",	BIN_AND,
    "|",	BIN_OR,
    "~|",	BIN_XOR,
    "~",	BIN_INV,
    
    "#",	ARRAY_LEN,

    "",		0
    };
    
static opcode_t fntab[] = {
    "argc",	FN_ARGC,
    "argv",	FN_ARGV,
    "date",	FN_STRFTIME,
    "filesize",	FN_SIZE,
    "glob",	FN_GLOB,
    "index",	FN_INDEX,
    "rindex",	FN_RINDEX,
    "seq",	FN_SEQ,
    "stat",	FN_STAT,
    "substr",	FN_SUBSTR,
    "strftime",	FN_STRFTIME,
    "strlen",	FN_STRLEN,
    "systime",	FN_SYSTIME,
    "test",	FN_TEST,
    "",		0
    };

static int get_opcode(opcode_t *opctab, char *sym)
{
	int	i;

	for (i=0; opctab[i].sym[0] != 0; i++) {
		if (strcmp(opctab[i].sym, sym) == 0)
			return (opctab[i].opcode);
		}

	return (-1);
}


	/* Return the next symbol from the input.
	 */

static symbol_t *alloc_symbol()
{
	symbol_t *sym;

	sym = allocate(sizeof(symbol_t));
	memset(sym, 0, sizeof(symbol_t));

	return (sym);
}

static int free_symbol(symbol_t *sym)
{
	if (sym->buffer != NULL)
		free(sym->buffer);
	
	free(sym);
	return (0);
}

static int get_symbol(vm_t *vm, char **from, symbol_t *next, int lookahead)
{
	unsigned int c, k, size;
	int	i;
	char	*here;
	static int lastsym = 0;
	static char opchar[] = "+-*/%&|!<=>.~?:,#^";

	next->opc = -1;
	next->u.word[0] = 0;
	while ((c = **from) != 0  &&  c <= ' ')
		*from += 1;

	if ((c = **from) == 0)
		return (next->opc = 0);

	here = *from;
	if (isalpha(c)  ||  c == '.'  ||  c == '_') {
		if (lastsym == SYM_NAME  ||  lastsym == SYM_NUMBER)
			next->opc = 0;
		else {
			size = sizeof(next->u.word) - 2;
			k = 0;
			while (c = **from, ISLETTER(c)) {
				*from += 1;
				if (k < size)
					next->u.word[k++] = c;
				}

			next->u.word[k] = 0;
			next->opc = SYM_NAME;
			}
		}
	else if (isdigit(c)) {
		if (c != '0') {
			next->u.num = a2l(*from, from, 10);
			if (**from == '#') {
				int	base;

				base = next->u.num;
				*from += 1;
				next->u.num = a2l(*from, from, base);
				}
			}
		else {
			if ((*from)[1] != 'x'  &&  (*from)[1] != 'X')
				next->u.num = a2l(*from, from, 8);
			else {
				*from += 2;
				next->u.num = a2l(*from, from, 16); /* strtol(*from, from, 16); */
				}
			}
			
		next->opc = SYM_NUMBER;
		}
	else if (c == '"'  ||  c == '\'') {
		int	d, k;

		if (lookahead == 0) {
			*from += 1;
			k = 0;
			
			while ((d = **from) != 0) {
				if (k + 4 >= next->size)
					next->buffer = reallocate(next->buffer, next->size += 512);

				if (d == '\\'  &&  (*from)[1] != 0) {
					d = (*from)[1];
					*from += 2;
					if (d == 0  ||  (d >= 0  &&  d <= '7')) {
						char	i, numchar[10];
						
						if (d == '0'  &&  (**from == 'x'  ||  **from == 'X')) {
							strcpy(numchar, "00");
							*from += 1;
							for (i=0; i<=1; i++) {
								if ((d = **from) != 0) {
									numchar[i] = d;
									*from += 1;
									}
								}

							d = a2l(numchar, NULL, 16); /* strtol(numchar, NULL, 16); */
							}
						else if (d >= '0'  &&  d <= '7') {
							strcpy(numchar, "000");
							numchar[0] = d;
							for (i=1; i<=2; i++) {
								if ((d = **from) != 0) {
									numchar[i] = d;
									*from += 1;
									}
								}

							d = a2l(numchar, NULL, 8); /* strtol(numchar, NULL, 8); */
							}
						else {
							fprint (2, "unexpected numchar format\n");
							longjmp(vm->onerror, -2);
							}

						if (d != 0  &&  d != 1)
							next->buffer[k++] = d;
						}
					else if (d == 'a')
						next->buffer[k++] = '\a';
					else if (d == 'b')
						next->buffer[k++] = '\b';
					else if (d == 'n')
						next->buffer[k++] = '\n';
					else if (d == 't')
						next->buffer[k++] = '\t';
					else
						next->buffer[k++] = d;
					}
				else if (c != d) {
					next->buffer[k++] = d;
					*from += 1;
					}
				else {
					next->buffer[k++] = 0;
					*from += 1;
					break;
					}
				}

			if (c != d) {
				fprint(2, "unterminated string\n");
				longjmp(vm->onerror, -1);
				}
				
			next->u.string = next->buffer;
			}

		next->opc = SYM_STRING;
		}
	else if (strchr(opchar, c) != NULL) {
		size = sizeof(next->u.word) - 2;
		k = 0;
		while (strchr(opchar, c = **from) != NULL) {
			*from += 1;
			if (k < size)
				next->u.word[k++] = c;
			}

		next->u.word[k] = 0;
		next->opc = get_opcode(optab, next->u.word);
		if (next->opc < 0  &&  strlen(next->u.word) == 1)
			next->opc = next->u.word[0];
		}
	else {
		*from += 1;
		next->opc = c;
		}

end:
	if (lookahead != 0) {
		*from = here;
		return (next->opc);
		}
		
	lastsym = 0 /* next->opc */;
	return (next->opc);
}



	/* The parser/interpreter.
	 */

static int level1(vm_t *vm, char **here, symbol_t *this)
{
	int	opc;
	
	if (get_symbol(vm, here, this, 1) == 0)
		return (0);		/* nothing to do */

	level1b(vm, here, this);
	while ((opc = this->opc) == DROP_TOS) {
		drop_tos(vm);
		level1b(vm, here, this);
		}

	return (0);
}

static int level1b(vm_t *vm, char **here, symbol_t *this)
{
	level2a(vm, here, this);
	if (this->opc == EXPR_IFTHEN) {
		long	expr;
		char	*x;
		
		expr = pop_number(vm);

		level2a(vm, here, this);
		if (this->opc != EXPR_ELSE) {
			error(vm, this, "expected : operator", here);
			return (0);
			}

		level2a(vm, here, this);
		if (expr == 1)
			x = pop_stringptr(vm);
		else {
			x = pop_stringptr(vm);
			pop_stringptr(vm);
			push_string(vm, x, -1);
			}
		}
		
	return (0);
}

static int level2a(vm_t *vm, char **here, symbol_t *this)
{
	int	opc;
	long	x, y;
	
	level2b(vm, here, this);
	while ((opc = this->opc) == LOG_OR  ||  opc == BIN_OR) {
		level2b(vm, here, this);

		x = pop_number(vm);
		y = pop_number(vm);
		if (opc == LOG_OR)
			x = (x != 0  ||  y != 0)? 1: 0;
		else
			x = x | y;
		
		push_number(vm, x);
		}

	return (this->opc);
}

static int level2b(vm_t *vm, char **here, symbol_t *this)
{
	int	opc;
	long	x, y;
	
	level3a(vm, here, this);
	while ((opc = this->opc) == LOG_AND  ||  opc == BIN_AND) {
		level3a(vm, here, this);
		
		x = pop_number(vm);
		y = pop_number(vm);
		if (opc == LOG_AND)
			x = (x != 0  &&  y != 0)? 1: 0;
		else
			x = y & x;

		push_number(vm, x);
		}

	return (this->opc);
}

static int level3a(vm_t *vm, char **here, symbol_t *this)
{
	int	opc;
	long	x, y;

	level3b(vm, here, this);
	while ((opc = this->opc) == CMP_EQ  ||  opc == CMP_NE  ||
	        opc == CMP_LT  ||  opc == CMP_LE  ||
		opc == CMP_GT  ||  opc == CMP_GE) {
		level3b(vm, here, this);

		x = pop_number(vm);
		y = pop_number(vm);

		switch (opc) {
		case CMP_EQ:
			x = (x == y)? 1: 0;
			break;

		case CMP_NE:
			x = (x != y)? 1: 0;
			break;
		
		case CMP_LT:
			x = (y < x)?  1: 0;
			break;

		case CMP_LE:
			x = (y <= x)? 1: 0;
			break;

		case CMP_GE:
			x = (y >= x)? 1: 0;
			break;

		case CMP_GT:
			x = (y > x)?  1: 0;
			break;
			}

		push_number(vm, x);
		}

	return (0);
}

static int level3b(vm_t *vm, char **here, symbol_t *this)
{
	int	opc, rc;
	char	*x, *y;

	level4(vm, here, this);
	while ((opc = this->opc) == CMP_LEQ  ||  opc == CMP_LNE  ||
	        opc == CMP_LLT  ||  opc == CMP_LLE  ||
		opc == CMP_LGT  ||  opc == CMP_LGE) {
		level4(vm, here, this);

		x = pop_stringptr(vm);
		y = pop_stringptr(vm);
		rc = strcmp(y, x);
		
		switch (opc) {
		case CMP_LEQ:
			rc = (rc == 0);
			break;

		case CMP_LNE:
			rc = (rc != 0);
			break;
			
		case CMP_LLT:
			rc = (rc < 0);
			break;

		case CMP_LLE:
			rc = (rc <= 0);
			break;

		case CMP_LGE:
			rc = (rc >= 0);
			break;

		case CMP_LGT:
			rc = (rc > 0);
			break;
			}

		push_number(vm, rc);
		}

	return (0);
}

static int level4(vm_t *vm, char **here, symbol_t *this)
{
	int	opc;
	long	x, y;
	
	level5a(vm, here, this);
	while ((opc = this->opc) == SHIFT_RIGHT  ||  opc == SHIFT_LEFT) {
		level5a(vm, here, this);

		x = pop_number(vm);
		y = pop_number(vm);
		if (x <= 0)
			/* do nothing */ ;
		if (opc == SHIFT_RIGHT)
			y = y >> x;
		else
			y = y << x;

		push_number(vm, y);
		}
		
	return (0);
}

static int level5a(vm_t *vm, char **here, symbol_t *this)
{
	int	opc;
	char	*x, *y;

	level5b(vm, here, this);
	while ((opc = this->opc) == STR_MATCH) {
		int	c, i;
		char	*m;

		level5b(vm, here, this);
		
		x = pop_stringptr(vm);
		y = pop_stringptr(vm);
		m = setstring(vm, x, -1);

		for (i=0; (c = m[i]) != 0; i++)
			m[i] = (c == '?'  ||  c == '*'  ||  c == '[');

		push_number(vm, match(x, m, y) != 0? 1: 0);
		}

	return (0);
}

static int level5b(vm_t *vm, char **here, symbol_t *this)
{
	int	opc;
	char	*x, *y;
	
	level6a(vm, here, this);
	while ((opc = this->opc) == STR_CAT) {
		level6a(vm, here, this);
		
		x = pop_stringptr(vm);
		setstring(vm, pop_stringptr(vm), -1);
		x = appendstring(vm, x, -1);
		push_string(vm, x, -1);
		}

	return (this->opc);
}

static int level6a(vm_t *vm, char **here, symbol_t *this)
{
	int	opc;
	long	x, y;
	
	level6b(vm, here, this);
	while ((opc = this->opc) == NUM_PLUS  ||  opc == NUM_MINUS) {
		level6b(vm, here, this);

		x = pop_number(vm);
		y = pop_number(vm);

		if (opc == NUM_PLUS)
			x = y + x;
		else
			x = y - x;

		push_number(vm, x);
		}

	return (this->opc);
}

static int level6b(vm_t *vm, char **here, symbol_t *this)
{
	int	opc;
	long	x, y;
	
	level7(vm, here, this);
	while ((opc = this->opc) == NUM_MUL  ||  opc == NUM_DIV  ||  opc == NUM_MOD) {
		level7(vm, here, this);

		x = pop_number(vm);
		y = pop_number(vm);

		if (opc == NUM_MUL)
			x = y * x;
		else if (opc == NUM_DIV)
			x = (x != 0)? y / x: 0;
		else if (opc == NUM_MOD)
			x = (x != 0)? y % x: 0;

		push_number(vm, x);
		}

	return (this->opc);
}

static int level7(vm_t *vm, char **here, symbol_t *this)
{
	int	opc;
	long	x;
	
	get_symbol(vm, here, this, 1);
	if ((opc = this->opc) != NUM_MINUS  &&  opc != NUM_PLUS
	    &&  opc != LOG_NOT  &&  opc != BIN_INV)
		level8(vm, here, this);
	else {
		get_symbol(vm, here, this, 0);
		level8(vm, here, this);

		x = pop_number(vm);
		if (opc == NUM_PLUS)
			x = x;
		else if (opc == NUM_MINUS)
			x = -x;
		else if (opc == LOG_NOT)
			x = (x == 0)? 1: 0;
		else if (opc == BIN_INV)
			x = ~x;

		push_number(vm, x);
		}
		
	return (this->opc);	
}

static int level8(vm_t *vm, char **here, symbol_t *this)
{
	level9(vm, here, this);
	if (this->opc == '(') {
		level1b(vm, here, this);
		if (this->opc == ')')
			get_symbol(vm, here, this, 0);
/*			level9(vm, here, this); */
		}
		
	return (this->opc);
}

static int level9(vm_t *vm, char **here, symbol_t *this)
{
	char	*p;
	
	if (get_symbol(vm, here, this, 0) == 0) {
		fprint (2, "expr: unexpected end of expression\n");
		longjmp(vm->onerror, -2);
		}
		
	if (this->opc == SYM_NUMBER) {
		push_number(vm, this->u.num);
		get_symbol(vm, here, this, 0);
		}
	else if (this->opc == SYM_STRING) {
		push_string(vm, this->u.string, -1);
		get_symbol(vm, here, this, 0);
		}
	else if (this->opc == ARRAY_LEN) {
		get_symbol(vm, here, this, 0);
		if (this->opc != SYM_NAME) {
			fprint (2, "expr: variable name expected: #\n");
			longjmp(vm->onerror, -2);
			}

		push_number(vm, h_getcellcount(this->u.word));
		get_symbol(vm, here, this, 0);
		}
	else if (this->opc == SYM_NAME) {
		symbol_t *next;

		next = alloc_symbol();
		get_symbol(vm, here, next, 1);
		if (next->opc == '(') {
			int	opc;

			opc = get_opcode(fntab, this->u.word);
			if (opc == -1) {
				goto varhandler;

				fprint (2, "not a function: %s\n", this->u.word);
				longjmp(vm->onerror, 1);
				}

			get_symbol(vm, here, next, 0);
			do_function(vm, opc, this->u.word, here, next);
			if (next->opc != ')') {
				fprint (2, "syntax error: %s()\n", this->u.word);
				longjmp(vm->onerror, 1);
				}
				
			get_symbol(vm, here, this, 0);
			}
		else {
			int	index;

varhandler:
			index = 0;
			if (next->opc == '['  ||  next->opc == '(') {
				get_symbol(vm, here, next, 0);
				level1(vm, here, next);
				if (next->opc != ']'  &&  next->opc != ')') {
					fprint (2, "expr: unmatched '['\n");
					longjmp(vm->onerror, -2);
					}

				index = pop_number(vm);
				get_symbol(vm, here, next, 1);
				}
				
			if (next->opc == VAR_ASSIGN) {
				char	*val;

				get_symbol(vm, here, next, 0);
				level1(vm, here, next);
				h_exprasgn(this->u.word, index, convert_tos_to_string(vm));

				this->opc = next->opc;
				this->u   = next->u;
				if (next->buffer != NULL) {
					if (next->size >= this->size)
						this->buffer = reallocate(this->buffer, (this->size = next->size + 512));

					strcpy(this->buffer, next->buffer);
					}
				}
			else {
				int	freemem;
				char	*val;

				val = h_exprgetvar(this->u.word, index);
				push_string(vm, val, -1);

				get_symbol(vm, here, this, 0);
				}
			}

		free_symbol(next);
		}

	return (this->opc);
}


static int do_unarytest(vm_t *vm)
{
	int	rc, opc;
	char	*op, *filename;
	unsigned long mode;
	struct stat sbuf;

	filename = pop_stringptr(vm);
	op = pop_stringptr(vm);
	rc = stat(filename, &sbuf);
	
	opc = 0;
	if (*op == '-'  &&  op[2] == 0)
		opc = op[1];

	if (opc == 'e'  ||  rc != 0) {
		push_number(vm, rc == 0? 1: 0);
		return (0);
		}

	mode = sbuf.st_mode;
	if (opc == 'b')
		rc = S_ISBLK(mode);
	else if (opc == 'c')
		rc = S_ISCHR(mode);
	else if (opc == 'd')
		rc = S_ISDIR(mode);
	else if (opc == 'f')
		rc = S_ISREG(mode);
	else if (opc == 'g')
		rc = (mode & S_ISGID);
	else if (opc == 'k')
		rc = (mode & S_ISVTX);
	else if (opc == 'L')
		rc = S_ISLNK(mode);
	else if (opc == 'p')
		rc = S_ISFIFO(mode);
	else if (opc == 's')
		rc = (S_ISREG(mode))  &&  (sbuf.st_size > 0);
	else if (opc == 'S')
		rc = S_ISSOCK(mode);
	else if (opc == 'u')
		rc = (mode & S_ISUID);
	else if (opc == 'r'  ||  opc == 'w'  ||  opc == 'x') {
		int	perm;
		
		if (sbuf.st_uid == getuid())
			perm = (sbuf.st_mode & S_IRWXU) >> 6;
		else if (sbuf.st_gid == getgid())
			perm = (sbuf.st_mode & S_IRWXG) >> 3;
		else
			perm = sbuf.st_mode & S_IRWXO;

		rc = 0;
		if (opc == 'r'  &&  (perm & S_IROTH))
			rc = 1;
		else if (opc == 'w'  &&  (perm & S_IWOTH))
			rc = 1;
		else if (opc == 'x'  &&  (perm & S_IXOTH))
			rc = 1;
		}
	else if (opc == 'O')
		rc = (sbuf.st_uid == geteuid());
	else if (opc == 'G')
		rc = (sbuf.st_gid == getegid());
	else {
		fprint (2, "test: unknown test expression: %s\n", op);
		longjmp(vm->onerror, -2);
		}

	push_number(vm, rc != 0? 1: 0);
	return (0);
}

static int do_binarytest(vm_t *vm)
{
	int	rc;
	unsigned long mtime;
	char	*p, *op, *file, *listptr, item[200];
	struct stat sbuf;

	listptr = pop_stringptr(vm);
	file    = pop_stringptr(vm);
	op      = pop_stringptr(vm);

	if (strcmp(op, "-nt") != 0  &&  strcmp(op, "-ot") != 0) {
		fprint (2, "test: unknown test expression: %s\n", op);
		longjmp(vm->onerror, -2);
		}
	
	if (stat(file, &sbuf) != 0) {
		push_number(vm, 0);
		return (0);
		}
	
	mtime = sbuf.st_mtime;
	p = listptr;
	rc = 1;
	while (rc == 1  &&  *get_word(&p, item, sizeof(item)) != 0) {
		if (stat(item, &sbuf) != 0)
			rc = 0;
		else if (op[1] == 'n')
			rc = (mtime > sbuf.st_mtime); 
		else
			rc = (mtime < sbuf.st_mtime);
		}
	
	push_number(vm, rc);
	return (0);
}

static int do_stat(vm_t *vm)
{
	int	opc;
	long	rc;
	char	*op, *filename;
	struct stat sbuf;

	filename = pop_stringptr(vm);
	op = pop_stringptr(vm);
	rc = stat(filename, &sbuf);
	
	opc = 0;
	if (*op == '-'  &&  op[2] == 0)
		opc = op[1];

	if (opc == 'e') {
		push_number(vm, rc == 0? 1: 0);
		return (0);
		}
	else if (rc != 0) {
		push_number(vm, -1);
		return (0);
		}

	if (opc == 'a')
		rc = sbuf.st_atime;
	else if (opc == 'c')
		rc = sbuf.st_ctime;
	else if (opc == 'g')
		rc = sbuf.st_gid;
	else if (opc == 'm')
		rc = sbuf.st_mtime;
	else if (opc == 'o')
		rc = sbuf.st_uid;
	else if (opc == 'p') {
		char	perm[20];

		snprint(perm, sizeof(perm) - 2, "%04o",
			sbuf.st_mode & (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO));
		push_string(vm, perm, -1);
		return (0);
		}
	else if (opc == 'q') {
		char	perm[20];

		snprint(perm, sizeof(perm) - 2, "%06o", sbuf.st_mode);
		push_string(vm, perm, -1);
		return (0);
		}
	else if (opc == 's')
		rc = sbuf.st_size;
	else if (opc == 'u') {
		char	str[80], dev[20], inode[40];

/*
 * Funtioniert unter libc6(?) nicht mehr.
 * Warum eigentlich? -- 11APR99wzk
 *
 *		snprint (str, sizeof(str) - 2, "[%.4x]:%ld", sbuf.st_dev, sbuf.st_ino);
 */
 
		snprint (dev, sizeof(dev) - 2, "[%04x]", sbuf.st_dev);
		snprint (inode, sizeof(inode) - 2, "%ld", sbuf.st_ino);
		snprint (str, sizeof(str) - 2, "%s:%s", dev, inode);

		push_string(vm, str, -1);
		return (0);
		}
	else {
		fprint (2, "test: unknown test expression: %s\n", op);
		return (-1);
		}

	push_number(vm, rc);
	return (0);
}

static int optfnarg(vm_t *vm, char *name, char **here, symbol_t *this)
{
	level1b(vm, here, this);
	if (this->opc != DROP_TOS  &&  this->opc != ')') {
		fprint (2, "argument error: %s\n", name);
		longjmp(vm->onerror, -2);
		}
	else if (this->opc == ')')
		return (1);

	return (0);
}

static int fnarg(vm_t *vm, char *name, char **here, symbol_t *this, int last)
{
	level1b(vm, here, this);
	if (last == 0) {
		if (this->opc != DROP_TOS) {
			fprint (2, "missing arguments: %s\n", name);
			longjmp(vm->onerror, 1);
			}
		}
	else {
		if (this->opc != ')') {
			fprint (2, "extra arguments: %s\n", name);
			longjmp(vm->onerror, 1);
			}
		}
		
	return (0);
}

static int do_function(vm_t *vm, int opc, char *name, char **here, symbol_t *this)
{
	int	c, i, k, len, argc;
	char	*p, *r, *s;
	struct stat sbuf;
	
	switch (opc) {
	case FN_TEST:
		argc = 2;
		fnarg(vm, name, here, this, 0);
		if (optfnarg(vm, name, here, this) != 0)
			do_unarytest(vm);
		else {
			fnarg(vm, name, here, this, 1);
			do_binarytest(vm);
			}
		break;

	case FN_STAT:
		fnarg(vm, name, here, this, 0);
		fnarg(vm, name, here, this, 1);
		do_stat(vm);
		break;

	case FN_SIZE:
		fnarg(vm, name, here, this, 1);
		p = pop_stringptr(vm);
		push_number(vm, stat(p, &sbuf) != 0? 0: sbuf.st_size);
		break;

	case FN_SEQ: {
			int	i, start, end, inc;
			char	buffer[20];
			
			fnarg(vm, name, here, this, 0);
			start = pop_number(vm);
			
			fnarg(vm, name, here, this, 0);
			end = pop_number(vm);
			
			fnarg(vm, name, here, this, 1);
			inc = pop_number(vm);


			if (inc == 0)
				inc = 1;

			p = setstring(vm, "", 0);
			if (inc > 0) {
				for (i=start; i<=end; i += inc) {
					if (i > start)
						appendstring(vm, " ", 1);

					snprint (buffer, sizeof(buffer) - 2, "%d", i);
					appendstring(vm, buffer, -1);
					}
				}
			else {
				for (i=end; i>=start; i += inc) {
					if (i > start)
						appendstring(vm, " ", 1);

					snprint (buffer, sizeof(buffer) - 2, "%d", i);
					appendstring(vm, buffer, -1);
					}
				}

			push_string(vm, p, -1);
			break;
			}
	
	case FN_GLOB:
		fnarg(vm, name, here, this, 1);
		p = pop_stringptr(vm);
		push_string(vm, h_glob(p), -1);
		break;
	
	case FN_INDEX:
		fnarg(vm, name, here, this, 0);
		fnarg(vm, name, here, this, 1);
		r = pop_stringptr(vm);
		p = pop_stringptr(vm);
		if (*r == 0)
			push_number(vm, 1);
		else {
			s = strstr(p, r);
			push_number(vm, (s == NULL)? 0: s - p + 1);
			}
		break;
		
	case FN_RINDEX:
		fnarg(vm, name, here, this, 0);
		fnarg(vm, name, here, this, 1);
		r = pop_stringptr(vm);
		p = pop_stringptr(vm);
		if (*r == 0)
			push_number(vm, 1);
		else {
			for (i=strlen(p) - strlen(r); i>=0; i--) {
				if (p[i] == *r  &&  (r[1] == 0  ||  strcmp(&p[i], r) == 0))
					break;
				}

			push_number(vm, (i < 0)? 0: i + 1);
			}
		break;

	case FN_SUBSTR:
		argc = 2;
		fnarg(vm, name, here, this, 0);
		if (optfnarg(vm, name, here, this) == 0) {
			fnarg(vm, name, here, this, 1);
			argc = 3;
			}

		if (argc == 2) {
			k = pop_number(vm) - 1;
			p = pop_stringptr(vm);
			len = strlen(p) - k;
			}
		else {
			len = pop_number(vm);
			k = pop_number(vm) - 1;
			p = pop_stringptr(vm);
			}

		if (*p == 0  ||  len <= 0  ||  k < 0  ||  k >= strlen(p))
			push_string(vm, "", 0);
		else {
			r = &p[k];
			if (len >= strlen(r))
				len = strlen(r);

			r[len] = 0;
			push_string(vm, r, -1);
			}
		break;
	
	case FN_STRLEN:
		fnarg(vm, name, here, this, 1);
		p = pop_stringptr(vm);
		push_number(vm, strlen(p));
		break;
	
	
	case FN_STRFTIME:
		{
			unsigned long ul;
			char	str[200];
			struct tm *when;
			
			fnarg(vm, name, here, this, 0);
			p = pop_stringptr(vm);
			fnarg(vm, name, here, this, 1);
			ul = pop_number(vm);
			if (ul == 1)
				ul = time(NULL);
			
			when = localtime(&ul);
			strftime(str, sizeof(str) - 2, p, when);
			push_string(vm, str, -1);
			}

		break;
	
	case FN_SYSTIME:
		{
			unsigned long ul;

			get_symbol(vm, here, this, 0);
			ul = time(NULL);
			push_number(vm, ul);
			}

		break;
		
	case FN_ARGC:
		get_symbol(vm, here, this, 0);
		push_number(vm, h_getcellcount("*") - 1);
		break;
	
	case FN_ARGV:
		fnarg(vm, name, here, this, 1);
		k = pop_number(vm);
		push_string(vm, h_exprgetvar("*", k + 1), -1);
		break;
		}
		
	return (0);
}

int do_expr(char *line, char **result)
{
	int	rc;
	char	*p, *here;
	symbol_t *this;
	static vm_t *vm = NULL;

	if (vm == NULL) {
		vm = allocate(sizeof(vm_t));
		memset(vm, 0, sizeof(vm_t));
		}

	this = alloc_symbol();
	clear_stack(vm);
	here = line;

	if (setjmp(vm->onerror) != 0)
		return (-1);

	level1(vm, &here, this);
	
	if (this->opc != 0) {
		fprint (2, "expression syntax error\n");
		return (-1);
		}

	if (vm->stack.sp == 0) {
		*result = "";
		return (1);
		}
	else {
		if (vm->stack.cell[vm->stack.sp].type == STRING)
			rc = (vm->stack.cell[vm->stack.sp].u.str.len > 0);
		else
			rc = vm->stack.cell[vm->stack.sp].u.val != 0;
			
		*result = pop_stringptr(vm);
		}
	
	free_symbol(this);
	return (rc);
}


#endif	/* WITH_EXPR */

