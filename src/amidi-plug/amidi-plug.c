/*
*
* Author: Giacomo Lozito <james@develia.org>, (C) 2005-2007
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

#include <pthread.h>

#include <audacious/drct.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <libaudcore/hook.h>

#include "amidi-plug.h"

static void amidiplug_play_loop (InputPlayback * playback);

static gboolean amidiplug_play (InputPlayback * playback, const gchar *
 filename_uri, VFSFile * file, gint start_time, gint stop_time, gboolean pause);
static void amidiplug_pause (InputPlayback * playback, gboolean paused);
static void amidiplug_mseek (InputPlayback * playback, gint time);
static Tuple * amidiplug_get_song_tuple (const gchar * filename_uri, VFSFile *
 file);

AUD_INPUT_PLUGIN
(
    .name = N_("AMIDI-Plug (MIDI Player)"),
    .domain = PACKAGE,
    .init = amidiplug_init,
    .about = amidiplug_aboutbox,
    .configure = amidiplug_configure,
    .play = amidiplug_play,
    .stop = amidiplug_stop,
    .pause = amidiplug_pause,
    .mseek = amidiplug_mseek,
    .get_time = amidiplug_get_time,
    .get_volume = amidiplug_get_volume,
    .set_volume = amidiplug_set_volume,
    .cleanup = amidiplug_cleanup,
    .probe_for_tuple = amidiplug_get_song_tuple,
    .file_info_box = amidiplug_file_info_box,
    .is_our_file_from_vfs = amidiplug_is_our_file_from_vfs,
    .extensions = amidiplug_vfs_extensions,
)

static pthread_mutex_t control_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t control_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t audio_control_mutex = PTHREAD_MUTEX_INITIALIZER;

static gboolean initted;
static gint seek_time;

static pthread_t audio_thread;
static gboolean audio_stop_flag;
static gint audio_seek_time;

static volatile gint current_time = -1;

static gboolean amidiplug_init (void)
{
    initted = FALSE;
    return TRUE;
}

static void soft_init (void)
{
    pthread_mutex_lock (& control_mutex);

    if (! initted)
    {
        i_configure_cfg_ap_read ();
        i_backend_load (amidiplug_cfg_ap.ap_seq_backend);

        initted = TRUE;
    }

    pthread_mutex_unlock (& control_mutex);
}

static void amidiplug_cleanup (void)
{
    if (initted)
        i_backend_unload ();
}

static gint amidiplug_is_our_file_from_vfs( const gchar *filename_uri , VFSFile *fp )
{
  gchar magic_bytes[4];

  soft_init ();

  if ( fp == NULL )
    return FALSE;

  if ( VFS_FREAD( magic_bytes , 1 , 4 , fp ) != 4 )
    return FALSE;

  if ( !strncmp( magic_bytes , "MThd" , 4 ) )
  {
    DEBUGMSG( "MIDI found, %s is a standard midi file\n" , filename_uri );
    return TRUE;
  }

  if ( !strncmp( magic_bytes , "RIFF" , 4 ) )
  {
    /* skip the four bytes after RIFF,
       then read the next four */
    if ( VFS_FSEEK( fp , 4 , SEEK_CUR ) != 0 )
      return FALSE;

    if ( VFS_FREAD( magic_bytes , 1 , 4 , fp ) != 4 )
      return FALSE;

    if ( !strncmp( magic_bytes , "RMID" , 4 ) )
    {
      DEBUGMSG( "MIDI found, %s is a riff midi file\n" , filename_uri );
      return TRUE;
    }
  }

  return FALSE;
}

static void amidiplug_configure (void)
{
    soft_init ();
    i_configure_gui ();
}


static void amidiplug_aboutbox( void )
{
  i_about_gui();
}


static void amidiplug_file_info_box (const gchar * filename)
{
    soft_init ();
    i_fileinfo_gui (filename);
}


static void amidiplug_stop( InputPlayback * playback )
{
  DEBUGMSG( "STOP request at tick: %i\n" , midifile.playing_tick );

  pthread_mutex_lock (& control_mutex);
  amidiplug_playing_status = AMIDIPLUG_STOP;
  pthread_cond_broadcast (& control_cond);
  pthread_mutex_unlock (& control_mutex);
}

static void amidiplug_pause (InputPlayback * playback, gboolean paused)
{
    pthread_mutex_lock (& control_mutex);
    amidiplug_playing_status = paused ? AMIDIPLUG_PAUSE : AMIDIPLUG_PLAY;
    pthread_cond_broadcast (& control_cond);
    pthread_mutex_unlock (& control_mutex);
}

static void amidiplug_mseek (InputPlayback * playback, gint time)
{
    pthread_mutex_lock (& control_mutex);
    seek_time = time;
    pthread_cond_broadcast (& control_cond);
    pthread_mutex_unlock (& control_mutex);
}

static gint amidiplug_get_time (InputPlayback * playback)
{
    return current_time;
}

static gint amidiplug_get_volume( gint * l_p , gint * r_p )
{
  if ( backend.autonomous_audio == TRUE )
  {
    backend.audio_volume_get( l_p , r_p );
    return 1;
  }
  return 0;
}


static gint amidiplug_set_volume( gint  l , gint  r )
{
  if ( backend.autonomous_audio == TRUE )
  {
    backend.audio_volume_set( l , r );
    return 1;
  }
  return 0;
}


static Tuple * amidiplug_get_song_tuple (const gchar * filename_uri, VFSFile *
 file)
{
  /* song title, get it from the filename */
  Tuple *tuple = tuple_new_from_filename(filename_uri);
  midifile_t mf;

  soft_init ();

  if ( i_midi_parse_from_filename( filename_uri , &mf ) )
    tuple_set_int(tuple, FIELD_LENGTH, NULL, mf.length / 1000);

  i_midi_free( &mf );

  return tuple;
}


static gboolean amidiplug_play (InputPlayback * playback, const gchar *
 filename_uri, VFSFile * file, gint start_time, gint stop_time, gboolean pause)
{
  g_return_val_if_fail (file != NULL, FALSE);

  gint port_count = 0;
  gint au_samplerate = -1, au_bitdepth = -1, au_channels = -1;

  soft_init ();

  if ( backend.gmodule == NULL )
  {
    g_warning( "No sequencer backend selected\n" );
    /* not usable, cause now amidiplug_play is in a different thread
    i_message_gui( _("AMIDI-Plug - warning") ,
                   _("No sequencer backend has been selected!\nPlease configure AMIDI-Plug before playing.") ,
                   AMIDIPLUG_MESSAGE_WARN , NULL , TRUE );
    */
    amidiplug_playing_status = AMIDIPLUG_ERR;
    return FALSE;
  }

  /* get information about audio from backend, if available */
  backend.audio_info_get( &au_channels , &au_bitdepth , &au_samplerate );
  DEBUGMSG( "PLAY requested, audio details: channels -> %i , bitdepth -> %i , samplerate -> %i\n" ,
            au_channels , au_bitdepth , au_samplerate );

  if ( backend.autonomous_audio == FALSE )
  {
    DEBUGMSG( "PLAY requested, opening audio output plugin\n" );
    playback->output->open_audio( FMT_S16_NE , au_samplerate , au_channels );
  }

  DEBUGMSG( "PLAY requested, midifile init\n" );
  /* midifile init */
  i_midi_init( &midifile );

  /* get the number of selected ports */
  port_count = backend.seq_get_port_count();
  if ( port_count < 1 )
  {
    aud_interface_show_error (_("You have not selected any sequencer ports for "
     "MIDI playback.  You can do so in the MIDI plugin preferences."));
    amidiplug_playing_status = AMIDIPLUG_ERR;
    return FALSE;
  }

  DEBUGMSG( "PLAY requested, opening file: %s\n" , filename_uri );
  midifile.file_pointer = file;
  midifile.file_name = g_strdup(filename_uri);

  switch( i_midi_file_read_id( &midifile ) )
  {
    case MAKE_ID('R', 'I', 'F', 'F'):
    {
      DEBUGMSG( "PLAY requested, RIFF chunk found, processing...\n" );
      /* read riff chunk */
      if ( !i_midi_file_parse_riff( &midifile ) )
        WARNANDBREAKANDPLAYERR( "%s: invalid file format (riff parser)\n" , filename_uri );

      /* if that was read correctly, go ahead and read smf data */
    }

    case MAKE_ID('M', 'T', 'h', 'd'):
    {
      DEBUGMSG( "PLAY requested, MThd chunk found, processing...\n" );
      if ( !i_midi_file_parse_smf( &midifile , port_count ) )
        WARNANDBREAKANDPLAYERR( "%s: invalid file format (smf parser)\n" , filename_uri );

      if ( midifile.time_division < 1 )
        WARNANDBREAKANDPLAYERR( "%s: invalid time division (%i)\n" , filename_uri , midifile.time_division );

      DEBUGMSG( "PLAY requested, setting ppq and tempo...\n" );
      /* fill midifile.ppq and midifile.tempo using time_division */
      if ( !i_midi_setget_tempo( &midifile ) )
        WARNANDBREAKANDPLAYERR( "%s: invalid values while setting ppq and tempo\n" , filename_uri );

      DEBUGMSG( "PLAY requested, sequencer start\n" );
      /* sequencer start */
      if ( !backend.seq_start( filename_uri ) )
        WARNANDBREAKANDPLAYERR( "%s: problem with seq_start, play aborted\n" , filename_uri );

      DEBUGMSG( "PLAY requested, sequencer on\n" );
      /* sequencer on */
      if ( !backend.seq_on() )
        WARNANDBREAKANDPLAYERR( "%s: problem with seq_on, play aborted\n" , filename_uri );

      DEBUGMSG( "PLAY requested, setting sequencer queue tempo...\n" );
      /* set sequencer queue tempo using ppq and tempo (call only after i_midi_setget_tempo) */
      if ( !backend.seq_queue_tempo( midifile.current_tempo , midifile.ppq ) )
      {
        backend.seq_off(); /* kill the sequencer */
        WARNANDBREAKANDPLAYERR( "%s: ALSA queue problem, play aborted\n" , filename_uri );
      }

      /* fill midifile.length, keeping in count tempo-changes */
      i_midi_setget_length( &midifile );
      DEBUGMSG( "PLAY requested, song length calculated: %i msec\n" , (gint)(midifile.length / 1000) );

      playback->set_params (playback, au_bitdepth * au_samplerate * au_channels
       / 8, au_samplerate, au_channels);

      /* done with file */
      midifile.file_pointer = NULL;

      /* play play play! */
      DEBUGMSG( "PLAY requested, starting play thread\n" );
      amidiplug_playing_status = pause ? AMIDIPLUG_PAUSE : AMIDIPLUG_PLAY;
      seek_time = (start_time > 0) ? start_time : -1;
      playback->set_pb_ready(playback);
      amidiplug_play_loop(playback);
      break;
    }

    default:
    {
      amidiplug_playing_status = AMIDIPLUG_ERR;
      g_warning( "%s is not a Standard MIDI File\n" , filename_uri );
      break;
    }
  }

  return TRUE;
}

static void * audio_loop (void * arg)
{
    InputPlayback * playback = arg;
    void * buffer = NULL;
    gint buffer_size = 0;

    while (1)
    {
        pthread_mutex_lock (& audio_control_mutex);

        if (audio_stop_flag)
        {
            pthread_mutex_unlock (& audio_control_mutex);
            break;
        }

        if (audio_seek_time != -1)
        {
            playback->output->flush (audio_seek_time);
            audio_seek_time = -1;
        }

        pthread_mutex_unlock (& audio_control_mutex);

        if (backend.seq_output (& buffer, & buffer_size))
            playback->output->write_audio (buffer, buffer_size);
    }

    g_free (buffer);
    return NULL;
}

static void audio_start (InputPlayback * playback)
{
    audio_stop_flag = FALSE;
    audio_seek_time = -1;
    pthread_create (& audio_thread, NULL, audio_loop, (void *) playback);
}

static void audio_seek (InputPlayback * playback, gint time)
{
    pthread_mutex_lock (& audio_control_mutex);
    audio_seek_time = time;
    playback->output->abort_write ();
    pthread_mutex_unlock (& audio_control_mutex);
}

static void audio_pause (InputPlayback * playback, gboolean pause)
{
    playback->output->pause (pause);
}

static void audio_stop (InputPlayback * playback)
{
    pthread_mutex_lock (& audio_control_mutex);
    audio_stop_flag = TRUE;
    playback->output->abort_write ();
    pthread_mutex_unlock (& audio_control_mutex);
    pthread_join (audio_thread, NULL);
}

static void do_seek (gint time)
{
    backend.seq_event_allnoteoff (midifile.playing_tick);
    backend.seq_queue_stop ();
    amidiplug_skipto (time * (gint64) 1000 / midifile.avg_microsec_per_tick);
}

static void do_pause (gboolean pause)
{
    if (pause)
    {
        backend.seq_event_allnoteoff (midifile.playing_tick);
        backend.seq_queue_stop ();
    }
    else
    {
        midifile.skip_offset = midifile.playing_tick;
        backend.seq_queue_start ();
    }
}

static void amidiplug_play_loop (InputPlayback * playback)
{
  gint j = 0;
  gboolean rewind = TRUE, paused = FALSE, stopped = FALSE;

  if ( rewind )
  {
    /* initialize current position in each track */
    for (j = 0; j < midifile.num_tracks; ++j)
      midifile.tracks[j].current_event = midifile.tracks[j].first_event;
  }

  if (! backend.autonomous_audio)
      audio_start (playback);

  /* queue start */
  backend.seq_queue_start();
  /* common settings for all our events */
  backend.seq_event_init();

  DEBUGMSG( "PLAY thread, start the play loop\n" );
  for (;;)
  {
    midievent_t * event = NULL;
    midifile_track_t * event_track = NULL;
    gint i, min_tick = midifile.max_tick + 1;

    pthread_mutex_lock (& control_mutex);

    if (amidiplug_playing_status == AMIDIPLUG_STOP)
    {
        pthread_mutex_unlock (& control_mutex);
        stopped = TRUE;
        break;
    }

    if (seek_time != -1)
    {
        if (! backend.autonomous_audio)
            audio_seek (playback, seek_time);

        do_seek (seek_time);
        seek_time = -1;
    }

    if (amidiplug_playing_status == AMIDIPLUG_PAUSE)
    {
        if (! paused)
        {
            do_pause (TRUE);

            if (! backend.autonomous_audio)
                audio_pause (playback, TRUE);

            paused = TRUE;
        }

        pthread_cond_wait (& control_cond, & control_mutex);
        pthread_mutex_unlock (& control_mutex);
        continue;
    }

    if (paused)
    {
        do_pause (FALSE);

        if (! backend.autonomous_audio)
            audio_pause (playback, FALSE);

        paused = FALSE;
    }

    pthread_mutex_unlock (& control_mutex);

    /* search next event */
    for (i = 0; i < midifile.num_tracks; ++i)
    {
      midifile_track_t * track = &midifile.tracks[i];
      midievent_t * e2 = track->current_event;
      if (( e2 ) && ( e2->tick < min_tick ))
      {
        min_tick = e2->tick;
        event = e2;
        event_track = track;
      }
    }

    if (!event)
      break; /* end of song reached */

    /* advance pointer to next event */
    event_track->current_event = event->next;
    /* consider the midifile.skip_offset */
    event->tick_real = event->tick - midifile.skip_offset;


    switch (event->type)
    {
      case SND_SEQ_EVENT_NOTEON:
        backend.seq_event_noteon( event );
        break;
      case SND_SEQ_EVENT_NOTEOFF:
        backend.seq_event_noteoff( event );
        break;
      case SND_SEQ_EVENT_KEYPRESS:
        backend.seq_event_keypress( event );
        break;
      case SND_SEQ_EVENT_CONTROLLER:
        backend.seq_event_controller( event );
        break;
      case SND_SEQ_EVENT_PGMCHANGE:
        backend.seq_event_pgmchange( event );
        break;
      case SND_SEQ_EVENT_CHANPRESS:
        backend.seq_event_chanpress( event );
        break;
      case SND_SEQ_EVENT_PITCHBEND:
        backend.seq_event_pitchbend( event );
        break;
      case SND_SEQ_EVENT_SYSEX:
        backend.seq_event_sysex( event );
        break;
      case SND_SEQ_EVENT_TEMPO:
        backend.seq_event_tempo( event );
        DEBUGMSG( "PLAY thread, processing tempo event with value %i on tick %i\n" ,
                  event->data.tempo , event->tick );
        midifile.current_tempo = event->data.tempo;
        break;
      case SND_SEQ_EVENT_META_TEXT:
        /* do nothing */
        break;
      case SND_SEQ_EVENT_META_LYRIC:
        /* do nothing */
        break;
      default:
        DEBUGMSG( "PLAY thread, encountered invalid event type %i\n" , event->type );
        break;
    }

    midifile.playing_tick = event->tick;

    if (backend.autonomous_audio)
    {
      current_time = (gint64) midifile.playing_tick *
       midifile.avg_microsec_per_tick / 1000;

      /* these backends deal with audio production themselves (i.e. ALSA) */
      backend.seq_output( NULL , NULL );
    }
  }

  backend.seq_output_shut (stopped ? midifile.playing_tick : midifile.max_tick,
   midifile.skip_offset);

  if (! backend.autonomous_audio)
      audio_stop (playback);

  backend.seq_off ();
  backend.seq_stop ();
  i_midi_free (& midifile);

  current_time = -1;
}


/* amidigplug_skipto: re-do all events that influence the playing of our
   midi file; re-do them using a time-tick of 0, so they are processed
   istantaneously and proceed this way until the playing_tick is reached;
   also obtain the correct skip_offset from the playing_tick */
void amidiplug_skipto( gint playing_tick )
{
  gint i;

  /* this check is always made, for safety*/
  if ( playing_tick >= midifile.max_tick )
    playing_tick = midifile.max_tick - 1;

  /* initialize current position in each track */
  for (i = 0; i < midifile.num_tracks; ++i)
    midifile.tracks[i].current_event = midifile.tracks[i].first_event;

  /* common settings for all our events */
  backend.seq_event_init();
  backend.seq_queue_start();

  DEBUGMSG( "SKIPTO request, starting skipto loop\n" );
  for (;;)
  {
    midievent_t * event = NULL;
    midifile_track_t * event_track = NULL;
    gint i, min_tick = midifile.max_tick + 1;

    /* search next event */
    for (i = 0; i < midifile.num_tracks; ++i)
    {
      midifile_track_t * track = &midifile.tracks[i];
      midievent_t * e2 = track->current_event;
      if (( e2 ) && ( e2->tick < min_tick ))
      {
        min_tick = e2->tick;
        event = e2;
        event_track = track;
      }
    }

    /* unlikely here... unless very strange MIDI files are played :) */
    if (!event)
    {
      DEBUGMSG( "SKIPTO request, reached the last event but not the requested tick (!)\n" );
      break; /* end of song reached */
    }

    /* reached the requested tick, job done */
    if ( event->tick >= playing_tick )
    {
      DEBUGMSG( "SKIPTO request, reached the requested tick, exiting from skipto loop\n" );
      break;
    }

    /* advance pointer to next event */
    event_track->current_event = event->next;
    /* set the time tick to 0 */
    event->tick_real = 0;

    switch (event->type)
    {
      /* do nothing for these
      case SND_SEQ_EVENT_NOTEON:
      case SND_SEQ_EVENT_NOTEOFF:
      case SND_SEQ_EVENT_KEYPRESS:
      {
        break;
      } */
      case SND_SEQ_EVENT_CONTROLLER:
        backend.seq_event_controller( event );
        break;
      case SND_SEQ_EVENT_PGMCHANGE:
        backend.seq_event_pgmchange( event );
        break;
      case SND_SEQ_EVENT_CHANPRESS:
        backend.seq_event_chanpress( event );
        break;
      case SND_SEQ_EVENT_PITCHBEND:
        backend.seq_event_pitchbend( event );
        break;
      case SND_SEQ_EVENT_SYSEX:
        backend.seq_event_sysex( event );
        break;
      case SND_SEQ_EVENT_TEMPO:
        backend.seq_event_tempo( event );
        midifile.current_tempo = event->data.tempo;
        break;
    }

    if ( backend.autonomous_audio == TRUE )
    {
      /* these backends deal with audio production themselves (i.e. ALSA) */
      backend.seq_output( NULL , NULL );
    }
  }

  midifile.skip_offset = playing_tick;
  midifile.playing_tick = playing_tick;
}
