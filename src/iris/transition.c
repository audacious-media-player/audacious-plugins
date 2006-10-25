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

/* Transitions on theme change
 * TODO : transitions on song change
 * new transitions are easy to add
 */

#include <stdlib.h>
#include "iris.h"

extern GLfloat y_angle;
extern GLfloat x_angle;
extern int max_transition_frames;
extern int transition_frames;
int transition = 0;

void
trans_zoom_out (gboolean init)
{
  static GLfloat x, y, z;
  //only init when starting transition
  if (init)
    {
      x = 1.0;
      y = 1.0;
      z = 1.0;
      return;
    }

  if (max_transition_frames / 2 < transition_frames)
    {
      x -= 1.0 / (max_transition_frames / 2);
      y -= 1.0 / (max_transition_frames / 2);
      z -= 1.0 / (max_transition_frames / 2);
    }
  else
    {
      x += 1.0 / (max_transition_frames / 2);
      y += 1.0 / (max_transition_frames / 2);
      z += 1.0 / (max_transition_frames / 2);
    }

  glScalef (x, y, z);
}

void
trans_zoom_in (gboolean init)
{
  static GLfloat x, y, z;
  //only init when starting transition
  if (init)
    {
      x = 1.0;
      y = 1.0;
      z = 1.0;
      return;
    }

  if (max_transition_frames / 2 < transition_frames)
    {
      x += 5.0 / (max_transition_frames / 2);
      y += 5.0 / (max_transition_frames / 2);
      z += 5.0 / (max_transition_frames / 2);
    }
  else
    {
      x -= 5.0 / (max_transition_frames / 2);
      y -= 5.0 / (max_transition_frames / 2);
      z -= 5.0 / (max_transition_frames / 2);
    }

  glScalef (x, y, z);
}

/* move up to top down look */
void
trans_vertical_view (gboolean init)
{
  static GLfloat x_angle_tmp;
  if (init)
    {
      x_angle_tmp = x_angle;
    }
  if (max_transition_frames / 2 < transition_frames)
    {
      x_angle += (90.0 - x_angle_tmp) / (max_transition_frames / 2);
    }
  else
    {
      x_angle -= (90.0 - x_angle_tmp) / (max_transition_frames / 2);
    }

}

void
trans_spin_half_and_back (gboolean cc)
{
  if (max_transition_frames / 2 < transition_frames)
    {
      if (cc)
	y_angle -= 180.0 / (max_transition_frames / 2);
      else
	y_angle += 180.0 / (max_transition_frames / 2);
    }
  else
    {
      if (cc)
	y_angle += 180.0 / (max_transition_frames / 2);
      else
	y_angle -= 180.0 / (max_transition_frames / 2);
    }
}

void
trans_spin_full (gboolean cc)
{
  if (cc)
    y_angle -= 360.0 / max_transition_frames;
  else
    y_angle += 360.0 / max_transition_frames;
}

/* called on themeswitch */
void
init_theme_transition (void)
{
  /* don't forget to update after adding new transitions */
  transition = (int) ((gfloat) 6 * rand () / (RAND_MAX + 1.0));

  /* init/reset transitions that need it */
  trans_zoom_in (TRUE);
  trans_zoom_out (TRUE);
  trans_vertical_view (TRUE);
}

/* called every frame of a transition */
void
theme_transition ()
{
  switch (transition)
    {
    case 0:
      trans_zoom_out (FALSE);
      break;
    case 1:
      trans_zoom_out (FALSE);
      trans_spin_half_and_back ((int)
				((gfloat) 1 * rand () / (RAND_MAX + 1.0)));
      break;
    case 2:
      trans_zoom_in (FALSE);
      break;
    case 3:
      trans_zoom_in (FALSE);
      trans_spin_half_and_back ((int)
				((gfloat) 1 * rand () / (RAND_MAX + 1.0)));
      break;
    case 4:
      trans_vertical_view (FALSE);
      trans_zoom_in (FALSE);
      break;
    case 5:
      trans_vertical_view (FALSE);
      trans_zoom_in (FALSE);
      trans_spin_full ((int) ((gfloat) 1 * rand () / (RAND_MAX + 1.0)));
      break;
    }
}
