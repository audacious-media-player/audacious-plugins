/* execute.h -- execute precompiled expression code
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef Included_EXECUTE_H
#define Included_EXECUTE_H

#include <glib.h>

#include "dict.h"
#include "storage.h"

/* Execution stack. */

typedef struct { 
#define STACK_DEPTH 64
  int sp;			/* stack pointer */
  double value[STACK_DEPTH];
} ex_stack;

/* Prototypes. */

gboolean check_stack (ex_stack *stack, int depth);
void push (ex_stack *stack, double value);
double pop (ex_stack *stack);

void expr_execute (expression_t *expr, symbol_dict_t *dict);

#endif /* Included_EXECUTE_H */
