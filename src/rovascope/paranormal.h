#ifndef _PARANORMAL_H
#define _PARANORMAL_H

#include <glib.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "actuators.h"

struct pn_sound_data
{
  gint16 pcm_data[2][512];
  gint16 freq_data[2][256];
};

struct pn_image_data
{
  int width, height;
  struct pn_color cmap[256];
  guchar *surface[2];
};

/* The executable (ie xmms.c or standalone.c)
   is responsible for allocating this and filling
   it with default/saved values */
struct pn_rc
{
  struct pn_actuator *actuator;
};

/* core funcs */
void pn_init (void);
void pn_cleanup (void);
void pn_render (void);
void pn_swap_surfaces (void);

/* Implemented elsewhere (ie xmms.c or standalone.c) */
void pn_set_rc ();
void pn_fatal_error (const char *fmt, ...);
void pn_error (const char *fmt, ...);
void pn_quit (void);

/* Implimented in cfg.c */
void pn_configure (void);

/* globals used for rendering */
extern struct pn_rc         *pn_rc;
extern struct pn_sound_data *pn_sound_data;
extern struct pn_image_data *pn_image_data;

extern gboolean pn_new_beat;

/* global trig pre-computes */
extern float sin_val[360];
extern float cos_val[360];

/* beat detection */
int pn_is_new_beat(void);

struct pn_actuator *rovascope_get_random_colourmap(void);
struct pn_actuator *rovascope_get_random_general(void);
struct pn_actuator *rovascope_get_random_normal_scope(void);

struct pn_actuator *rovascope_get_random_actuator(void);

#endif /* _PARANORMAL_H */
