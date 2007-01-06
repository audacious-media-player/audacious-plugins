/* dict.c -- symbol dictionary structures
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
 
#define V_SPACE_INIT 8
#define V_SPACE_INCR 8
 
#include <glib.h>
#include <string.h>

#include "dict.h"

static void more_variables (symbol_dict_t *dict) {
  var_t *new_var;

  dict->v_space += V_SPACE_INCR;

  new_var = (var_t *)g_malloc (dict->v_space * sizeof(var_t));

  memcpy (new_var, dict->variables, dict->v_count * sizeof(var_t));
  g_free (dict->variables);

  dict->variables = new_var;
}

symbol_dict_t *dict_new (void) {
  symbol_dict_t *dict;
  
  dict = (symbol_dict_t *) g_malloc (sizeof(symbol_dict_t));

  /* Allocate space for variables. */
  dict->v_count = 0;
  dict->v_space = V_SPACE_INIT;
  dict->variables = (var_t *)g_malloc (dict->v_space * sizeof(var_t));
  
  return dict;
}

void dict_free (symbol_dict_t *dict) {
  int i;

  if (!dict)
    return;

  /* Free memory used by variables. */
  for (i = 0; i < dict->v_count; i++)
    g_free (dict->variables[i].name);
  g_free (dict->variables);

  g_free (dict);
}

static int dict_define_variable (symbol_dict_t *dict, const char *name) {
  var_t *var;

  if (dict->v_count >= dict->v_space)
    more_variables (dict);

  var = &dict->variables[dict->v_count];

  var->value = 0.0;
  var->name = g_strdup (name);

  return dict->v_count++;
}

int dict_lookup (symbol_dict_t *dict, const char *name) {
  int i;

  for (i = 0; i < dict->v_count; i++) {
    if (strcmp (dict->variables[i].name, name) == 0)
      return i;
  }
  
  /* Not defined -- define a new variable. */
  return dict_define_variable (dict, name);
}

double *dict_variable (symbol_dict_t *dict, const char *var_name) {
  int id = dict_lookup (dict, var_name);
  return &dict->variables[id].value;
}
