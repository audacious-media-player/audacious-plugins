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

/* FIXME: rename actuators to pn_actuators */
/* FIXME: add a color type to the OPT_TYPE's */

#ifndef _ACTUATORS_H
#define _ACTUATORS_H

#include <glib.h>
#include <SDL.h>

/* Helper macros for actuator functions */
#define PN_ACTUATOR_INIT_FUNC(f) ((void (*) (gpointer *data))f)
#define PN_ACTUATOR_CLEANUP_FUNC(f) ((void (*) (gpointer data))f)
#define PN_ACTUATOR_EXEC_FUNC(f) \
((void (*) (const struct pn_actuator_option *opts, gpointer data))f)

struct pn_color
{
  guchar r, g, b;
  guchar spluzz; /* gotta look like an SDL_Color */
};


union actuator_option_val
{
  int ival;
  float fval;
  const char *sval;
  struct pn_color cval;
  gboolean bval;
};

/* A actuator's option description */
struct pn_actuator_option_desc
{
  const char *name;
  const char *doc; /* option documentation */
  enum
  {
    OPT_TYPE_INT = 0,
    OPT_TYPE_FLOAT = 1,
    OPT_TYPE_STRING = 2,
    OPT_TYPE_COLOR = 3,
    OPT_TYPE_COLOR_INDEX = 4, /* uses ival */
    OPT_TYPE_BOOLEAN = 5
  } type;
  union actuator_option_val default_val;
};

/* The actual option instance */
struct pn_actuator_option
{
  const struct pn_actuator_option_desc *desc;
  union actuator_option_val val;
};

/* An operation's description */
struct pn_actuator_desc
{
  const char *name;	/* e.g. "container_simple" */
  const char *dispname;	/* e.g. "Simple Container" */
  const char *doc; /* documentation txt */
  const enum
  {
    ACTUATOR_FLAG_CONTAINER = 1<<0
  } flags;

  /* A null terminating (ie a actuator_option_desc == {0,...,0})
     array - OPTIONAL (optional fields can be NULL) */
  const struct pn_actuator_option_desc *option_descs;

  /* Init function - data points to the actuator.data - OPTIONAL */
  void (*init) (gpointer *data);

  /* Cleanup actuatortion - OPTIONAL */
  void (*cleanup) (gpointer data);

  /* Execute actuatortion - REQUIRED (duh!) */
  void (*exec) (const struct pn_actuator_option *opts, gpointer data);
};

/* An actual operation instance */
struct pn_actuator
{
  const struct pn_actuator_desc *desc;
  struct pn_actuator_option *options;
  gpointer data;
};

/* The array containing all operations (see builtins.c) */
extern struct pn_actuator_desc *builtin_table[];

/* functions for actuators */
struct pn_actuator_desc *get_actuator_desc (const char *name);
struct pn_actuator *copy_actuator (const struct pn_actuator *a);
struct pn_actuator *create_actuator (const char *name);
void destroy_actuator (struct pn_actuator *actuator);
void exec_actuator (struct pn_actuator *actuator);

#endif /* _ACTUATORS_H */
