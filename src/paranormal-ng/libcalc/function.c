/* function.c --
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
#include <string.h>
 
#include "function.h"

/* Function pointer type. */
typedef struct {
  char *name;
  double (*funcptr)(ex_stack *stack);
} func_t;

/* */

static double f_log (ex_stack *stack) {
  return log (pop (stack));
}

static double f_sin (ex_stack *stack) {
  return sin (pop (stack));
}

static double f_cos (ex_stack *stack) {
  return cos (pop (stack));
}

static double f_tan (ex_stack *stack) {
  return tan (pop (stack));
}

static double f_asin (ex_stack *stack) {
  return asin (pop (stack));
}

static double f_acos (ex_stack *stack) {
  return acos (pop (stack));
}

static double f_atan (ex_stack *stack) {
  return atan (pop (stack));
}

static double f_if (ex_stack *stack) {
  double a = pop (stack);
  double b = pop (stack);
  return (pop (stack) != 0.0) ? a : b;
}

static double f_div (ex_stack *stack) {
  int y = (int)pop (stack);
  int x = (int)pop (stack);
  return (y == 0) ? 0 : (x / y);
}

static double f_rand (ex_stack *stack) {
  return g_random_double_range((double) pop(stack), (double) pop(stack));
}

/* */

static const func_t init[] = {
  { "sin", f_sin },
  { "cos", f_cos },
  { "tan", f_tan },
  { "asin", f_asin },
  { "acos", f_acos },
  { "atan", f_atan },
  { "log", f_log },
  { "if", f_if },
  { "div", f_div },
  { "rand", f_rand }
};

int function_lookup (const char *name) {
  int i;

  for (i = 0; i < sizeof (init) / sizeof (init[0]); i++)
    if (strcmp (init[i].name, name) == 0)
      return i;

  g_warning ("Unknown function: %s\n", name);
  return -1;
}

void function_call (int func_id, ex_stack *stack) {
  g_assert (func_id >= 0);
  g_assert (func_id < sizeof (init) / sizeof (init[0]));

  push (stack, (*init[func_id].funcptr)(stack));
}
