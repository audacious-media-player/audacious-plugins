/* parser.y -- Bison parser for libexp
 *
 * Copyright (C) 2001 Janusz Gregorczyk <jgregor@kki.net.pl>
 *
 * This file is part of xvs.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* suppress conflict warnings */
%expect 37

/* C declarations. */
%{
#include <ctype.h>
#include <glib.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "dict.h"
#include "execute.h"
#include "function.h"
#include "parser.h"
#include "storage.h"

#define YYPARSE_PARAM yyparam
#define YYLEX_PARAM yyparam

static gboolean expr_add_compile (expression_t *expr, symbol_dict_t *dict,
				  char *str);

#define GENERATE(str) if (!expr_add_compile (((parser_control *)yyparam)->expr, \
				        ((parser_control *)yyparam)->dict, str)) \
                   YYABORT;
%}

/* Data types. */
%union {
char *s_value;
char c_value;
double d_value;
int i_value;
}

%{
int yyerror (char * s);
int yylex (YYSTYPE * yylval, void * yyparam);
%}

%pure_parser

/* Terminal symbols. */
%token <s_value> NAME
%token <d_value> NUMBER

/* Precedence rules. */
%right '='
%left '-' '+'
%left '*' '/'
%left NEG
%right '^'

/* Grammar follows */
%%

/* Input consits of a (possibly empty) list of expressions. */
input:			/* empty */
			| input expression_list
;

/* expression_list is a ';' separated list of expressions. */
expression_list:	/* empty */
    			| expression
			    { }
                        | expression_list ';'
    			| error ';'
			    { yyerrok; }

/* argument list is a comma separated list od expressions */
argument_list:
			expression
                            {
                            }
                        | argument_list ',' expression
                            {
                            }

/* expression is a C-like expression. */
expression:		NUMBER
			    {
                              char *buf = g_strdup_printf ("c%f:", $1);
                              GENERATE (buf);
                              g_free (buf);
                            }
		        | NAME
			    {
                              char *buf = g_strdup_printf ("l%s:", $1);
                              GENERATE (buf);
                              g_free (buf);
                            }
    			| NAME '=' expression
			    {
                              char *buf = g_strdup_printf ("s%s:", $1);
                              GENERATE (buf);
                              g_free (buf);
                            }
                        | NAME '(' argument_list ')'
                            {
                              char *buf = g_strdup_printf ("f%s:", $1);
                              GENERATE (buf);
                              g_free (buf);
                            }

			| expression '>' expression
			    { GENERATE (">"); }
			| expression '<' expression
			    { GENERATE ("<"); }

			| expression '+' expression
			    { GENERATE ("+"); }
			| expression '-' expression
			    { GENERATE ("-"); }
			| expression '*' expression
			    { GENERATE ("*"); }
			| expression '/' expression
			    { GENERATE ("/"); }
			| '-' expression %prec NEG
			    { GENERATE ("n"); }
			| expression '^' expression
			    { GENERATE ("^"); }
			| '(' expression ')'
			    { }
;

%%
/* End of grammar */

/* Called by yyparse on error. */
int yyerror (char *s) {
  /* Ignore errors, just print a warning. */
  g_warning ("%s\n", s);
  return 0;
}

int yylex (YYSTYPE *yylval, void *yyparam) {
  int c;
  parser_control *pc = (parser_control *) yyparam;

  /* Ignore whitespace, get first nonwhite character. */
  while ((c = vfs_getc (pc->input)) == ' ' || c == '\t' || c == '\n');

  /* End of input ? */
  if (c == EOF)
    return 0;

  /* Char starts a number => parse the number. */
  if (isdigit (c)) {
    if (vfs_ungetc (c, pc->input) == EOF)
      return 0;

    {
      char *old_locale, *saved_locale;

      old_locale = setlocale (LC_ALL, NULL);
      saved_locale = g_strdup (old_locale);
      setlocale (LC_ALL, "C");
      sscanf ((gchar *) ((VFSBuffer *)(pc->input->handle))->iter, "%lf", & yylval->d_value);

      while (isdigit(c) || c == '.')
      {
        c = vfs_getc(pc->input);
      }

      if (c != EOF && vfs_ungetc (c, pc->input) == EOF)
        return 0;

      setlocale (LC_ALL, saved_locale);
      g_free (saved_locale);
    }
    return NUMBER;
  }

  /* Char starts an identifier => read the name. */
  if (isalpha (c)) {
    GString *sym_name;

    sym_name = g_string_new (NULL);

    do {
      sym_name = g_string_append_c (sym_name, c);

      /* Get another character. */
      c = vfs_getc (pc->input);
    } while (c != EOF && isalnum (c));

    if (c != EOF && vfs_ungetc (c, pc->input) == EOF) {
      g_string_free (sym_name, FALSE);
      return 0;
    }

    yylval->s_value = sym_name->str;

    g_string_free (sym_name, FALSE);

    return NAME;
  }

  /* Any other character is a token by itself. */
  return c;
}

static int load_name (char *str, char **name) {
  int count = 0;
  GString *new = g_string_new (NULL);

  while (*str != 0 && *str != ':') {
    g_string_append_c (new, *str++);
    count++;
  }

  *name = new->str;
  g_string_free (new, FALSE);

  return count;
}

static gboolean expr_add_compile (expression_t *expr, symbol_dict_t *dict,
				  char *str) {
  char op;
  double dval;
  int i;
  char *name;

  while ((op = *str++)) {
    switch (op) {
    case 'c':			/* A constant. */
      store_byte (expr, 'c');
      sscanf (str, "%lf%n", &dval, &i);
      str += i;
      store_double (expr, dval);
      str++;			/* Skip ';' */
      break;

    case 'f':			/* A function call. */
      store_byte (expr, 'f');
      str += load_name (str, &name);
      i = function_lookup (name);
      if (i < 0) return FALSE;	/* Fail on error. */
      store_int (expr, i);
      g_free (name);
      str++;			/* Skip ';' */
      break;

    case 'l':			/* Load a variable. */
    case 's':			/* Store a variable. */
      store_byte (expr, op);
      str += load_name (str, &name);
      i = dict_lookup (dict, name);
      store_int (expr, i);
      g_free (name);
      str++;			/* Skip ';' */
      break;

    default:			/* Copy verbatim. */
      store_byte (expr, op);
      break;
    }
  }

  return TRUE;
}

expression_t *expr_compile_string (const char* str, symbol_dict_t *dict)
{
  parser_control pc;
  VFSFile *stream;

  g_return_val_if_fail(str != NULL && dict != NULL, NULL);

  stream = vfs_buffer_new_from_string ( (char *) str );

  pc.input = stream;
  pc.expr = expr_new ();
  pc.dict = dict;

  if (yyparse (&pc) != 0) {
    /* Check for error. */
    expr_free (pc.expr);
    pc.expr = NULL;
  }

  vfs_fclose (stream);

  return pc.expr;
}
