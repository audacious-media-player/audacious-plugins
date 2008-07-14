/* storage.h -- 
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

#ifndef Included_STORAGE_H
#define Included_STORAGE_H

#include <glib.h>

typedef struct {
  GString *data;
} expression_t;

/* Expr block. */
expression_t *expr_new (void);
void expr_free (expression_t *expr);

int load_int (char *str);
double load_double (char *str);

void store_byte (expression_t *expr, char byte);
void store_int (expression_t *expr, int val);
void store_double (expression_t *expr, double val);

#endif /* Included_STORAGE_H */
