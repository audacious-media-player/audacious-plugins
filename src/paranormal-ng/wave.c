/*
 * paranormal-ng: iterated pipeline-driven visualization plugin
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

#include "paranormal.h"
#include "actuators.h"
#include "pn_utils.h"

#include "drawing.h"

#include "libcalc/calc.h"

/* **************** wave_horizontal **************** */
struct pn_actuator_option_desc wave_horizontal_opts[] =
{
  {"channels", "Which sound channels to use: negative = channel 1, \npositive = channel 2, "
   "zero = both (two wave-forms.)", OPT_TYPE_INT, {ival: -1} },
  {"value", "The colour value to use.", OPT_TYPE_COLOR, {cval: { 0, 192, 255 } } },
  {"width", "The line's width.", OPT_TYPE_INT, {ival: 1} },
  { NULL }
};

void
wave_horizontal_exec_lines (const struct pn_actuator_option *opts,
		    gpointer data)
{
  int channel = ( opts[0].val.ival < 0 ) ? 0 : 1;
  guchar value = (opts[1].val.ival < 0 || opts[1].val.ival > 255) ? 255 : opts[1].val.ival;
  GLfloat *x_pos, *y_pos;	/* dynamic tables which store the positions for the line */
  GLfloat *x2_pos, *y2_pos;	/* dynamic tables which store the positions for the line */
  gint i, j;
  float step;
  GLubyte colour[] = { opts[1].val.cval.r, opts[1].val.cval.g, opts[1].val.cval.b, 255 };

  x_pos = g_new0(GLfloat, 257);
  y_pos = g_new0(GLfloat, 257);
  x2_pos = g_new0(GLfloat, 257);
  y2_pos = g_new0(GLfloat, 257);

  step = pn_image_data->width / 128.;

  /* calculate the line. */
  for (j = 0, i = -128; j < 256; i++, j++)
    {
      if (opts[0].val.ival != 0)
        {
           x_pos[j] = (i * step) / pn_image_data->width;
           y_pos[j] = (GLfloat) CAP (pn_sound_data->pcm_data[channel][j * 2]>>5, 
			(pn_image_data->height>>1)) / (GLfloat) pn_image_data->height;
        }
      else
        {
           x_pos[j] = (i * step) / pn_image_data->width;
           y_pos[j] = ((GLfloat) CAP (pn_sound_data->pcm_data[channel][j * 2]>>5, 
			(pn_image_data->height>>1)) / (GLfloat) pn_image_data->height) - 0.5f;

           x2_pos[j] = (i * step) / pn_image_data->width;
           y2_pos[j] = ((GLfloat) CAP (pn_sound_data->pcm_data[channel][j * 2]>>5, 
			(pn_image_data->height>>1)) / (GLfloat) pn_image_data->height) + 0.5f;

        }
    }

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glLineWidth(opts[2].val.ival);
  glBegin(GL_LINE_STRIP);

  glColor4ubv(colour);

  /* draw the line. */
  for (i = 1; i < 256; i++)
    {
       glVertex2d(x_pos[i - 1], y_pos[i - 1]);
       glVertex2d(x_pos[i], y_pos[i]);
    }

  glEnd();

  if ( opts[0].val.ival == 0 )
    {
       glBegin(GL_LINE_STRIP);

       for (i = 1; i < 256; i++)
         {
           glVertex2d(x2_pos[i - 1] / pn_image_data->width, y2_pos[i - 1] / pn_image_data->height);
           glVertex2d(x2_pos[i] / pn_image_data->width, y2_pos[i] / pn_image_data->height);
         }

       glEnd();
    }

  glPopMatrix();

  g_free(x_pos);
  g_free(y_pos);
  g_free(x2_pos);
  g_free(y2_pos);
}

static void
wave_horizontal_exec (const struct pn_actuator_option *opts,
		    gpointer data)
{
  wave_horizontal_exec_lines(opts, data);
}

struct pn_actuator_desc builtin_wave_horizontal =
{
  "wave_horizontal", "Horizontal Waveform",
  "Draws one or two waveforms horizontally across "
  "the drawing surface",
  0, wave_horizontal_opts,
  NULL, NULL, wave_horizontal_exec
};

/* **************** wave_vertical **************** */
struct pn_actuator_option_desc wave_vertical_opts[] =
{
  {"channels", "Which sound channels to use: negative = channel 1, \npositive = channel 2, "
   "zero = both (two wave-forms.)", OPT_TYPE_INT, {ival: -1} },
  {"value", "The colour value to use.", OPT_TYPE_COLOR, {cval: { 0, 192, 255 } } },
  {"width", "The line's width.", OPT_TYPE_INT, {ival: 1} },
  { NULL }
};

static void
wave_vertical_exec_lines (const struct pn_actuator_option *opts,
		    gpointer data)
{
  int channel = ( opts[0].val.ival < 0 ) ? 0 : 1;
  guchar value = (opts[1].val.ival < 0 || opts[1].val.ival > 255) ? 255 : opts[1].val.ival;
  GLfloat *x_pos, *y_pos;	/* dynamic tables which store the positions for the line */
  GLfloat *x2_pos, *y2_pos;	/* dynamic tables which store the positions for the line */
  gint i, j;
  float step;
  GLubyte colour[] = { opts[1].val.cval.r, opts[1].val.cval.g, opts[1].val.cval.b, 255 };

  x_pos = g_new0(GLfloat, 129);
  y_pos = g_new0(GLfloat, 129);
  x2_pos = g_new0(GLfloat, 129);
  y2_pos = g_new0(GLfloat, 129);

  step = pn_image_data->height / 64.;

  /* calculate the line. */
  for (i = -64, j = 0; j < 128; i++, j++)
    {
      if (opts[0].val.ival != 0)
        {
           x_pos[j] = (GLfloat) CAP (pn_sound_data->pcm_data[channel][j * 2]>>6, 
			(pn_image_data->height>>1)) / (GLfloat) pn_image_data->width;
	   y_pos[j] = (i * step) / pn_image_data->height;
        }
      else
        {
           x_pos[j] = ((GLfloat) CAP (pn_sound_data->pcm_data[channel][j * 2]>>6, 
			(pn_image_data->height>>1)) / (GLfloat) pn_image_data->width) - 0.5;
	   y_pos[j] = (i * step) / pn_image_data->height;

           x2_pos[j] = ((GLfloat) CAP (pn_sound_data->pcm_data[channel][j * 2]>>6, 
			(pn_image_data->height>>1)) / (GLfloat) pn_image_data->width) + 0.5;
	   y2_pos[j] = (i * step) / pn_image_data->height;
        }
    }

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glLineWidth(opts[2].val.ival);
  glBegin(GL_LINE_STRIP);

  glColor4ubv(colour);

  /* draw the line. */
  for (i = 1; i < 128; i++)
    {
       glVertex2d(x_pos[i - 1], y_pos[i - 1]);
       glVertex2d(x_pos[i], y_pos[i]);
    }

  glEnd();

  if ( opts[0].val.ival == 0 )
    {
       glBegin(GL_LINE_STRIP);

       for (i = 1; i < 128; i++)
         {
           glVertex2d(x2_pos[i - 1] / pn_image_data->width, y2_pos[i - 1] / pn_image_data->height);
           glVertex2d(x2_pos[i] / pn_image_data->width, y2_pos[i] / pn_image_data->height);
         }

       glEnd();
    }

  glPopMatrix();

  g_free(x_pos);
  g_free(y_pos);
  g_free(x2_pos);
  g_free(y2_pos);
}

static void
wave_vertical_exec (const struct pn_actuator_option *opts,
		    gpointer data)
{
  wave_vertical_exec_lines(opts, data);
}

struct pn_actuator_desc builtin_wave_vertical =
{
  "wave_vertical", "Vertical Waveform",
  "Draws one or two waveforms vertically across "
  "the drawing surface",
  0, wave_vertical_opts,
  NULL, NULL, wave_vertical_exec
};

/* FIXME: allow for only 1 channel for wave_normalize & wave_smooth */
/* **************** wave_normalize **************** */
static struct pn_actuator_option_desc wave_normalize_opts[] =
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
wave_normalize_exec (const struct pn_actuator_option *opts,
		     gpointer data)
{
  int i, j, max=0;
  float denom;

  for (j=0; j<2; j++)
    {
      if ( !(opts[3].val.ival) || (opts[3].val.ival < 0 && j == 0) ||
	   (opts[3].val.ival > 0 && j == 1) ) {
	
	for (i=0; i<512; i++)
	  if (abs(pn_sound_data->pcm_data[j][i]) > max)
	    max = abs(pn_sound_data->pcm_data[j][i]);
	
	if (opts[0].val.ival > 0)
	  denom = max/(opts[0].val.ival<<8);
	else if (opts[1].val.fval > 0)
	  denom = max/(opts[1].val.fval * (pn_image_data->width<<8));
	else
	  denom = max/(opts[2].val.fval * (pn_image_data->height<<8));
	
	if (denom > 0)
	  for (i=0; i<512; i++)
	    pn_sound_data->pcm_data[j][i]
	      /= denom;
      }
    }
}

struct pn_actuator_desc builtin_wave_normalize =
{
  "wave_normalize", "Normalize Waveform Data",
  "Normalizes the waveform data used by the wave_* actuators",
  0, wave_normalize_opts,
  NULL, NULL, wave_normalize_exec
};

/* **************** wave_smooth **************** */
struct pn_actuator_option_desc wave_smooth_opts[] = 
{
  { "channels", "Which sound channel(s) to smooth: negative = channel 1, \n"
    "\tpositive = channel 2, 0 = both channels.",
    OPT_TYPE_INT, { ival: 0 } },
  {0}
};

static void
wave_smooth_exec (const struct pn_actuator_option *opts,
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
	    k = (pn_sound_data->pcm_data[j][i]<<3)
	      + (pn_sound_data->pcm_data[j][i+1]<<2)
	      + (pn_sound_data->pcm_data[j][i-1]<<2)
	      + (pn_sound_data->pcm_data[j][i+2]<<2)
	      + (pn_sound_data->pcm_data[j][i-2]<<2)
	      + (pn_sound_data->pcm_data[j][i+3]<<1)
	      + (pn_sound_data->pcm_data[j][i-3]<<1)
	      + (pn_sound_data->pcm_data[j][i+4]<<1)
	      + (pn_sound_data->pcm_data[j][i-4]<<1);
	    tmp[i] = k >> 5;
	  }
	memcpy (pn_sound_data->pcm_data[j]+4, tmp, sizeof (gint16) * 504);
      } 
    }
}

struct pn_actuator_desc builtin_wave_smooth =
{
  "wave_smooth", "Smooth Waveform Data",
  "Smooth out the waveform data used by the wave_* actuators",
  0, wave_smooth_opts,
  NULL, NULL, wave_smooth_exec
};

/* **************** wave_radial **************** */
static struct pn_actuator_option_desc wave_radial_opts[] =
{
  { "base_radius", " ", 
    OPT_TYPE_FLOAT, { fval: 0 } },
  {"value", "The colour value to use.", OPT_TYPE_COLOR, {cval: { 0, 192, 255 } } },
  {"width", "The line's width.", OPT_TYPE_INT, {ival: 1} },
  { NULL }
};

static void
wave_radial_exec (const struct pn_actuator_option *opts,
		  gpointer data)
{
  int i, x, y;
  GLfloat *x_pos, *y_pos;	/* dynamic tables which store the positions for the line */
  GLubyte colour[] = { opts[1].val.cval.r, opts[1].val.cval.g, opts[1].val.cval.b, 255 };

  x_pos = g_new0(GLfloat, 361);
  y_pos = g_new0(GLfloat, 361);
  
  for(i=0; i<360; i++)
    {
      x_pos[i] = (opts[0].val.fval + (pn_sound_data->pcm_data[0][(int)(i*(512.0/360.0))]))
	* cos_val[i];
      y_pos[i] = (opts[0].val.fval + (pn_sound_data->pcm_data[0][(int)(i*(512.0/360.0))]))
	* sin_val[i];
    }

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glLineWidth(opts[2].val.ival);
  glBegin(GL_LINES);

  glColor4ubv(colour);

  /* draw the line. */
  for (i = 1; i < 360; i++)
    {
       glVertex2d(x_pos[i - 1], y_pos[i - 1]);
       glVertex2d(x_pos[i], y_pos[i]);
    }

  glEnd();

  glPopMatrix();

  g_free(x_pos);
  g_free(y_pos);
};	       

struct pn_actuator_desc builtin_wave_radial =
{
  "wave_radial", "Radial Waveform",
  "Draws a single waveform varying"
  " radially from the center of the image",
  0, wave_radial_opts,
  NULL, NULL, wave_radial_exec
};

/* **************** wave_scope **************** */

static struct pn_actuator_option_desc wave_scope_opts[] =
{
  {"init_script", "Initialization script.", OPT_TYPE_STRING, {sval: "n = 800; t = -0.05;"} },
  {"frame_script", "Script to run at the beginning of each frame.", OPT_TYPE_STRING, {sval: "t = t + 0.05;"} },
  {"sample_script", "Script to run for each sample.", OPT_TYPE_STRING, {sval: "d = index + value; r = t + index * 3.141952924 * 4; x = cos(r) * d; y = sin(r) * d;"} },
  {"lines", "Use lines instead of dots.", OPT_TYPE_BOOLEAN, {bval: TRUE} },
  { NULL }
};

struct pn_scope_data
{
  expression_t *expr_on_init, *expr_on_frame, *expr_on_sample;
  symbol_dict_t *dict;
  gboolean reset;
};

static void
wave_scope_init(gpointer *data)
{
  *data = g_new0(struct pn_scope_data, 1);

  /* the expressions will need to be compiled, so prepare for that */
  ((struct pn_scope_data *)*data)->reset = TRUE;
}

static void
wave_scope_cleanup(gpointer op_data)
{
  struct pn_scope_data *data = (struct pn_scope_data *) op_data;

  g_return_if_fail(data != NULL);

  if (data->expr_on_init)
    expr_free(data->expr_on_init);

  if (data->expr_on_frame)
    expr_free(data->expr_on_frame);

  if (data->expr_on_sample)
    expr_free(data->expr_on_sample);

  if (data->dict)
    dict_free(data->dict);

  if (data)
    g_free(data);
}

static void
wave_scope_exec(const struct pn_actuator_option *opts,
		gpointer op_data)
{
  struct pn_scope_data *data = (struct pn_scope_data *) op_data;
  gint i;
  gdouble *xf, *yf, *index, *value, *points;

  if (data->reset)
    {
       if (data->dict)
         dict_free(data->dict);

       data->dict = dict_new();

       if (opts[0].val.sval != NULL)
         data->expr_on_init = expr_compile_string(opts[0].val.sval, data->dict);

       if (opts[1].val.sval != NULL)
         data->expr_on_frame = expr_compile_string(opts[1].val.sval, 
		data->dict);

       if (opts[2].val.sval != NULL)
         data->expr_on_sample = expr_compile_string(opts[2].val.sval, 
		data->dict);

       if (data->expr_on_init != NULL)
         expr_execute(data->expr_on_init, data->dict);

       data->reset = FALSE;
    }

  xf = dict_variable(data->dict, "x");
  yf = dict_variable(data->dict, "y");
  index = dict_variable(data->dict, "index");
  value = dict_variable(data->dict, "value");
  points = dict_variable(data->dict, "points");

  if (data->expr_on_frame != NULL)
    expr_execute(data->expr_on_frame, data->dict);

  if (*points > 513 || *points == 0)
      *points = 513;

  if (data->expr_on_sample != NULL)
    {
       glBegin(GL_LINE_STRIP);

       for (i = 0; i < *points; i++)
          {
             *value = 1.0 * pn_sound_data->pcm_data[0][i & 511] / 32768.0;
             *index = i / (*points - 1);

             expr_execute(data->expr_on_sample, data->dict);

             glVertex2d(*xf, *yf);
          }

       glEnd();
       glPopMatrix();
    }
}

struct pn_actuator_desc builtin_wave_scope =
{
  "wave_scope", "Scope",
  "A programmable scope.",
  0, wave_scope_opts,
  wave_scope_init, wave_scope_cleanup, wave_scope_exec
};
