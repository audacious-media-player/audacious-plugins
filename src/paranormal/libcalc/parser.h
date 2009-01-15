/* parser.h -- header file for libexp
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

#ifndef Included_PARSER_H
#define Included_PARSER_H

#include <glib.h>
#include <stdio.h>
#include <audacious/plugin.h>
#include <libaudcore/vfs_buffer.h>

#include "execute.h"

/* Structure passed do yyparse. */
typedef struct {
  VFSFile *input;
  expression_t *expr;
  symbol_dict_t *dict;
} parser_control;

/* Prototypes. */
expression_t *expr_compile_string (const char *str, symbol_dict_t *dict);

#endif /* Included_PARSER_H */
