#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include <glib.h>

#include "paranormal.h"
#include "actuators.h"
#include "pn_utils.h"

/* **************** freq_dots **************** */
/* FIXME: take this piece of crap out */
static void
freq_dots_exec (const struct pn_actuator_option *opts,
		gpointer data)
{
  int i, basex;

  basex = (pn_image_data->width>>1)-128;
  for (i=basex < 0 ? -basex : 0 ; i < 256; i++)
    {
      pn_image_data->surface[0][PN_IMG_INDEX (basex+i, (pn_image_data->height>>1)
					      - CAP (pn_sound_data->freq_data[0][i], 120))]
	= 0xff;
      pn_image_data->surface[0][PN_IMG_INDEX (basex+256-i, (pn_image_data->height>>1)
					      + CAP (pn_sound_data->freq_data[1][i], 120))]
	= 0xff;
    }
}

struct pn_actuator_desc builtin_freq_dots =
{
  "freq_dots",
  "Frequency Scope",
  "Draws dots varying vertically with the freqency data.",
  0, NULL,
  NULL, NULL, freq_dots_exec
};

/* **************** freq_drops **************** */
static void
freq_drops_exec (const struct pn_actuator_option *opts,
		gpointer data)
{
  int i,j;
  
  for (i=0; i<256; i++)
    for (j=0; j<pn_sound_data->freq_data[0][i]>>3; i++)
      pn_image_data->surface[0][PN_IMG_INDEX (rand() % pn_image_data->width,
					      rand() % pn_image_data->height)]
	= 0xff;
}

struct pn_actuator_desc builtin_freq_drops =
{
  "freq_drops",
  "Random Dots",
  "Draws dots at random on the image (louder music = more dots)",
  0, NULL,
  NULL, NULL, freq_drops_exec
};
