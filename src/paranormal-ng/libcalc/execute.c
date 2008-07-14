/* execute.c -- execute precompiled expression expr
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
 
#include <glib.h>
#include <math.h>
 
#include "execute.h"
#include "function.h"

/* Execution stack. */

gboolean check_stack (ex_stack *stack, int depth) {
  if (stack->sp < depth) {
    g_warning ("Stack error");
    return FALSE;
  }

  return TRUE;
}

void push (ex_stack *stack, double value) {
  g_assert (stack);

  if (stack->sp < STACK_DEPTH) {
    stack->value[stack->sp++] = value;
  } else {
    g_warning ("Stack overflow");
  }
}

double pop (ex_stack *stack) {
  g_assert (stack);

  if (stack->sp > 0) {
    return stack->value[--stack->sp];
  } else {
    g_warning ("Stack error (stack empty)");
    return 0.0;
  }
}

/* */

void expr_execute (expression_t *expr, symbol_dict_t *dict) {
  char op, *str = expr->data->str;
  ex_stack stack = { 0, { 0.0 }};

  while ((op = *str++)) {
    switch (op) { 
    case 'l': /* Load a variable. */
      push (&stack, dict->variables[load_int (str)].value);
      str += sizeof (int);
      break;
      
    case 's': /* Store to a variable. */
      dict->variables[load_int (str)].value = pop (&stack);
      str += sizeof (int);
      break;
   
    case 'f': /* Call a function. */
      function_call (load_int (str), &stack);
      str += sizeof (int);
      break;
      
    case 'c': /* Load a constant. */
      push (&stack, load_double (str));
      str += sizeof (double);
      break;

    case 'n': /* Do a negation. */
      push (&stack, -pop (&stack));
      break;

    case '+': /* Do an addition. */
      push (&stack, pop (&stack) + pop (&stack));
      break;
    case '-': /* Do a subtraction. */
      push (&stack, pop (&stack) - pop (&stack));
      break;
    case '*': /* Do a multiplication. */
      push (&stack, pop (&stack) * pop (&stack));
      break;
    case '/': /* Do a division. */
      if (check_stack (&stack, 2)) {
        double y = stack.value[stack.sp - 2] / stack.value[stack.sp - 1];
	stack.sp -= 2;
	push (&stack, y);
      }
      break;
    case '^': /* Do an exponentiation. */
      if (check_stack (&stack, 2)) {
        double y = pow (stack.value[stack.sp - 2], stack.value[stack.sp - 1]);
	stack.sp -= 2;
	push (&stack, y);
      }
      break;

    default:
      g_warning ("Invalid opcode: %c", op);
      return;
    }
  }
}
