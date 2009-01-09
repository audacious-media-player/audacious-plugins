/*
 * kanashi: iterated javascript-driven visualization plugin
 * Copyright (c) 2009 William Pitcock <nenolod@dereferenced.org>
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

#ifndef _KANASHI_H
#define _KANASHI_H

#define XP_UNIX	/* XXX */

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <mozjs/jsapi.h>

struct kanashi_color
{
  guchar r, g, b, a;
};

struct kanashi_sound_data
{
  gint16 pcm_data[2][512];
  gint16 freq_data[2][256];
};

struct kanashi_image_data
{
  int width, height;
  struct kanashi_color cmap[256];
  guchar *surface[2];
};

/* The executable (ie xmms.c or standalone.c)
   is responsible for allocating this and filling
   it with default/saved values */
struct kanashi_rc
{
  struct kanashi_actuator *actuator;
};

/* core funcs */
gboolean kanashi_init (void);
void kanashi_cleanup (void);
void kanashi_render (void);
void kanashi_swap_surfaces (void);
gboolean kanashi_load_preset (const char *filename);

/* Implemented elsewhere (ie xmms.c or standalone.c) */
void kanashi_set_rc ();
void kanashi_fatal_error (const char *fmt, ...);
void kanashi_error (const char *fmt, ...);
void kanashi_quit (void);

/* Implimented in cfg.c */
void kanashi_configure (void);

/* globals used for rendering */
extern struct kanashi_rc         *kanashi_rc;
extern struct kanashi_sound_data *kanashi_sound_data;
extern struct kanashi_image_data *kanashi_image_data;

extern gboolean kanashi_new_beat;

/* global trig pre-computes */
extern float sin_val[360];
extern float cos_val[360];

/* beat detection */
int kanashi_is_new_beat(void);

#endif /* _KANASHI_H */
