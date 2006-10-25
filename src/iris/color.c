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

#include <stdlib.h>
#include "iris.h"

void
get_color (GLfloat * red, GLfloat * green, GLfloat * blue, GLfloat * signal)
{
  switch (config.color_mode)
    {
    case 0:
      {
	*red = config.color_red;
	*green = config.color_green;
	*blue = config.color_blue;
	break;
      }
    case 1:
      {
	*red =
	  config.color1_red +
	  ((*signal) * (config.color2_red - config.color1_red));
	*green =
	  config.color1_green +
	  ((*signal) * (config.color2_green - config.color1_green));
	*blue =
	  config.color1_blue +
	  ((*signal) * (config.color2_blue - config.color1_blue));
	break;
      }
    case 2:
      {
	*red = 1.0 * rand () / (RAND_MAX + 1.0);
	*green = 1.0 * rand () / (RAND_MAX + 1.0);
	*blue = 1.0 * rand () / (RAND_MAX + 1.0);
	break;
      }
    }
}
