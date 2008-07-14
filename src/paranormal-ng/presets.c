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

/* FIXME: add documentation support to preset files */
/* FIXME: add multiple-presets-per-file support (maybe) */

#include <config.h>

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#include <glib.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "paranormal.h"
#include "actuators.h"
#include "containers.h"

/* cur->name should be the actuator name */
static void
parse_actuator (xmlNodePtr cur, struct pn_actuator *a)
{
  int i;
  char *content;
  struct pn_actuator *child;

  for (cur = cur->xmlChildrenNode; cur; cur = cur->next)
    {
      if (xmlIsBlankNode (cur) || cur->type != XML_ELEMENT_NODE)
	continue;

      /* see if it's an option */
      for (i=0; a->options && a->options[i].desc; i++)
	if (! xmlStrcmp (cur->name,
			 (const xmlChar *) a->options[i].desc->name))
	  break;

      if (a->options && a->options[i].desc)
	{
	  /* it is an option, so let's set it! */
	  content = (char*)xmlNodeGetContent (cur);

	  /* FIXME: warning? */
	  if (! content)
	    continue;

	  /* FIXME: perhaps do a little better job of error checking? */
	  switch (a->options[i].desc->type)
	    {
	    case OPT_TYPE_INT:
	      a->options[i].val.ival = (int)strtol (content, NULL, 0);
	      break;
	    case OPT_TYPE_FLOAT:
	      a->options[i].val.fval = (float)strtod (content, NULL);
	      break;
	    case OPT_TYPE_STRING:
	      a->options[i].val.sval = g_strdup (content);
	      break;
	    case OPT_TYPE_COLOR:
	      {
		guint r,g,b;
		char *s = content+1;
		r = strtoul (s, &s, 0);
		if (r > 255 || ! (s = strchr (s, ',')))
		  goto bad_color;
		g = strtoul (s+1, &s, 0);
		if (g > 255 || ! (s = strchr (s, ',')))
		  goto bad_color;
		b = strtoul (s+1, NULL, 0);
		if (b > 255)
		  goto bad_color;

		a->options[i].val.cval.r = (guchar)r;
		a->options[i].val.cval.g = (guchar)g;
		a->options[i].val.cval.b = (guchar)b;

		break;
	      }
	    bad_color:
	      pn_error ("parse error: invalid color value: option \"%s\" ignored.\n"
			"  correct syntax: (r,g,b) where r, g, and b are the\n"
			"  red, green, and blue components of the "
			"color, respectively", cur->name);
	      break;
	      
	    case OPT_TYPE_COLOR_INDEX:
	      {
		int c = (int)strtol (content, NULL, 0);
		if (c < 0 || c > 255)
		  pn_error ("parse error: invalid color index \"%s\" (%d): option ignored.\n"
			    "  the value must be between 0 and 255",
			    cur->name, c);
		else
		  a->options[i].val.ival = c;
		break;
	      }
	    case OPT_TYPE_BOOLEAN:
	      {
		char *c, *d;
		for (c=content; isspace (*c); c++);
		for (d=c; !isspace(*d); d++);
		*d = '\0';
		if (g_strcasecmp (c, "true") == 0)
		  a->options[i].val.bval = TRUE;
		else if (g_strcasecmp (c, "false") == 0)
		  a->options[i].val.bval = FALSE;
		else
		  pn_error ("parse error: invalid boolean value \"%s\" (%s): option ignored.\n"
			    "  the value must be either 'true' or 'false'",
			    cur->name, c);
	      }
	    }

	  /* gotta free content */
	  xmlFree ((xmlChar*)content);
	}
      /* See if we have a child actuator */
      else if (a->desc->flags & ACTUATOR_FLAG_CONTAINER
	       && (child = create_actuator ((char*)cur->name)))
	{
	  container_add_actuator (a, child);
	  parse_actuator (cur, child);
	}
      else
	/* We have an error */
	pn_error ("parse error: unknown entity \"%s\": ignored.", cur->name);
    }
}

struct pn_actuator *
load_preset (const char *filename)
{
  xmlDocPtr doc;
  xmlNodePtr cur;
  struct pn_actuator *a = NULL;

  doc = xmlParseFile (filename);
  if (! doc)
    return NULL;

  cur = xmlDocGetRootElement (doc);
  if (! cur)
    xmlFreeDoc (doc);

  if (xmlStrcmp (cur->name, (const xmlChar *) "paranormal_preset"))
    {
      xmlFreeDoc (doc);
      return NULL;
    }

  for (cur = cur->children; cur; cur = cur->next)
    {
      if (xmlIsBlankNode (cur) || cur->type != XML_ELEMENT_NODE)
	continue;

      /* if (...) { ... } else if (is_documentation [see top of file]) ... else */
      {
	a = create_actuator ((char*)cur->name);

	/* FIXME: warn? */
	if (! a)
	  continue;

	parse_actuator (cur, a);
	break;
      }
    }

  /* Don't need this any longer */
  xmlFreeDoc (doc);

  return a;
}

/* FIXME: do the file writing w/ error checking */
static gboolean
save_preset_recursive (FILE *file, const struct pn_actuator *actuator,
		       int recursion_depth)
{
  int i;
  GSList *child;

  /* open this actuator */
  fprintf (file, "%*s<%s>\n", recursion_depth, "", actuator->desc->name);

  /* options */
  if (actuator->options)
    for (i=0; actuator->options[i].desc; i++)
      {
	fprintf (file, "%*s <%s> ", recursion_depth, "",
		 actuator->desc->option_descs[i].name);
	switch (actuator->desc->option_descs[i].type)
	  {
	  case OPT_TYPE_INT:
	  case OPT_TYPE_COLOR_INDEX:
	    fprintf (file, "%d", actuator->options[i].val.ival);
	    break;
	  case OPT_TYPE_FLOAT:
	    fprintf (file, "%.5f", actuator->options[i].val.fval);
	    break;
	  case OPT_TYPE_STRING:
	    fprintf (file, "%s", actuator->options[i].val.sval);
	    break;
	  case OPT_TYPE_COLOR:
	    fprintf (file, "%d, %d, %d", actuator->options[i].val.cval.r,
		     actuator->options[i].val.cval.g,
		     actuator->options[i].val.cval.b);
	    break;
	  case OPT_TYPE_BOOLEAN:
	    if (actuator->options[i].val.bval)
	      fprintf (file, "TRUE");
	    else
	      fprintf (file, "FALSE");
	    break;
	  }
	fprintf (file, " </%s>\n", actuator->desc->option_descs[i].name);
      }

  /* children */
  if (actuator->desc->flags & ACTUATOR_FLAG_CONTAINER)
    for (child = *(GSList **)actuator->data; child; child = child->next)
      if (! save_preset_recursive (file, (struct pn_actuator*) child->data,
				   recursion_depth+1))
	return FALSE;

  /* close the actuator */
  fprintf (file, "%*s</%s>\n", recursion_depth, "", actuator->desc->name);

  return TRUE;
}

gboolean
save_preset (const char *filename, const struct pn_actuator *actuator)
{
  FILE *file;

  file = fopen (filename, "w");
  if (! file)
    {
      pn_error ("fopen: %s", strerror (errno));
      return FALSE;
    }

  fprintf (file, "<?xml version=\"1.0\"?>\n\n<paranormal_preset>\n");

  if (actuator)
    if (! save_preset_recursive (file, actuator, 1))
      {
	fclose (file);
	return FALSE;
      }

  fprintf (file, "</paranormal_preset>");

  fclose (file);

  return TRUE;

}
