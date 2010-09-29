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
#define AMIDIPLUG_SEEK  3
#define AMIDIPLUG_ERR	4

#include "i_common.h"
#include <audacious/plugin.h>

#include "i_vfslayer.h"
#include "i_backend.h"
#include "i_configure.h"
#include "i_midi.h"
#include "i_fileinfo.h"
#include "i_utils.h"

static GMutex * amidiplug_gettime_mutex = NULL;
static GMutex * amidiplug_playing_mutex = NULL;

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

gpointer amidiplug_play_loop( gpointer );
void amidiplug_skipto( gint );
static gboolean amidiplug_init (void);
static void amidiplug_cleanup( void );
static void amidiplug_aboutbox( void );
static void amidiplug_configure( void );
static gint amidiplug_is_our_file_from_vfs( const gchar * , VFSFile * );
static void amidiplug_play( InputPlayback * );
static void amidiplug_stop( InputPlayback * );
static void amidiplug_pause( InputPlayback *, gshort );
static void amidiplug_mseek (InputPlayback * playback, gulong time);
static gint amidiplug_get_time( InputPlayback * );
static gint amidiplug_get_volume( gint * , gint * );
static gint amidiplug_set_volume( gint , gint );
static Tuple *amidiplug_get_song_tuple( const gchar * );
static void amidiplug_file_info_box( const gchar * );

InputPlugin amidiplug_ip =
{
  .description = "AMIDI-Plug " AMIDIPLUG_VERSION " (MIDI Player)", /* description */
  .init = amidiplug_init,			/* init */
  .about = amidiplug_aboutbox,			/* aboutbox */
  .configure = amidiplug_configure,			/* configure */
  .play_file = amidiplug_play,			/* play_file */
  .stop = amidiplug_stop,			/* stop */
  .pause = amidiplug_pause,			/* pause */
  .mseek = amidiplug_mseek,			/* seek */
  .get_time = amidiplug_get_time,			/* get_time */
  .get_volume = amidiplug_get_volume,			/* get_volume */
  .set_volume = amidiplug_set_volume,			/* set_volume */
  .cleanup = amidiplug_cleanup,			/* cleanup */
  .get_song_tuple = amidiplug_get_song_tuple,		/* get_song_info */
  .file_info_box = amidiplug_file_info_box,		/* file_info_box */
  .is_our_file_from_vfs = amidiplug_is_our_file_from_vfs,	/* is_our_file_from_vfs */
  .vfs_extensions = amidiplug_vfs_extensions		/* vfs_extensions */
};

#endif /* !_I_AMIDIPLUG_H */
