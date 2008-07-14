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

#include "paranormal.h"

/*
 * This algorithm is by Janusz Gregorcyzk, the implementation is
 * mine, however.
 *
 *   -- nenolod
 */
int
pn_is_new_beat(void)
{
  gint i;
  gint total = 0;
  gboolean ret = FALSE;
  static gint previous;

  for (i = 1; i < 512; i++)
    {
       total += abs (pn_sound_data->pcm_data[0][i] -
		     pn_sound_data->pcm_data[0][i - 1]);
    }

  total /= 512;

  ret = (total > (2 * previous));

  previous = total;

  return ret;
}
