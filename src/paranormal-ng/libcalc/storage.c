/* storage.c --
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
#include <stddef.h>
 
#include "storage.h"

/* Code block. */

expression_t *expr_new (void) {
  expression_t *new_expr;

  new_expr = (expression_t *)g_malloc (sizeof(expression_t));
  new_expr->data = g_string_new (NULL);
  return new_expr;
}

void expr_free (expression_t *expr) {
  if (!expr)
    return;

  g_string_free (expr->data, TRUE);
  g_free (expr);
}

static void load_data (char *str, void *dest, size_t size) {
  char *ch = (char *)dest;
  while (size--)
    *ch++ = *str++;
}

int load_int (char *str) {
  int val;
  load_data (str, &val, sizeof(val));
  return val;
}

double load_double (char *str) {
  double val;
  load_data (str, &val, sizeof(val));
  return val;
}

static void store_data (expression_t *expr, void *src, size_t size) {
  char *ch = (char *)src;
  while (size--)
    store_byte (expr, *ch++);
}

void store_byte (expression_t *expr, char byte) {
  g_string_append_c (expr->data, byte);
}

void store_int (expression_t *expr, int val) {
  store_data (expr, &val, sizeof(val));
}

void store_double (expression_t *expr, double val) {
  store_data (expr, &val, sizeof(val));
}
