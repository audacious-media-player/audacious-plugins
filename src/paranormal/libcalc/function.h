/* function.h --
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

#ifndef Included_FUNCTION
#define Included_FUNCTION

#include "execute.h"

/* Prototypes. */

int function_lookup (const char *name);
void function_call (int func_id, ex_stack *stack);

#endif /* Included_FUNCTION */
