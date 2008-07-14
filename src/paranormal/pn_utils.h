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

#ifndef _PN_UTILS_H
#define _PN_UTILS_H

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define CAP(i,c) (i > c ? c : i < -(c) ? -(c) : i)
#define CAPHILO(i,h,l) (i > h ? h : i < l ? l : i)
#define CAPHI(i,h) (i > h ? h : i)
#define CAPLO(i,l) (i < l ? l : i)

#define PN_IMG_INDEX(x,y) ((x) + (pn_image_data->width * (y)))

#endif /* _PN_UTILS_H */
