/*
*
* Author: Giacomo Lozito <james@develia.org>, (C) 2005-2006
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*
*/

#ifndef _I_COMMON_H
#define _I_COMMON_H 1

#include <stdio.h>

/* #define DEBUG */

#define WARNANDBREAK(...) { fprintf (stderr, __VA_ARGS__); break; }
#define WARNANDBREAKANDPLAYERR(...) { amidiplug_playing_status = AMIDIPLUG_ERR; WARNANDBREAK(__VA_ARGS__) }

#ifdef DEBUG
#define DEBUGMSG(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUGMSG(...)
#endif /* DEBUG */

/* multi-purpose data bucket */
typedef struct
{
  int bint[2];
  char * bcharp[2];
  void * bpointer[2];
}
data_bucket_t;

#endif /* !_I_COMMON_H */
