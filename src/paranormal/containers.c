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

#include <stdio.h>

#include <glib.h>

#include "actuators.h"
#include "paranormal.h"

/* **************** all containers **************** */

/* Add a actuator to a container (end of list) */
void
container_add_actuator (struct pn_actuator *container, struct pn_actuator *a)
{
  g_assert (container->desc->flags & ACTUATOR_FLAG_CONTAINER);
  g_assert (a);

  *((GSList **)container->data) =
    g_slist_append (*(GSList **)container->data, a);
}  

void
container_remove_actuator (struct pn_actuator *container, struct pn_actuator *a)
{
  g_assert (container->desc->flags & ACTUATOR_FLAG_CONTAINER);
  g_assert (a);

  *((GSList **)container->data) =
    g_slist_remove (*(GSList **)container->data, a);
} 

/* clear the containee list */
void
container_unlink_actuators (struct pn_actuator *container)
{
  g_assert (container->desc->flags & ACTUATOR_FLAG_CONTAINER);

  g_slist_free (*(GSList **)container->data);
  *(GSList **)container->data = NULL;
}

/* this does NOT free data */
static void
container_cleanup (GSList** data)
{
  GSList *child;

  for (child = *data; child; child = child->next)
    destroy_actuator ((struct pn_actuator *) child->data);

  g_slist_free (*data);
}

/* **************** container_simple **************** */
static void
container_simple_init (GSList ***data)
{
  *data = g_new0 (GSList *, 1);
}

static void
container_simple_cleanup (GSList **data)
{
  container_cleanup (data);
  g_free (data);
}

static void
container_simple_exec (const struct pn_actuator_option *opts,
		     GSList **data)
{
  GSList *child;

  for (child = *data; child; child = child->next)
    exec_actuator ((struct pn_actuator *) child->data);
}

struct pn_actuator_desc builtin_container_simple =
{
  "container_simple",
  "Simple Container",
  "A simple (unconditional) container\n\n"
  "This is usually used as the root actuator of a list",
  ACTUATOR_FLAG_CONTAINER, NULL,
  PN_ACTUATOR_INIT_FUNC (container_simple_init),
  PN_ACTUATOR_CLEANUP_FUNC (container_simple_cleanup),
  PN_ACTUATOR_EXEC_FUNC (container_simple_exec)
};

/* **************** container_once **************** */
struct container_once_data
{
  GSList *children; /* This MUST be first! */

  gboolean done;
};

static void
container_once_init (struct container_once_data **data)
{
  *data = g_new0 (struct container_once_data, 1);
}

static void
container_once_cleanup (GSList **data)
{
  container_cleanup (data);
  g_free (data);
}

static void
container_once_exec (const struct pn_actuator_option *opts,
		     struct container_once_data *data)
{
  if (! data->done)
    {
      GSList *child;

      for (child = data->children; child; child = child->next)
	exec_actuator ((struct pn_actuator *) child->data);

      data->done = TRUE;
    }
}

struct pn_actuator_desc builtin_container_once =
{
  "container_once",
  "Initialization Container",
  "A container whose contents get executed exactly once.\n\n"
  "This is often used to set initial graphics states such as the\n"
  "pixel depth, or to display some text (such as credits)",
  ACTUATOR_FLAG_CONTAINER, NULL,
  PN_ACTUATOR_INIT_FUNC (container_once_init),
  PN_ACTUATOR_CLEANUP_FUNC (container_once_cleanup),
  PN_ACTUATOR_EXEC_FUNC (container_once_exec)
};

/* **************** container_cycle ***************** */
static struct pn_actuator_option_desc container_cycle_opts[] =
{
  { "change_interval", "The number of seconds between changing the "
    "child to be executed", OPT_TYPE_INT, { ival: 20 } },
  { "beat", "Whether or not the change should only occur on a beat",
    OPT_TYPE_BOOLEAN, { bval: TRUE } },
  { NULL }
};

struct container_cycle_data
{
  GSList *children;
  GSList *current;
  int last_change;
};

static void
container_cycle_init (gpointer *data)
{
  *data = g_new0 (struct container_cycle_data, 1);
}

static void
container_cycle_cleanup (gpointer data)
{
  container_cleanup (data);
  g_free (data);
}

static void
container_cycle_exec (const struct pn_actuator_option *opts,
		      gpointer data)
{
  struct container_cycle_data *cdata = (struct container_cycle_data*)data;
  int now;

  /*
   * Change branch if all of the requirements are met for the branch to change.
   */
  if ((opts[1].val.bval == TRUE && pn_new_beat != FALSE) || opts[1].val.bval == FALSE)
    {
       now = SDL_GetTicks();	

       if (now - cdata->last_change
           > opts[0].val.ival * 1000)
         {
            cdata->last_change = now;

            /* FIXME: add randomization support */
            if (cdata->current)
              cdata->current = cdata->current->next;
         }
    }

  if (! cdata->current)
    cdata->current = cdata->children;

  if (cdata->current)
    exec_actuator ((struct pn_actuator*)cdata->current->data);
}

struct pn_actuator_desc builtin_container_cycle =
{
  "container_cycle",
  "Branched-execution Container",
  "A container that alternates which of its children is executed;  it "
  "can either change on an interval, or only on a beat.",
  ACTUATOR_FLAG_CONTAINER, container_cycle_opts,
  container_cycle_init, container_cycle_cleanup, container_cycle_exec
};

/* **************** container_onbeat **************** */
static void
container_onbeat_init (GSList ***data)
{
  *data = g_new0 (GSList *, 1);
}

static void
container_onbeat_cleanup (GSList **data)
{
  container_cleanup (data);
  g_free (data);
}

static void
container_onbeat_exec (const struct pn_actuator_option *opts,
		     GSList **data)
{
  GSList *child;

  if (pn_new_beat == TRUE)
    {
      for (child = *data; child; child = child->next)
        exec_actuator ((struct pn_actuator *) child->data);
    }
}

struct pn_actuator_desc builtin_container_onbeat =
{
  "container_onbeat",
  "OnBeat Container",
  "A simple container which only triggers on a beat.",
  ACTUATOR_FLAG_CONTAINER, NULL,
  PN_ACTUATOR_INIT_FUNC (container_onbeat_init),
  PN_ACTUATOR_CLEANUP_FUNC (container_onbeat_cleanup),
  PN_ACTUATOR_EXEC_FUNC (container_onbeat_exec)
};
