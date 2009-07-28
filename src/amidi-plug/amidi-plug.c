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

#include "amidi-plug.h"
#include <audacious/plugin.h>

InputPlugin *amidiplug_iplist[] = { &amidiplug_ip, NULL };

DECLARE_PLUGIN(amidi-plug, NULL, NULL, amidiplug_iplist, NULL, NULL, NULL, NULL, NULL);

static gint amidiplug_is_our_file_from_vfs( const gchar *filename_uri , VFSFile *fp )
{
  gchar magic_bytes[4];

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

static void amidiplug_init( void )
{
  g_log_set_handler(NULL , G_LOG_LEVEL_WARNING , g_log_default_handler , NULL);
  
  amidiplug_gettime_mutex = g_mutex_new();
  amidiplug_playing_mutex = g_mutex_new();
  amidiplug_pause_cond = g_cond_new();
  amidiplug_seekonpause_cond = g_cond_new();
  
  DEBUGMSG( "init, read configuration\n" );
  /* read configuration for amidi-plug */
  i_configure_cfg_ap_read();
  amidiplug_playing_status = AMIDIPLUG_STOP;
  backend.gmodule = NULL;
  /* load the backend selected by user */
  i_backend_load( amidiplug_cfg_ap.ap_seq_backend );
}


static void amidiplug_cleanup( void )
{
  i_backend_unload(); /* unload currently loaded backend */
  
  g_mutex_free(amidiplug_gettime_mutex);
  g_mutex_free(amidiplug_playing_mutex);
  g_cond_free(amidiplug_pause_cond);
  g_cond_free(amidiplug_seekonpause_cond);
  amidiplug_pause_cond = NULL;
  amidiplug_seekonpause_cond = NULL;
}


static void amidiplug_configure( void )
{
  /* display the nice config dialog */
  DEBUGMSG( "opening config system\n" );
  i_configure_gui();
}


static void amidiplug_aboutbox( void )
{
  i_about_gui();
}


static void amidiplug_file_info_box( const gchar * filename_uri )
{
  i_fileinfo_gui( filename_uri );
}


static void amidiplug_stop( InputPlayback * playback )
{
  DEBUGMSG( "STOP request at tick: %i\n" , midifile.playing_tick );

  g_mutex_lock( amidiplug_playing_mutex );
  amidiplug_playing_status = AMIDIPLUG_STOP;
  g_cond_signal( amidiplug_pause_cond );
  g_mutex_unlock( amidiplug_playing_mutex );
  if ( amidiplug_play_thread )
  {
    g_thread_join( amidiplug_play_thread );
    amidiplug_play_thread = NULL;
  }
  if (( backend.autonomous_audio == FALSE ) && ( amidiplug_audio_thread ))
  {
    g_thread_join( amidiplug_audio_thread );
    amidiplug_audio_thread = NULL;
  }
  DEBUGMSG( "STOP activated (play thread joined)\n" );

  /* kill the sequencer (while it may have been already killed if coming
     from pause, it's safe to do anyway since it checks for multiple calls) */
  if ( backend.gmodule != NULL )
    backend.seq_off();

  /* call seq_stop */
  if ( backend.gmodule != NULL )
    backend.seq_stop();

  /* close audio if current backend works with output plugin */
  if (( backend.gmodule != NULL ) && ( backend.autonomous_audio == FALSE ))
  {
    DEBUGMSG( "STOP activated, closing audio output plugin\n" );
    playback->output->buffer_free();
    playback->output->buffer_free();
    playback->output->close_audio();
  }
  /* free midi data (if it has not been freed yet) */
  i_midi_free( &midifile );
}


static void amidiplug_pause( InputPlayback * playback, gshort paused )
{
  if ( paused )
  {
    g_mutex_lock( amidiplug_playing_mutex );
    if ( amidiplug_playing_status == AMIDIPLUG_SEEK )
    {
      DEBUGMSG( "handle SEEK ON PAUSE situation\n" , midifile.playing_tick );
      /* uh oh, we have a pending seek; this can happen when a seek
         is requested while playback is paused; wait for that seek to
         be completed before pausing the song again */
      while ( amidiplug_playing_status != AMIDIPLUG_PLAY )
        g_cond_wait( amidiplug_seekonpause_cond , amidiplug_playing_mutex );
    }
    g_mutex_unlock( amidiplug_playing_mutex );
    DEBUGMSG( "PAUSE request at tick: %i\n" , midifile.playing_tick );
    g_mutex_lock( amidiplug_playing_mutex );
    amidiplug_playing_status = AMIDIPLUG_PAUSE;
    g_mutex_unlock( amidiplug_playing_mutex );
    if ( backend.autonomous_audio == FALSE )
      playback->output->pause(paused);
  }
  else
  {
    DEBUGMSG( "PAUSE deactivated, resume playing from tick %i\n" , midifile.playing_tick );
    if ( backend.autonomous_audio == FALSE )
      playback->output->pause(paused);
    g_mutex_lock( amidiplug_playing_mutex );
    amidiplug_playing_status = AMIDIPLUG_PLAY;
    g_cond_signal( amidiplug_pause_cond );
    /* wait for unpause operation to be completed */
    g_cond_wait( amidiplug_pause_cond , amidiplug_playing_mutex );
    g_mutex_unlock( amidiplug_playing_mutex );
  }
}


static void amidiplug_seek( InputPlayback * playback, gint time )
{
  DEBUGMSG( "SEEK requested (time %i)...\n" , time );
  g_mutex_lock( amidiplug_playing_mutex );
  midifile.seeking_tick = (gint)((time * 1000000) / midifile.avg_microsec_per_tick);
  amidiplug_playing_status = AMIDIPLUG_SEEK;
  g_mutex_unlock( amidiplug_playing_mutex );

  if ( backend.autonomous_audio == FALSE )
    playback->output->flush(time * 1000);
}


static gint amidiplug_get_time( InputPlayback *playback )
{
  if ( backend.autonomous_audio == FALSE )
  {
    g_mutex_lock( amidiplug_playing_mutex );
    if (( amidiplug_playing_status == AMIDIPLUG_PLAY ) ||
        ( amidiplug_playing_status == AMIDIPLUG_PAUSE ) ||
        ( amidiplug_playing_status == AMIDIPLUG_SEEK ) ||
        (( amidiplug_playing_status == AMIDIPLUG_STOP ) && ( playback->output->buffer_playing() )))
    {
      g_mutex_unlock( amidiplug_playing_mutex );
      return playback->output->output_time();
    }
    else if ( amidiplug_playing_status == AMIDIPLUG_STOP )
    {
      g_mutex_unlock( amidiplug_playing_mutex );
      DEBUGMSG( "GETTIME on stopped song, returning -1\n" , time );
      return -1;
    }
    else /* AMIDIPLUG_ERR */
    {
      g_mutex_unlock( amidiplug_playing_mutex );
      DEBUGMSG( "GETTIME on halted song (an error occurred?), returning -1 and stopping the player\n" );
      audacious_drct_stop();
      return -1;
    }
  }
  else
  {
    gint pt;
    g_mutex_lock( amidiplug_playing_mutex );
    if (( amidiplug_playing_status == AMIDIPLUG_PLAY ) ||
        ( amidiplug_playing_status == AMIDIPLUG_PAUSE ) ||
        ( amidiplug_playing_status == AMIDIPLUG_SEEK ))
    {
      g_mutex_unlock( amidiplug_playing_mutex );
      g_mutex_lock( amidiplug_gettime_mutex );
      pt = midifile.playing_tick;
      g_mutex_unlock( amidiplug_gettime_mutex );
      return (gint)((pt * midifile.avg_microsec_per_tick) / 1000);
    }
    else if ( amidiplug_playing_status == AMIDIPLUG_STOP )
    {
      g_mutex_unlock( amidiplug_playing_mutex );
      DEBUGMSG( "GETTIME on stopped song, returning -1\n" , time );
      return -1;
    }
    else /* AMIDIPLUG_ERR */
    {
      g_mutex_unlock( amidiplug_playing_mutex );
      DEBUGMSG( "GETTIME on halted song (an error occurred?), returning -1 and stopping the player\n" , time );
      audacious_drct_stop();
      return -1;
    }
  }
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


static Tuple * amidiplug_get_song_tuple( const gchar *filename_uri )
{
  /* song title, get it from the filename */
  Tuple *tuple = aud_tuple_new_from_filename(filename_uri);
  gchar *title, *filename = g_filename_from_uri(filename_uri, NULL, NULL);
  
  if (filename != NULL)
      title = g_path_get_basename(filename_uri);
  else
      title = g_strdup(filename_uri);
  
  aud_tuple_associate_string(tuple, FIELD_TITLE, NULL, title);
  g_free(title);
  g_free(filename);

  /* sure, it's possible to calculate the length of a MIDI file anytime,
     but the file must be entirely parsed to calculate it; this could
     lead to a bit of performance loss, so let the user decide here */
  if ( amidiplug_cfg_ap.ap_opts_length_precalc )
  {
    /* let's calculate the midi length, using this nice helper function that
       will return 0 if a problem occurs and the length can't be calculated */
    midifile_t mf;

    if ( i_midi_parse_from_filename( filename_uri , &mf ) )
      aud_tuple_associate_int(tuple, FIELD_LENGTH, NULL, mf.length / 1000);

    i_midi_free( &mf );
  }

  return tuple;
}


static void amidiplug_play( InputPlayback * playback )
{
  gchar * filename_uri = playback->filename;
  gchar * filename = NULL;
  gint port_count = 0;
  gint au_samplerate = -1, au_bitdepth = -1, au_channels = -1;

  if ( backend.gmodule == NULL )
  {
    g_warning( "No sequencer backend selected\n" );
    /* not usable, cause now amidiplug_play is in a different thread
    i_message_gui( _("AMIDI-Plug - warning") ,
                   _("No sequencer backend has been selected!\nPlease configure AMIDI-Plug before playing.") ,
                   AMIDIPLUG_MESSAGE_WARN , NULL , TRUE );
    */
    amidiplug_playing_status = AMIDIPLUG_ERR;
    return;
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
    g_warning( "No ports selected\n" );
    amidiplug_playing_status = AMIDIPLUG_ERR;
    return;
  }

  DEBUGMSG( "PLAY requested, opening file: %s\n" , filename_uri );
  midifile.file_pointer = VFS_FOPEN( filename_uri , "rb" );
  if (!midifile.file_pointer)
  {
    g_warning( "Cannot open %s\n" , filename_uri );
    amidiplug_playing_status = AMIDIPLUG_ERR;
    return;
  }
  midifile.file_name = filename_uri;

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


      /* our length is in microseconds, but the player wants milliseconds */
      filename = g_filename_from_uri( filename_uri , NULL , NULL );
      if ( !filename ) filename = g_strdup( filename_uri );
      playback->set_params( playback , G_PATH_GET_BASENAME(filename) ,
                             (gint)(midifile.length / 1000) ,
                             au_bitdepth * au_samplerate * au_channels / 8 ,
                             au_samplerate , au_channels );
      g_free( filename );

      /* done with file */
      VFS_FCLOSE( midifile.file_pointer );
      midifile.file_pointer = NULL;

      /* play play play! */
      DEBUGMSG( "PLAY requested, starting play thread\n" );
      g_mutex_lock( amidiplug_playing_mutex );
      amidiplug_playing_status = AMIDIPLUG_PLAY;
      g_mutex_unlock( amidiplug_playing_mutex );
      amidiplug_play_thread = g_thread_self();
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

  if ( midifile.file_pointer )
  {
    /* done with file */
    VFS_FCLOSE( midifile.file_pointer );
    midifile.file_pointer = NULL;
  }
}



gpointer amidiplug_play_loop( gpointer arg )
{
  InputPlayback *playback = arg;
  gint j = 0;
  gboolean rewind = TRUE;

  if ( rewind )
  {
    /* initialize current position in each track */
    for (j = 0; j < midifile.num_tracks; ++j)
      midifile.tracks[j].current_event = midifile.tracks[j].first_event;
  }

  if ( backend.autonomous_audio == FALSE )
    amidiplug_audio_thread = g_thread_create(amidiplug_audio_loop, playback, TRUE, NULL);

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
    
    /* check if the song has been paused/seeked/stopped */
    g_mutex_lock( amidiplug_playing_mutex );
    if ( amidiplug_playing_status != AMIDIPLUG_PLAY )
    {
      DEBUGMSG( "PLAY thread, PAUSE/SEEK/STOP requested, handle in play loop\n" );
      if ( amidiplug_playing_status == AMIDIPLUG_PAUSE )
      {
        DEBUGMSG( "PLAY thread, PAUSE requested, shut notes and make the play loop wait\n" );
        backend.seq_event_allnoteoff( midifile.playing_tick );
        backend.seq_queue_stop();
        while (( amidiplug_playing_status != AMIDIPLUG_PLAY ) &&
                ( amidiplug_playing_status != AMIDIPLUG_STOP ))
          g_cond_wait( amidiplug_pause_cond , amidiplug_playing_mutex );
        DEBUGMSG( "PLAY thread, UNPAUSE requested, resume playing\n" );
        g_mutex_lock( amidiplug_gettime_mutex );
        midifile.skip_offset = midifile.playing_tick;
        g_mutex_unlock( amidiplug_gettime_mutex );
        if ( backend.autonomous_audio == FALSE )
          amidiplug_audio_thread = g_thread_create(amidiplug_audio_loop, playback, TRUE, NULL);
        backend.seq_queue_start();
        g_cond_signal( amidiplug_pause_cond );
      }
      if ( amidiplug_playing_status == AMIDIPLUG_SEEK )
      {
        backend.seq_event_allnoteoff( midifile.playing_tick );
        backend.seq_queue_stop();
        amidiplug_skipto( midifile.seeking_tick );
        midifile.seeking_tick = -1;
        amidiplug_playing_status = AMIDIPLUG_PLAY;
        g_cond_signal( amidiplug_seekonpause_cond );
      }
      if ( amidiplug_playing_status == AMIDIPLUG_STOP )
      {
        DEBUGMSG( "PLAY thread, STOP requested, stopping...\n" );
        g_mutex_unlock( amidiplug_playing_mutex );  
        break; /* exit from the for (;;) loop */
      }
    }
    g_mutex_unlock( amidiplug_playing_mutex );    

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
        g_mutex_lock( amidiplug_gettime_mutex );
        midifile.current_tempo = event->data.tempo;
        g_mutex_unlock( amidiplug_gettime_mutex );
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

    g_mutex_lock( amidiplug_gettime_mutex );
    midifile.playing_tick = event->tick;
    g_mutex_unlock( amidiplug_gettime_mutex );

    if ( backend.autonomous_audio == TRUE )
    {
      /* these backends deal with audio production themselves (i.e. ALSA) */
      backend.seq_output( NULL , NULL );
    }
  }

  backend.seq_output_shut( midifile.max_tick , midifile.skip_offset );

  g_mutex_lock( amidiplug_playing_mutex );
  if ( amidiplug_playing_status != AMIDIPLUG_PAUSE )
  {
    amidiplug_playing_status = AMIDIPLUG_STOP;
    DEBUGMSG( "PLAY thread, song stopped/ended\n" );
  }
  g_mutex_unlock( amidiplug_playing_mutex );
  return NULL;
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
        g_mutex_lock( amidiplug_gettime_mutex );
        midifile.current_tempo = event->data.tempo;
        g_mutex_unlock( amidiplug_gettime_mutex );
        break;
    }

    if ( backend.autonomous_audio == TRUE )
    {
      /* these backends deal with audio production themselves (i.e. ALSA) */
      backend.seq_output( NULL , NULL );
    }
  }

  midifile.skip_offset = playing_tick;

  return;
}


gpointer amidiplug_audio_loop( gpointer arg )
{
  InputPlayback *playback = arg;
  gboolean going = 1;
  gpointer buffer = NULL;
  gint buffer_size = 0;
  while ( going )
  {
    if ( backend.seq_output( &buffer , &buffer_size ) )
    {
      playback->pass_audio( playback ,
                     FMT_S16_NE , 2 , buffer_size , buffer , &going );
    }
    g_mutex_lock( amidiplug_playing_mutex );
    if (( amidiplug_playing_status != AMIDIPLUG_PLAY ) &&
        ( amidiplug_playing_status != AMIDIPLUG_SEEK ))
      going = FALSE;
    g_mutex_unlock( amidiplug_playing_mutex );
  }
  if ( buffer != NULL )
    g_free( buffer );
  return NULL;
}
