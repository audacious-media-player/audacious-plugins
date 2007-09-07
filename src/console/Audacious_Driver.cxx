/*
 * Audacious: Cross platform multimedia player
 * Copyright (c) 2005-2006 Audacious Team
 *
 * Driver for Game_Music_Emu library. See details at:
 * http://www.slack.net/~ant/libs/
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib.h>
#include <audacious/i18n.h>
#include <gtk/gtk.h>
extern "C" {
#include "audacious/main.h"
#include "audacious/tuple.h"
#include "audacious/tuple_formatter.h"
#include "audacious/util.h"
#include "audacious/output.h"
#include "audacious/playlist.h"
}
#include <string.h>
#include <stdlib.h>
#include <math.h>

// configdb and prefs ui
#include "Audacious_Config.h"

#include "Music_Emu.h"
#include "Gzip_Reader.h"

int const fade_threshold = 10 * 1000;
int const fade_length    = 8 * 1000;
int const path_max = 4096;

AudaciousConsoleConfig audcfg =
{ 180, FALSE, 32000, 0, 0, FALSE, 0, FALSE };
static GThread* decode_thread;
static GStaticMutex playback_mutex = G_STATIC_MUTEX_INIT;
static int console_ip_is_going;
static volatile long pending_seek;
extern InputPlugin console_ip;
static Music_Emu* emu = 0;

static blargg_err_t log_err( blargg_err_t err )
{
	if ( err )
		g_critical( "console error: %s\n", err );
	return err;
}

static void log_warning( Music_Emu* emu )
{
	const char* w = emu->warning();
	if ( w )
		g_warning( "console warning: %s\n", w );
}

static void unload_file()
{
	if ( emu )
		log_warning( emu );
	gme_delete( emu );
	emu = NULL;
}

// Handles URL parsing, file opening and identification, and file loading.
// Keeps file header around when loading rest of file to avoid seeking
// and re-reading.
class File_Handler {
public:
	gchar* path;            // path without track number specification
	int track;              // track number (0 = first track)
	bool track_specified;   // false if no track number was specified in path
	Music_Emu* emu;         // set to 0 to take ownership
	gme_type_t type;
	
	// Parses path and identifies file type
	File_Handler( const char* path, VFSFile* fd = 0, gboolean is_our_file_hack = FALSE );
	
	// Creates emulator and returns 0. If this wasn't a music file or
	// emulator couldn't be created, returns 1.
	int load( long sample_rate );
	
	// Deletes owned emu and closes file
	~File_Handler();
private:
	char header [4];
	Vfs_File_Reader vfs_in;
	Gzip_Reader in;
};

File_Handler::File_Handler( const char* path_in, VFSFile* fd, gboolean is_our_file_hack )
{
	emu   = 0;
	type  = 0;
	track = 0;
	track_specified = false;
	
	path = g_strdup( path_in );
	if ( !path )
		return; // out of memory
	
	// extract track number
	gchar* args = strchr( path, '?' ); // TODO: use strrchr()?
	if ( args && g_ascii_isdigit( (guchar) *(args + 1) ) )
	{
		*args = '\0';
		// TODO: use func with better error reporting, and perhaps don't
		// truncate path if there is no number after ?
		track = atoi( args + 1 );
		track_specified = true;
	}

	// if the track is specified, then we have a match. don't worry
	// about it right now -nenolod
	if (track_specified && is_our_file_hack)
		return;
	
	// open vfs
	if ( fd )
		vfs_in.reset( fd );
	else if ( log_err( vfs_in.open( path ) ) )
		return;
	
	// now open gzip_reader on top of vfs
	if ( log_err( in.open( &vfs_in ) ) )
		return;
	
	// read and identify header
	if ( !log_err( in.read( header, sizeof header ) ) )
	{
		type = gme_identify_extension( gme_identify_header( header ) );
		if ( !type )
		{
			type = gme_identify_extension( path );
			if ( type != gme_gym_type ) // only trust file extension for headerless .gym files
				type = 0;
		}
	}
}

File_Handler::~File_Handler()
{
	gme_delete( emu );
	g_free( path );
}

int File_Handler::load( long sample_rate )
{
	if ( !type )
		return 1;
	
	emu = gme_new_emu( type, sample_rate );
	if ( !emu )
	{
		log_err( "Out of memory" );
		return 1;
	}
	
	{
		// combine header with remaining file data
		Remaining_Reader reader( header, sizeof header, &in );
		if ( log_err( emu->load( reader ) ) )
			return 1;
	}
	
	// files can be closed now
	in.close();
	vfs_in.close();
	
	log_warning( emu );
	
	// load .m3u from same directory( replace/add extension with ".m3u")
	char m3u_path [path_max + 5];
	strncpy( m3u_path, path, path_max );
	m3u_path [path_max] = 0;
	// TODO: use better path-building functions
	char* p = strrchr( m3u_path, '.' );
	if ( !p )
		p = m3u_path + strlen( m3u_path );
	strcpy( p, ".m3u" );
	
	Vfs_File_Reader m3u;
	if ( !m3u.open( m3u_path ) )
	{
		if ( log_err( emu->load_m3u( m3u ) ) ) // TODO: fail if m3u can't be loaded?
			log_warning( emu ); // this will log line number of first problem in m3u
	}
	
	return 0;
}

// Get info

static inline gchar *selective_strdup(const gchar *in)
{
	if (in == NULL || *in == '\0')
		return NULL;

	return g_strdup(in);
}

static Tuple* get_track_ti( const char* path, track_info_t const& info, int track )
{
	Tuple* ti = tuple_new();
	if ( ti )
	{
		tuple_associate_string(ti, "file-name", g_path_get_basename(path));
		tuple_associate_string(ti, "file-path", g_path_get_dirname(path));
		tuple_associate_string(ti, "artist", info.author);
		tuple_associate_string(ti, "album", info.game);
		tuple_associate_string(ti, "game", info.game);
		tuple_associate_string(ti, "title", info.song ? info.song : g_path_get_basename(path));
		if ( info.track_count > 1 )
		{
			tuple_associate_int(ti, "track-number", track + 1);
			tuple_associate_int(ti, "subsong", track);
		}
		tuple_associate_string(ti, "copyright", info.copyright);
		tuple_associate_string(ti, "console", info.system);
		tuple_associate_string(ti, "codec", info.system);
		tuple_associate_string(ti, "quality", "sequenced");
		tuple_associate_string(ti, "dumper", info.dumper);
		tuple_associate_string(ti, "comment", info.comment);

		int length = info.length;
		if ( length <= 0 )
			length = info.intro_length + 2 * info.loop_length;
		if ( length <= 0 )
			length = audcfg.loop_length * 1000;
		else if ( length >= fade_threshold )
			length += fade_length;
		tuple_associate_int(ti, "length", length);
	}
	return ti;
}

static char* format_and_free_ti( Tuple* ti, int* length )
{
	char* result = tuple_formatter_make_title_string(ti, get_gentitle_format());
	if ( result )
		*length = tuple_get_int(ti, "length");
	tuple_free((void *) ti);

	return result;
}

static Tuple *get_song_tuple( gchar *path )
{
	Tuple* result = 0;
	File_Handler fh( path );
	if ( !fh.load( gme_info_only ) )
	{
		track_info_t info;
		if ( !log_err( fh.emu->track_info( &info, fh.track ) ) )
			result = get_track_ti( fh.path, info, fh.track );
	}
	return result;
}

static void get_song_info( char* path, char** title, int* length )
{
	*length = -1;
	*title = NULL;
	
	Tuple* ti = get_song_tuple( path );
	if ( ti )
		*title = format_and_free_ti( ti, length );
}

// Playback

static void* play_loop_track( gpointer arg )
{
        InputPlayback *playback = (InputPlayback *) arg;
	g_static_mutex_lock( &playback_mutex );

	int end_delay = 0;	
	while ( console_ip_is_going )
	{
		// handle pending seek
		long s = pending_seek;
		pending_seek = -1; // TODO: use atomic swap
		if ( s >= 0 )
		{
			playback->output->flush( s * 1000 );
			emu->seek( s * 1000 );
		}
		
		// fill and play buffer of audio
		// TODO: see if larger buffer helps efficiency
		int const buf_size = 1024;
		Music_Emu::sample_t buf [buf_size];
		if ( end_delay )
		{
			// TODO: remove delay once host doesn't cut the end of track off
			if ( !--end_delay )
				console_ip_is_going = false;
			memset( buf, 0, sizeof buf );

		}
		else
		{
			emu->play( buf_size, buf );
			if ( emu->track_ended() )
			{
				double const seconds = 3;
				end_delay = emu->sample_rate() * (int) (seconds * 2) / buf_size;
			}
		}
		produce_audio( playback->output->written_time(), 
			FMT_S16_NE, 1, sizeof buf, buf, 
			&console_ip_is_going );
	}
	
	// stop playing
	unload_file();
	playback->output->close_audio();
	console_ip_is_going = 0;
	g_static_mutex_unlock( &playback_mutex );
	return NULL;
}

static void play_file( InputPlayback *playback )
{
        char* path = playback->filename;
	unload_file();
	
	// identify file
	File_Handler fh( path );
	if ( !fh.type )
		return;
	
	// select sample rate
	long sample_rate = 0;
	if ( fh.type == gme_spc_type )
		sample_rate = 32000;
	if ( audcfg.resample )
		sample_rate = audcfg.resample_rate;
	if ( !sample_rate )
		sample_rate = 44100;
	
	// create emulator and load file
	if ( fh.load( sample_rate ) )
		return;
	
	// stereo echo depth
	gme_set_stereo_depth( fh.emu, 1.0 / 100 * audcfg.echo );
	
	// set equalizer
	if ( audcfg.treble || audcfg.bass )
	{
		Music_Emu::equalizer_t eq;
		
		// bass - logarithmic, 2 to 8194 Hz
		double bass = 1.0 - (audcfg.bass / 200.0 + 0.5);
		eq.bass = (long) (2.0 + pow( 2.0, bass * 13 ));
		
		// treble - -50 to 0 to +5 dB
		double treble = audcfg.treble / 100.0;
		eq.treble = treble * (treble < 0 ? 50.0 : 5.0);
		
		fh.emu->set_equalizer(eq);
	}
	
	// get info
	int length = -1;
	track_info_t info;
	if ( !log_err( fh.emu->track_info( &info, fh.track ) ) )
	{
		if ( fh.type == gme_spc_type && audcfg.ignore_spc_length )
			info.length = -1;
		Tuple* ti = get_track_ti( fh.path, info, fh.track );
		if ( ti )
		{
			char* title = format_and_free_ti( ti, &length );
			if ( title )
			{
				console_ip.set_info( title, length, fh.emu->voice_count() * 1000, sample_rate, 2 );
				g_free( title );
			}
		}
	}
	
	// start track
	if ( log_err( fh.emu->start_track( fh.track ) ) )
		return;
	log_warning( fh.emu );
	if ( !playback->output->open_audio( FMT_S16_NE, sample_rate, 2 ) )
		return;
	
	// set fade time
	if ( length <= 0 )
		length = audcfg.loop_length * 1000;
	if ( length >= fade_threshold + fade_length )
		length -= fade_length / 2;
	fh.emu->set_fade( length, fade_length );
	
	// take ownership of emu
	emu = fh.emu;
	fh.emu = 0;
	
	pending_seek = -1;
	console_ip_is_going = 1;
	decode_thread = g_thread_self();
	playback->set_pb_ready(playback);
        play_loop_track( playback );
}

static void seek( InputPlayback * data, gint time )
{
	// TODO: use thread-safe atomic set
	pending_seek = time;
}

static void console_stop(InputPlayback *playback)
{
	console_ip_is_going = 0;
	if ( decode_thread )
	{
		g_thread_join( decode_thread );
		decode_thread = NULL;
	}
	playback->output->close_audio();
	unload_file();
}

static void console_pause(InputPlayback * playback, gshort p)
{
	playback->output->pause(p);
}

static int get_time(InputPlayback *playback)
{
	return console_ip_is_going ? playback->output->output_time() : -1;
}

static gint is_our_file_from_vfs( gchar* path, VFSFile* fd )
{
	gint result = 0;
	File_Handler fh( path, fd, TRUE );
	if ( fh.type )
	{
		if ( fh.track_specified || fh.type->track_count == 1 )
		{
			// don't even need to read file if track is specified or
			// that file format can't have more than one track per file
			result = 1;
		}
		else if ( !fh.load( gme_info_only ) )
		{
			// format requires reading file info to get track count
			if ( fh.emu->track_count() == 1 )
			{
				result = 1;
			}
			else
			{
				// for multi-track types, add each track to playlist
				for (int i = 0; i < fh.emu->track_count(); i++)
				{
					gchar _buf[path_max];
					g_snprintf(_buf, path_max, "%s?%d", fh.path, i);

					playlist_add_url(playlist_get_active(), _buf);
				}
				result = -1;
			}
		}
	}
	else if (fh.track_specified)
	{
		result = 1;
	}

	return result;
}

// Setup

static void console_init(void)
{
	console_cfg_load();
}

extern "C" void console_aboutbox(void)
{
	static GtkWidget * aboutbox = NULL;

	if (!aboutbox)
	{
		aboutbox = xmms_show_message(_("About the Console Music Decoder"),
						_("Console music decoder engine based on Game_Music_Emu 0.5.2.\n"
						"Supported formats: AY, GBS, GYM, HES, KSS, NSF, NSFE, SAP, SPC, VGM, VGZ\n"
						"Audacious implementation by: William Pitcock <nenolod@nenolod.net>, \n"
						"        Shay Green <gblargg@gmail.com>"),
						_("Ok"),
						FALSE, NULL, NULL);
		gtk_signal_connect(GTK_OBJECT(aboutbox), "destroy",
					(GCallback)gtk_widget_destroyed, &aboutbox);
	}
}

const gchar *gme_fmts[] = { "ay", "gbs", "gym", "hes", "kss", "nsf", "nsfe", 
		      "sap", "spc", "vgm", "vgz", NULL };

InputPlugin console_ip =
{
	NULL,
	NULL,
	(gchar *)"Game console audio module decoder",
	console_init,
	console_aboutbox,
	console_cfg_ui,
	NULL,
	FALSE,
	NULL,
	NULL,
	play_file,
	console_stop,
	console_pause,
	seek,
	NULL,
	get_time,
	NULL,
	NULL,   
	NULL,
	NULL,
	NULL,
	NULL,
	get_song_info,
	NULL,
	NULL,
	get_song_tuple,
	NULL,
	NULL,
	is_our_file_from_vfs,
	(gchar **)gme_fmts
};

InputPlugin *console_iplist[] = { &console_ip, NULL };

extern "C" {

DECLARE_PLUGIN(console, NULL, NULL, console_iplist, NULL, NULL, NULL, NULL,NULL);

};
