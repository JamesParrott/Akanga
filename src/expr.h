
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

#ifndef	_EXPR_INCLUDED
#define	_EXPR_INCLUDED

#include <ctype.h>
#include <setjmp.h>

#include "strlib.h"

/* #include "rc.h" */



	/* Required functions.
	 */

int h_exprasgn(char *name, int index, char *value);
char *h_exprgetvar(char *name, int index);
char *h_glob(char *pattern);

	/* All numbers are long integers.
	 */

#define	LONG_ONLY
typedef long number_t;


	/* The symbol types the parser may return ...
	 */

#define	SYM_NUMBER		1001
#define	SYM_STRING		1002
#define	SYM_NAME		1003
#define	SYM_SYSTEM		1004

	/* ... and this structure holds the symbol.
	 */

typedef struct _symbol {
    int		opc;

    union {
	number_t num;
	char	word[80];
	char	*string;
	} u;

    char	*buffer;
    int		size;
    } symbol_t;


	/* The virtual machine has a operator stack. This is the
	 * data structure for a single stack cell.
	 */

#define	NUMBER		1		/* Datentypen */
#define	STRING		2
#define	VARPTR		3
#define	PTR		4

typedef struct _cell {			/* Stackelement */
    int		type, heap;
    union {
	number_t val;
	void	*var;
	void	*ptr;
	struct {
	    int		len;
	    char	*ptr;
	    } str;
	} u;
    } cell_t;


	/* The data structure for the "virtual machine".
	 */

typedef struct _vm {
    jmp_buf	onerror;

    struct {
	cell_t	cell[40];
	int	sp;		/* (*) */

	int	heap;		/* (*) */
	char	*mem;
	int	max;
	} stack;


    struct {
	char	*ptr;
	int	here, max;
	} buffer;
    } vm_t;


	/* vm_t contains a buffer structure for strings of arbitrary
	 * size.
	 */

static char *setstring(vm_t *vm, char *string, int len);
static char *appendstring(vm_t *vm, char *string, int len);


	/* Stack operation prototypes.
	 */

static int drop_tos(vm_t *vm);
static int push_number(vm_t *vm, number_t val);
static number_t pop_number(vm_t *vm);
static int push_string(vm_t *vm, char *string, int len);
static char *pop_string(vm_t *vm, char *string, int size);
static char *pop_stringptr(vm_t *vm);
static char *convert_cell_to_string(cell_t *cell);
static char *convert_tos_to_string(vm_t *vm);



	/* Definitions for the tokenizer.
	 */

typedef struct _opcode {
    char	sym[10];
    int		opcode;
    } opcode_t;
    
#define	ISLETTER(c)	(isalpha(c)  ||  isdigit(c)  ||  c == '.'  ||  c == '_'  ||  c == '/')

static int error(vm_t *vm, symbol_t *this, char *message, char **here);
static int get_opcode(opcode_t *opctab, char *sym);
static int get_symbol(vm_t *vm, char **from, symbol_t *next, int lookahead);


	/* Operator Opcodes.
	 */

#define	NUM_PLUS	301
#define	NUM_MINUS	302
#define	NUM_MUL		303
#define	NUM_DIV		304
#define	NUM_MOD		305
#define	CMP_LT		306
#define	CMP_LE		307
#define	CMP_EQ		308
#define	CMP_NE		309
#define CMP_GE		310
#define	CMP_GT		311
#define	LOG_AND		312
#define	LOG_OR		313
#define	LOG_NOT		314
#define	VAR_ASSIGN	315
#define	DROP_TOS	316
#define	EXPR_IFTHEN	317
#define	EXPR_ELSE	318
#define	SHIFT_LEFT	319
#define	SHIFT_RIGHT	320
#define	BIN_AND		321
#define	BIN_OR		322
#define	BIN_XOR		323
#define	BIN_INV		324
#define	STR_CAT		325
#define	STR_MATCH	326
#define	CMP_LLT		327
#define	CMP_LLE		328
#define	CMP_LEQ		329
#define	CMP_LNE		330
#define CMP_LGE		331
#define	CMP_LGT		332
#define	ARRAY_LEN	333


	/* Function Opcodes.
	 */

#define	FN_TEST		401
#define	FN_STAT		402
#define	FN_SIZE		403
#define	FN_SEQ		404
#define	FN_GLOB		405
#define	FN_INDEX	406
#define	FN_RINDEX	407
#define	FN_SUBSTR	408
#define	FN_STRLEN	409
#define	FN_STRFTIME	410
#define	FN_ARGV		411
#define	FN_ARGC		412
#define	FN_SYSTIME	413


static int level1(vm_t *vm, char **here, symbol_t *this);
static int level1b(vm_t *vm, char **here, symbol_t *this);
static int level2a(vm_t *vm, char **here, symbol_t *this);
static int level2b(vm_t *vm, char **here, symbol_t *this);
static int level3a(vm_t *vm, char **here, symbol_t *this);
static int level3b(vm_t *vm, char **here, symbol_t *this);
static int level4(vm_t *vm, char **here, symbol_t *this);
static int level5a(vm_t *vm, char **here, symbol_t *this);
static int level5b(vm_t *vm, char **here, symbol_t *this);
static int level6a(vm_t *vm, char **here, symbol_t *this);
static int level6b(vm_t *vm, char **here, symbol_t *this);
static int level6c(vm_t *vm, char **here, symbol_t *this);
static int level7(vm_t *vm, char **here, symbol_t *this);
static int level8(vm_t *vm, char **here, symbol_t *this);
static int level9(vm_t *vm, char **here, symbol_t *this);
static int do_test(vm_t *vm);
static int fnarg(vm_t *vm, char *name, char **here, symbol_t *this, int last);
static int do_function(vm_t *vm, int opc, char *name, char **here, symbol_t *this);


	/* Entry point into the interpreter.
	 */

int do_expr(char *line, char **result);

#endif

