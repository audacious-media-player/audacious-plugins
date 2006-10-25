/*  Iris - visualization plugin for XMMS
 *  Copyright (C) 2000-2002 Cédric DELFOSSE (cdelfosse@free.fr)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "iris.h"


void
bar_top_or_bottom (GLfloat height, xz * xz1, xz * xz2, xz * xz3, xz * xz4)
{
  glVertex3f (xz1->x, height, xz1->z);
  glVertex3f (xz2->x, height, xz2->z);
  glVertex3f (xz4->x, height, xz4->z);
  glVertex3f (xz3->x, height, xz3->z);
}


void
bar_side (GLfloat height, xz * xz1, xz * xz2)
{
  glVertex3f (xz1->x, height, xz1->z);
  glVertex3f (xz1->x, 0, xz1->z);
  glVertex3f (xz2->x, 0, xz2->z);

  glVertex3f (xz2->x, height, xz2->z);
}


void
bar_full (GLfloat height, xz * xz1, xz * xz2, xz * xz3, xz * xz4)
{
  bar_top_or_bottom (height, xz1, xz2, xz3, xz4);
  bar_side (height, xz1, xz2);
  bar_side (height, xz3, xz4);
  bar_side (height, xz1, xz2);
  bar_side (height, xz3, xz4);
}
