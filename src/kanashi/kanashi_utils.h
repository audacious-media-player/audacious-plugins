/*
 * kanashi: iterated javascript-driven visualization engine
 * Copyright (c) 2009 William Pitcock <nenolod@dereferenced.org>
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

#ifndef _PN_UTILS_H
#define _PN_UTILS_H

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define CAP(i,c) (i > c ? c : i < -(c) ? -(c) : i)
#define CAPHILO(i,h,l) (i > h ? h : i < l ? l : i)
#define CAPHI(i,h) (i > h ? h : i)
#define CAPLO(i,l) (i < l ? l : i)

#define PN_IMG_INDEX(x,y) ((x) + (kanashi_image_data->width * (y)))

struct kanashi_color;

extern void kanashi_mosaic(gint amt);
extern void kanashi_fade(gint amt);
extern void kanashi_blur(void);
extern void kanashi_invert(void);

extern void kanashi_set_colormap_gradient(gint low_index, gint high_index, struct kanashi_color *color);

extern void kanashi_render_horizontal_waveform(gint channels, guchar value);
extern void kanashi_render_vertical_waveform(gint channels, guchar value);

#endif /* _PN_UTILS_H */
