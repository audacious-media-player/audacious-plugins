/*
 * kanashi: iterated javascript-driven visualization framework
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

/* FIXME: what to name this file? */

#include <config.h>

#include "kanashi.h"
#include "kanashi_utils.h"

/* **************** general_fade **************** */
void
kanashi_fade(gint amt)
{
    register gint i, j;

    for (j=0; j<kanashi_image_data->height; j++)
        for (i=0; i<kanashi_image_data->width; i++)
            kanashi_image_data->surface[0][PN_IMG_INDEX (i, j)] =
 	       CAPLO (kanashi_image_data->surface[0][PN_IMG_INDEX (i, j)] - amt, 0);
}

/* **************** general_blur **************** */
void
kanashi_blur(void)
{
  int i,j;
  register guchar *srcptr = kanashi_image_data->surface[0];
  register guchar *destptr = kanashi_image_data->surface[1];
  register int sum;

  for (j=0; j<kanashi_image_data->height; j++)
    for (i=0; i<kanashi_image_data->width; i++)
      {
	sum = *(srcptr)<<2;

	/* top */
	if (j > 0)
	  {
	    sum += *(srcptr-kanashi_image_data->width)<<1;
	    if (i > 0)
	      sum += *(srcptr-kanashi_image_data->width-1);
	    if (i < kanashi_image_data->width-1)
	      sum += *(srcptr-kanashi_image_data->width+1);
	  }
	/* bottom */
	if (j < kanashi_image_data->height-1)
	  {
	    sum += *(srcptr+kanashi_image_data->width)<<1;
	    if (i > 0)
	      sum += *(srcptr+kanashi_image_data->width-1);
	    if (i < kanashi_image_data->width-1)
	      sum += *(srcptr+kanashi_image_data->width+1);
	  }
	/* left */
	if (i > 0)
	  sum += *(srcptr-1)<<1;
	/* right */
	if (i < kanashi_image_data->width-1)
	  sum += *(srcptr+1)<<1;

	*destptr++ = (guchar)(sum >> 4);
	srcptr++;
      }

  kanashi_swap_surfaces ();
}

/* **************** general_mosaic **************** */
void
kanashi_mosaic(gint radius)
{
  int i,j;
  register guchar *srcptr = kanashi_image_data->surface[0];
  register guchar *destptr = kanashi_image_data->surface[1];

  for (j=0; j<kanashi_image_data->height; j += radius)
    for (i=0; i<kanashi_image_data->width; i += radius)
      {
        int ii = 0, jj = 0;
        guchar bval = 0;

        /* find the brightest colour */
        for (jj = 0; jj < radius && (j + jj < kanashi_image_data->height); jj++)
          for (ii = 0; ii < radius && (i + ii < kanashi_image_data->width); ii++)
            {
               guchar val = srcptr[PN_IMG_INDEX(i + ii,  j + jj)];

               if (val > bval)
                 bval = val;
            }

        for (jj = 0; jj < radius && (j + jj < kanashi_image_data->height); jj++)
          for (ii = 0; ii < radius && (i + ii < kanashi_image_data->width); ii++)
            {
               destptr[PN_IMG_INDEX(i + ii, j + jj)] = bval;
            }
      }

  kanashi_swap_surfaces ();
}

/* **************** general_clear **************** */
void
kanashi_clear(void)
{
   memset(kanashi_image_data->surface[0], '\0',
	  (kanashi_image_data->height * kanashi_image_data->width));
}

/* **************** general_invert **************** */
void
kanashi_invert(void)
{
  int i, j;

  for (j=0; j < kanashi_image_data->height; j++)
    for (i=0; i < kanashi_image_data->width; i++)
      kanashi_image_data->surface[0][PN_IMG_INDEX (i, j)] =
	255 - kanashi_image_data->surface[0][PN_IMG_INDEX (i, j)];
}

/* **************** general_replace **************** */
#if 0
static struct kanashi_actuator_option_desc general_replace_opts[] =
{
  { "start", "The beginning colour value that should be replaced by the value of out.",
    OPT_TYPE_INT, { ival: 250 } },
  { "end", "The ending colour value that should be replaced by the value of out.",
    OPT_TYPE_INT, { ival: 255 } },
  { "out", "The colour value that in is replaced with.",
    OPT_TYPE_INT, { ival: 0 } },
  { NULL }
};

static void
general_replace_exec (const struct kanashi_actuator_option *opts,
	   gpointer data)
{
  register int i, j;
  register guchar val;
  guchar begin = opts[0].val.ival > 255 || opts[0].val.ival < 0 ? 250 : opts[0].val.ival;
  guchar end = opts[1].val.ival > 255 || opts[1].val.ival < 0 ? 255 : opts[1].val.ival;
  guchar out = opts[2].val.ival > 255 || opts[2].val.ival < 0 ? 0 : opts[2].val.ival;

  for (j=0; j < kanashi_image_data->height; j++)
    for (i=0; i < kanashi_image_data->width; i++)
      {
        val = kanashi_image_data->surface[0][PN_IMG_INDEX (i, j)];
        if (val >= begin && val <= end)
          kanashi_image_data->surface[0][PN_IMG_INDEX (i, j)] = out;
      }
}

struct kanashi_actuator_desc builtin_general_replace =
{
  "general_replace", "Value Replace", "Performs a value replace on a range of values.",
  0, general_replace_opts,
  NULL, NULL, general_replace_exec
};
#endif

/* **************** general_swap **************** */
void
kanashi_swap(void)
{
  kanashi_swap_surfaces ();
}

/* **************** general_copy **************** */
void
kanashi_copy(void)
{
  memcpy(kanashi_image_data->surface[1], kanashi_image_data->surface[0], 
         (kanashi_image_data->width * kanashi_image_data->height));
}

/* **************** general_flip **************** */
void
kanashi_flip(gint direction)
{
  gint x, y;

  if (direction < 0)
    {
      for (y = 0; y < kanashi_image_data->height; y++)
        for (x = 0; x < kanashi_image_data->width; x++)
          {
             kanashi_image_data->surface[1][PN_IMG_INDEX(kanashi_image_data->width - x, y)] = 
               kanashi_image_data->surface[0][PN_IMG_INDEX(x, y)];
          }
    }
  else
    {
      for (y = 0; y < kanashi_image_data->height; y++)
        for (x = 0; x < kanashi_image_data->width; x++)
          {
             kanashi_image_data->surface[1][PN_IMG_INDEX(x, kanashi_image_data->height - y)] = 
               kanashi_image_data->surface[0][PN_IMG_INDEX(x, y)];
          }
    }

    kanashi_swap_surfaces ();
}
