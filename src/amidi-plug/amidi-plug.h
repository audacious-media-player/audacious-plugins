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
#define AMIDIPLUG_ERR	3

#include "i_common.h"
#include <audacious/plugin.h>
#include <audacious/output.h>
#include <audacious/auddrct.h>
#include <pthread.h>
#include "i_vfslayer.h"
#include "i_backend.h"
#include "i_configure.h"
#include "i_midi.h"
#include "i_fileinfo.h"
#include "i_utils.h"


static pthread_t amidiplug_play_thread;
static pthread_t amidiplug_audio_thread;
static pthread_mutex_t amidiplug_gettime_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t amidiplug_playing_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t amidiplug_playing_cond = PTHREAD_COND_INITIALIZER;

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

gchar *amidiplug_vfs_extensions[] = { "mid" , "midi" , "rmi" , "rmid" , NULL };

void * amidiplug_play_loop( void * );
void * amidiplug_audio_loop( void * );
void amidiplug_skipto( gint );
static void amidiplug_init( void );
static void amidiplug_cleanup( void );
static void amidiplug_aboutbox( void );
static void amidiplug_configure( void );
static gint amidiplug_is_our_file( gchar * );
static gint amidiplug_is_our_file_from_vfs( gchar * , VFSFile * );
static void amidiplug_play( InputPlayback * );
static void amidiplug_stop( InputPlayback * );
static void amidiplug_pause( InputPlayback *, gshort );
static void amidiplug_seek( InputPlayback *, gint );
static gint amidiplug_get_time( InputPlayback * );
static gint amidiplug_get_volume( gint * , gint * );
static gint amidiplug_set_volume( gint , gint );
static void amidiplug_get_song_info( gchar * , gchar ** , gint * );
static void amidiplug_file_info_box( gchar * );

InputPlugin amidiplug_ip =
{
  NULL,					/* handle */
  NULL,					/* filename */
  "AMIDI-Plug " AMIDIPLUG_VERSION " (MIDI Player)", /* description */
  amidiplug_init,			/* init */
  amidiplug_aboutbox,			/* aboutbox */
  amidiplug_configure,			/* configure */
  amidiplug_is_our_file,		/* is_our_file */
  NULL,					/* scan_dir */
  amidiplug_play,			/* play_file */
  amidiplug_stop,			/* stop */
  amidiplug_pause,			/* pause */
  amidiplug_seek,			/* seek */
  NULL,					/* set_eq */
  amidiplug_get_time,			/* get_time */
  amidiplug_get_volume,			/* get_volume */
  amidiplug_set_volume,			/* set_volume */
  amidiplug_cleanup,			/* cleanup */
  NULL,					/* get_vis_type */
  NULL,					/* add_vis_pcm */
  NULL,					/* set_info */
  NULL,					/* set_info_text */
  amidiplug_get_song_info,		/* get_song_info */
  amidiplug_file_info_box,		/* file_info_box */
  NULL,					/* output */
  NULL,					/* get_song_tuple */
  NULL,					/* set_song_tuple */
  NULL,					/* set_status_buffering */
  amidiplug_is_our_file_from_vfs,	/* is_our_file_from_vfs */
  amidiplug_vfs_extensions		/* vfs_extensions */
};

#endif /* !_I_AMIDIPLUG_H */
