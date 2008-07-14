/*
 * paranormal: iterated pipeline-driven visualization plugin
 * Copyright (c) 2006, 2007 William Pitcock <nenolod@dereferenced.org>
 * Portions copyright (c) 2001 Jamie Gennis <jgennis@mindspring.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
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

#include <config.h>

#include <glib.h>

#include "actuators.h"
//#include "containers.h"

/* FIXME: container options override containees - fix this? */
/* FIXME: add actuator groups (by a group name string) */

/* FIXME: add support for copying containers' children (optionally) */
struct pn_actuator *
copy_actuator (const struct pn_actuator *a)
{
  struct pn_actuator *actuator;
  int i;    

  actuator = g_new (struct pn_actuator, 1);

  actuator->desc = a->desc;

  /* Make an options table */
  if (actuator->desc->option_descs)
    {
      /* count the options */
      for (i=0; actuator->desc->option_descs[i].name; i++);

      actuator->options = g_new (struct pn_actuator_option, i + 1);
      for (i=0; actuator->desc->option_descs[i].name; i++)
	{
	  actuator->options[i].desc = &actuator->desc->option_descs[i];

	  /* copy the default options */
	  switch (actuator->desc->option_descs[i].type)
	    {
	    case OPT_TYPE_INT:
	    case OPT_TYPE_COLOR_INDEX:
	    case OPT_TYPE_FLOAT:
	    case OPT_TYPE_COLOR:
	    case OPT_TYPE_BOOLEAN:
	      memcpy (&actuator->options[i].val,
		      &a->options[i].val,
		      sizeof (union actuator_option_val));
	      break;
	    case OPT_TYPE_STRING:
              actuator->options[i].val.sval = g_strdup(a->options[i].val.sval);
              break;
	    default:
	      break;
	    }
	}

      /* the NULL option */
      actuator->options[i].desc = NULL;
    }
  else
    actuator->options = NULL;

  if (actuator->desc->init)
    actuator->desc->init (&actuator->data);

  return actuator;
}

struct pn_actuator_desc *
get_actuator_desc (const char *name)
{
  int i;

  for (i=0; builtin_table[i]; i++)
    if (! g_strcasecmp (name, builtin_table[i]->name) || ! g_strcasecmp(name, builtin_table[i]->dispname))
      break;

  /* actuator not found */
  if (! builtin_table[i])
    return NULL;

  return builtin_table[i];
}

struct pn_actuator *
create_actuator (const char *name)
{
  int i;
  struct pn_actuator_desc *desc;
  struct pn_actuator *actuator;

  /* find the actuatoreration */
  desc = get_actuator_desc (name);

  if (! desc)
    return NULL;

  actuator = g_new (struct pn_actuator, 1);
  actuator->desc = desc;

  /* Make an options table */
  if (actuator->desc->option_descs)
    {
      /* count the options */
      for (i=0; actuator->desc->option_descs[i].name != NULL; i++);

      actuator->options = g_new0 (struct pn_actuator_option, i + 1);
      for (i=0; actuator->desc->option_descs[i].name != NULL; i++)
	{
	  actuator->options[i].desc = &actuator->desc->option_descs[i];

	  /* copy the default options */
	  switch (actuator->desc->option_descs[i].type)
	    {
	    case OPT_TYPE_INT:
	    case OPT_TYPE_COLOR_INDEX:
	    case OPT_TYPE_FLOAT:
	    case OPT_TYPE_COLOR:
	    case OPT_TYPE_BOOLEAN:
	      memcpy (&actuator->options[i].val,
		      &actuator->desc->option_descs[i].default_val,
		      sizeof (union actuator_option_val));
	      break;
	    case OPT_TYPE_STRING:
	      /* NOTE: It's not realloc'ed so don't free it */
	      actuator->options[i].val.sval =
		actuator->desc->option_descs[i].default_val.sval;
	      break;
	    }
	}

      /* the NULL option */
      actuator->options[i].desc = NULL;
    }
  else
    actuator->options = NULL;

  if (actuator->desc->init)
    actuator->desc->init (&actuator->data);

  return actuator;
}

void
destroy_actuator (struct pn_actuator *actuator)
{
  int i;

  if (actuator->desc->cleanup)
    actuator->desc->cleanup (actuator->data);

  /* find any option val's that need to be freed */
  if (actuator->options)
    for (i=0; actuator->options[i].desc; i++)
      switch (actuator->options[i].desc->type)
	{
	case OPT_TYPE_INT:
	case OPT_TYPE_FLOAT:
	case OPT_TYPE_COLOR:
	case OPT_TYPE_COLOR_INDEX:
	case OPT_TYPE_BOOLEAN:
	  break;
	case OPT_TYPE_STRING:
	  if (actuator->options[i].val.sval
	      != actuator->options[i].desc->default_val.sval)
	    g_free ((char *)actuator->options[i].val.sval);
	}

  g_free (actuator->options);
  g_free (actuator);
}

void
exec_actuator (struct pn_actuator *actuator)
{
  g_assert (actuator);
  g_assert (actuator->desc);
  g_assert (actuator->desc->exec);
  actuator->desc->exec (actuator->options, actuator->data);
}

