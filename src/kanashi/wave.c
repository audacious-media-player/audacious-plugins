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

#include <config.h>

#include <math.h>

#include "kanashi.h"
#include "kanashi_utils.h"

#include "drawing.h"

/* **************** wave_horizontal **************** */
void
kanashi_render_horizontal_waveform(gint channels, guchar value)
{
  int channel = ( channels < 0 ) ? 0 : 1;
  int *x_pos, *y_pos;	/* dynamic tables which store the positions for the line */
  int *x2_pos, *y2_pos;	/* dynamic tables which store the positions for the line */
  int i;
  float step;

  g_print("waveform render\n");

  x_pos = g_new0(int, 257);
  y_pos = g_new0(int, 257);
  x2_pos = g_new0(int, 257);
  y2_pos = g_new0(int, 257);

  step = kanashi_image_data->width / 256.;

  /* calculate the line. */
  for (i = 0; i < 256; i++)
    {
      if (channels != 0)
        {
           x_pos[i] = i * step;
           y_pos[i] = (kanashi_image_data->height>>1) - 
			CAP (kanashi_sound_data->pcm_data[channel][i * 2]>>8, 
			(kanashi_image_data->height>>1)-1);
        }
      else
        {
           x_pos[i] = i * step;
           y_pos[i] = (kanashi_image_data->height>>2) - 
			CAP (kanashi_sound_data->pcm_data[0][i * 2]>>9,
			(kanashi_image_data->height>>2)-1);

           x2_pos[i] = i * step;
           y2_pos[i] = 3*(kanashi_image_data->height>>2) - 
			CAP (kanashi_sound_data->pcm_data[1][i * 2]>>9,
			(kanashi_image_data->height>>2)-1);

        }
    }

  /* draw the line. */
  for (i = 1; i < 256; i++)
    {
       kanashi_draw_line(x_pos[i - 1], y_pos[i - 1], x_pos[i], y_pos[i], value);

       if ( channels == 0 )
         kanashi_draw_line(x2_pos[i - 1], y2_pos[i - 1], x2_pos[i], y2_pos[i], value);
    }

  g_free(x_pos);
  g_free(y_pos);
  g_free(x2_pos);
  g_free(y2_pos);
}

void
kanashi_render_vertical_waveform(gint channels, guchar value)
{
  int channel = ( channels < 0 ) ? 0 : 1;
  int *x_pos, *y_pos;	/* dynamic tables which store the positions for the line */
  int *x2_pos, *y2_pos;	/* dynamic tables which store the positions for the line */
  int i;
  float step;

  x_pos = g_new0(int, 129);
  y_pos = g_new0(int, 129);
  x2_pos = g_new0(int, 129);
  y2_pos = g_new0(int, 129);

  step = kanashi_image_data->height / 128.;

  /* calculate the line. */
  for (i = 0; i < 128; i++)
    {
      if (channels != 0)
        {
           x_pos[i] = (kanashi_image_data->width>>1) - 
			CAP (kanashi_sound_data->pcm_data[channel]
			[i*4]>>8,
			(kanashi_image_data->width>>1)-1);
	   y_pos[i] = i * step;
        }
      else
        {
           x_pos[i] = (kanashi_image_data->width>>2) 
		      - CAP (kanashi_sound_data->pcm_data[0]
		     [i*4]>>9,
		     (kanashi_image_data->width>>2)-1);
           y_pos[i] = i * step;

           x2_pos[i] = 3*(kanashi_image_data->width>>2) 
		      - CAP (kanashi_sound_data->pcm_data[1]
		     [i*4]>>9,
		     (kanashi_image_data->width>>2)-1);
           y2_pos[i] = i * step;
        }
    }

  /* draw the line. */
  for (i = 1; i < 128; i++)
    {
       kanashi_draw_line(x_pos[i - 1], y_pos[i - 1], x_pos[i], y_pos[i], value);

       if ( channels == 0 )
         kanashi_draw_line(x2_pos[i - 1], y2_pos[i - 1], x2_pos[i], y2_pos[i], value);
    }

  g_free(x_pos);
  g_free(y_pos);
  g_free(x2_pos);
  g_free(y2_pos);
}

#if 0
/* FIXME: allow for only 1 channel for wave_normalize & wave_smooth */
/* **************** wave_normalize **************** */
static struct kanashi_actuator_option_desc wave_normalize_opts[] =
{
  { "height", "If positive, the height, in pixels, to which the waveform will be "
    "normalized; if negative, hfrac is used", OPT_TYPE_INT, { ival: -1 } },
  { "hfrac", "If positive, the fraction of the horizontal image size to which the "
    "waveform will be normalized; if negative, vfrac is used",
    OPT_TYPE_FLOAT, { fval: -1 } },
  { "vfrac", "If positive, the fraction of the vertical image size to which the "
    "waveform will be normalized",
    OPT_TYPE_FLOAT, { fval: .125 } },
  { "channels", "Which sound channel(s) to normalize: negative = channel 1,\n"
    "\tpositive = channel 2, 0 = both channels.",
    OPT_TYPE_INT, { ival: 0 } },
  { NULL }
};

static void
wave_normalize_exec (const struct kanashi_actuator_option *opts,
		     gpointer data)
{
  int i, j, max=0;
  float denom;

  for (j=0; j<2; j++)
    {
      if ( !(opts[3].val.ival) || (opts[3].val.ival < 0 && j == 0) ||
	   (opts[3].val.ival > 0 && j == 1) ) {
	
	for (i=0; i<512; i++)
	  if (abs(kanashi_sound_data->pcm_data[j][i]) > max)
	    max = abs(kanashi_sound_data->pcm_data[j][i]);
	
	if (opts[0].val.ival > 0)
	  denom = max/(opts[0].val.ival<<8);
	else if (opts[1].val.fval > 0)
	  denom = max/(opts[1].val.fval * (kanashi_image_data->width<<8));
	else
	  denom = max/(opts[2].val.fval * (kanashi_image_data->height<<8));
	
	if (denom > 0)
	  for (i=0; i<512; i++)
	    kanashi_sound_data->pcm_data[j][i]
	      /= denom;
      }
    }
}

struct kanashi_actuator_desc builtin_wave_normalize =
{
  "wave_normalize", "Normalize Waveform Data",
  "Normalizes the waveform data used by the wave_* actuators",
  0, wave_normalize_opts,
  NULL, NULL, wave_normalize_exec
};

/* **************** wave_smooth **************** */
struct kanashi_actuator_option_desc wave_smooth_opts[] = 
{
  { "channels", "Which sound channel(s) to smooth: negative = channel 1, \n"
    "\tpositive = channel 2, 0 = both channels.",
    OPT_TYPE_INT, { ival: 0 } },
  {0}
};

static void
wave_smooth_exec (const struct kanashi_actuator_option *opts,
		  gpointer data)
{
  int i, j, k;
  gint16 tmp[512];

  for (j=0; j<2; j++)
    {
      if ( !(opts[0].val.ival) || (opts[0].val.ival < 0 && j == 0) ||
	   (opts[0].val.ival > 0 && j == 1) ) { 
	
	for (i=4; i<508; i++)
	  {
	    k = (kanashi_sound_data->pcm_data[j][i]<<3)
	      + (kanashi_sound_data->pcm_data[j][i+1]<<2)
	      + (kanashi_sound_data->pcm_data[j][i-1]<<2)
	      + (kanashi_sound_data->pcm_data[j][i+2]<<2)
	      + (kanashi_sound_data->pcm_data[j][i-2]<<2)
	      + (kanashi_sound_data->pcm_data[j][i+3]<<1)
	      + (kanashi_sound_data->pcm_data[j][i-3]<<1)
	      + (kanashi_sound_data->pcm_data[j][i+4]<<1)
	      + (kanashi_sound_data->pcm_data[j][i-4]<<1);
	    tmp[i] = k >> 5;
	  }
	memcpy (kanashi_sound_data->pcm_data[j]+4, tmp, sizeof (gint16) * 504);
      } 
    }
}

struct kanashi_actuator_desc builtin_wave_smooth =
{
  "wave_smooth", "Smooth Waveform Data",
  "Smooth out the waveform data used by the wave_* actuators",
  0, wave_smooth_opts,
  NULL, NULL, wave_smooth_exec
};

/* **************** wave_radial **************** */
static struct kanashi_actuator_option_desc wave_radial_opts[] =
{
  { "base_radius", " ", 
    OPT_TYPE_FLOAT, { fval: 0 } },
  {"value", "The colour value to use.", OPT_TYPE_INT, {ival: 255} },
/*  {"lines", "Use lines instead of dots.", OPT_TYPE_BOOLEAN, {bval: TRUE} }, */
  { NULL }
};

static void
wave_radial_exec (const struct kanashi_actuator_option *opts,
		  gpointer data)
{
  int i, x, y;
  guchar value = (opts[1].val.ival < 0 || opts[1].val.ival > 255) ? 255 : opts[1].val.ival;
  
  for(i=0; i<360; i++)
    {
      x = (kanashi_image_data->width>>1)
	+ (opts[0].val.fval + (kanashi_sound_data->pcm_data[0][(int)(i*(512.0/360.0))]>>8))
	* cos_val[i];
      y = (kanashi_image_data->height>>1) 
	+ (opts[0].val.fval + (kanashi_sound_data->pcm_data[0][(int)(i*(512.0/360.0))]>>8))
	* sin_val[i];

      kanashi_image_data->surface[0][PN_IMG_INDEX (CAPHILO(x,kanashi_image_data->width,0),
					      CAPHILO(y,kanashi_image_data->height,0))]
	= value;
    }
};	       

struct kanashi_actuator_desc builtin_wave_radial =
{
  "wave_radial", "Radial Waveform",
  "Draws a single waveform varying"
  " radially from the center of the image",
  0, wave_radial_opts,
  NULL, NULL, wave_radial_exec
};
#endif
