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

#ifndef _I_AMIDIPLUG_H
#define _I_AMIDIPLUG_H 1

#define AMIDIPLUG_STOP	0
#define AMIDIPLUG_PLAY	1
#define AMIDIPLUG_PAUSE	2
#define AMIDIPLUG_ERR	4

#include "i_common.h"
#include <audacious/plugin.h>

#include "i_vfslayer.h"
#include "i_backend.h"
#include "i_configure.h"
#include "i_midi.h"
#include "i_fileinfo.h"
#include "i_utils.h"

gint amidiplug_playing_status = AMIDIPLUG_STOP;

midifile_t midifile;

amidiplug_sequencer_backend_t backend;

amidiplug_cfg_ap_t amidiplug_cfg_ap =
{
  NULL,		/* ap_seq_backend */
  0,		/* ap_opts_transpose_value */
  0,		/* ap_opts_drumshift_value */
  0,		/* ap_opts_length_precalc */
  0,		/* ap_opts_comments_extract */
  0		/* ap_opts_lyrics_extract */
};

static const gchar * const amidiplug_vfs_extensions[] = {"mid", "midi", "rmi",
 "rmid", NULL};

void amidiplug_skipto( gint );
static gboolean amidiplug_init (void);
static void amidiplug_cleanup( void );
static void amidiplug_aboutbox( void );
static void amidiplug_configure( void );
static gint amidiplug_is_our_file_from_vfs( const gchar * , VFSFile * );
static void amidiplug_stop( InputPlayback * );
static gint amidiplug_get_time( InputPlayback * );
static gint amidiplug_get_volume( gint * , gint * );
static gint amidiplug_set_volume( gint , gint );
static void amidiplug_file_info_box( const gchar * );

#endif /* !_I_AMIDIPLUG_H */
