/*
 * Audacious: Cross platform multimedia player
 * Copyright (c) 2005-2006 Audacious Team
 *
 * Driver for Game_Music_Emu library. See details at:
 * http://www.slack.net/~ant/libs/
 */

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "audacious/util.h"
#include "audacious/titlestring.h"
extern "C" {
#include "audacious/output.h"
#include "audacious/playlist.h"
}
#include <string.h>
#include <stdlib.h>
#include <math.h>

// configdb and prefs ui
#include "Audacious_Config.h"

#include "Music_Emu.h"
#include "Vfs_File.h"

int const fade_threshold = 10 * 1000;
int const fade_length    = 8 * 1000;

AudaciousConsoleConfig audcfg = { 180, FALSE, 32000, TRUE, 0, 0, FALSE, 0 };
static GThread* decode_thread;
static GStaticMutex playback_mutex = G_STATIC_MUTEX_INIT;
static int console_ip_is_going;
static volatile long pending_seek;
extern InputPlugin console_ip;
static Music_Emu* emu = 0;
static int track_ended;

static blargg_err_t log_err( blargg_err_t err )
{
	if ( err )
		printf( "console error: %s\n", err );
	return err;
}

static void log_warning( Music_Emu* emu )
{
	const char* w = emu->warning();
	if ( w )
		printf( "console warning: %s\n", w );
}

static void unload_file()
{
	if ( emu )
		log_warning( emu );
	gme_delete( emu );
	emu = NULL;
}

// Extracts track number from file path, also frees memory at end of block

struct Url_Parser
{
	gchar* path; // path without track number specification
	int track;   // track number (0 = first track)
	bool track_specified; // false if no track number was specified in path
	Url_Parser( gchar* path );
	~Url_Parser() { g_free( path ); }
};

Url_Parser::Url_Parser( gchar* path_in )
{
	track = 0;
	track_specified = false;
	
	path = g_strdup( path_in );
	if ( path )
	{
		gchar* args = strchr( path, '?' );
		if ( args )
		{
			*args = '\0';
			track = atoi( args + 1 );
			if ( track )
				track_specified = true;
		}
	}
}

// Determine file type based on header contents. Returns 0 if unrecognized or path is NULL.
static gme_type_t identify_file( gchar* path )
{
	if ( path )
	{
		char header [4] = { };
		GME_FILE_READER in;
		if ( !log_err( in.open( path ) ) && !log_err( in.read( header, sizeof header ) ) )
			return gme_identify_extension( gme_identify_header( header ), gme_type_list() );
	}
	return 0;
}

// Load file into emulator/info reader and load m3u in same directory, if present.
// If emu is NULL, returns out of memory error.
static blargg_err_t load_in_emu( Music_Emu* emu, const char* path, VFSFile* fd = 0 )
{
	if ( !emu )
		return "Out of memory";
	
	Vfs_File_Reader in;
	blargg_err_t err = 0;
	if ( fd )
		in.reset( fd ); // use fd and let caller close it
	else
		err = in.open( path );
	
	if ( !err )
		emu->load( in );
	in.close();
	
	if ( !err )
	{
		log_warning( emu );
		
		// load .m3u in same directory
		int const path_max = 4096;
		char m3u_path [path_max + 5];
		strncpy( m3u_path, path, path_max );
		m3u_path [path_max] = 0;
		char* p = strrchr( m3u_path, '.' );
		if ( !p )
			p = m3u_path + strlen( m3u_path );
		strcpy( p, ".m3u" );
		
		if ( emu->load_m3u( m3u_path ) ) { } // TODO: log error if m3u file exists
	}
	
	return err;
}

// Get info

static TitleInput* get_track_ti( const char* path, track_info_t const& info, int track )
{
	TitleInput* ti = bmp_title_input_new();
	if ( ti )
	{
		ti->file_name  = g_path_get_basename( path );
		ti->file_path  = g_path_get_dirname ( path );
		ti->performer  = g_strdup( info.author );
		ti->album_name = g_strdup( info.game );
		ti->track_name = g_strdup( info.song ? info.song : ti->file_name );
		if ( info.track_count > 1 )
			ti->track_number = track + 1;
		ti->comment    = g_strdup( info.copyright );
		
		int length = info.length;
		if ( length <= 0 )
			length = info.intro_length + 2 * info.loop_length;
		if ( length <= 0 )
			length = audcfg.loop_length * 1000;
		else if ( length >= fade_threshold )
			length += fade_length;
		ti->length = length;
	}
	return ti;
}

static char* format_and_free_ti( TitleInput* ti, int* length )
{
	char* result = xmms_get_titlestring( xmms_get_gentitle_format(), ti );
	if ( result )
		*length = ti->length;
	bmp_title_input_free( ti );
	
	return result;
}

static TitleInput *get_song_tuple( gchar *path )
{
	TitleInput* result = 0;
	
	Url_Parser url( path );
	Music_Emu* emu = gme_new_info( identify_file( url.path ) );
	track_info_t info;
	if ( !log_err( load_in_emu( emu, url.path ) ) &&
			!log_err( emu->track_info( &info, url.track ) ) )
		result = get_track_ti( url.path, info, url.track );
	delete emu;
	return result;
}

static void get_song_info( char* path, char** title, int* length )
{
	*length = -1;
	*title = NULL;
	
	TitleInput* ti = get_song_tuple( path );
	if ( ti )
		*title = format_and_free_ti( ti, length );
}

// Playback

static void* play_loop_track( gpointer )
{
	g_static_mutex_lock( &playback_mutex );
	
	while ( console_ip_is_going )
	{
		int const buf_size = 1024;
		Music_Emu::sample_t buf [buf_size];
		
		// handle pending seek
		long s = pending_seek;
		pending_seek = -1; // TODO: use atomic swap
		if ( s >= 0 )
		{
			console_ip.output->flush( s * 1000 );
			emu->seek( s * 1000 );
		}

		// fill buffer
		if ( track_ended )
		{
			// TODO: remove delay once host doesn't cut the end of track off
			int const delay = 0; // seconds
			if ( track_ended++ > delay * emu->sample_rate() / (buf_size / 2) )
				console_ip_is_going = false;
			memset( buf, 0, sizeof buf );
		}
		else
		{
			emu->play( buf_size, buf );
			track_ended = emu->track_ended();
		}
		produce_audio( console_ip.output->written_time(), 
			FMT_S16_NE, 1, sizeof buf, buf, 
			&console_ip_is_going );
	}
	
	// stop playing
	unload_file();
	console_ip.output->close_audio();
	console_ip_is_going = 0;
	g_static_mutex_unlock( &playback_mutex );
	// TODO: should decode_thread be cleared here?
	g_thread_exit( NULL );
	return NULL;
}

static void play_file( char* path )
{
	unload_file();
	
	// identify file
	Url_Parser url( path );
	gme_type_t type = identify_file( url.path );
	if ( !type ) return;
	
	// sample rate
	long sample_rate = 0;
	if ( type == gme_spc_type )
		sample_rate = 32000;
	if ( audcfg.resample )
		sample_rate = audcfg.resample_rate;
	if ( !sample_rate )
		sample_rate = 44100;
	
	// create emulator and load
	emu = gme_new_emu( type, sample_rate );
	if ( load_in_emu( emu, url.path ) )
	{
		unload_file();
		return;
	}
	
	// stereo echo depth
	gme_set_stereo_depth( emu, 1.0 / 100 * audcfg.echo );
	
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
		
		emu->set_equalizer(eq);
	}
	
	// get info
	int length = -1;
	track_info_t info;
	if ( !log_err( emu->track_info( &info, url.track ) ) )
	{
		if ( type == gme_spc_type && audcfg.ignore_spc_length )
			info.length = -1;
		TitleInput* ti = get_track_ti( url.path, info, url.track );
		if ( ti )
		{
			char* title = format_and_free_ti( ti, &length );
			if ( title )
			{
				console_ip.set_info( title, length, emu->voice_count() * 1000, sample_rate, 2 );
				g_free( title );
			}
		}
	}
	if ( length <= 0 )
		length = audcfg.loop_length * 1000;
	
	if ( log_err( emu->start_track( url.track ) ) )
	{
		unload_file();
		return;
	}
	log_warning( emu );
	
	// start track
    if ( !console_ip.output->open_audio( FMT_S16_NE, sample_rate, 2 ) )
		return;
	pending_seek = -1;
	track_ended = 0;
	if ( length >= fade_threshold + fade_length )
		length -= fade_length;
	emu->set_fade( length, fade_length );
	console_ip_is_going = 1;
	decode_thread = g_thread_create( play_loop_track, NULL, TRUE, NULL );
}

static void seek( gint time )
{
	// TODO: be sure seek works at all
	// TODO: disallow seek on slow formats (SPC, GYM, VGM using FM)?
	pending_seek = time;
}

static void console_stop(void)
{
	console_ip_is_going = 0;
	if ( decode_thread )
	{
		g_thread_join( decode_thread );
		decode_thread = NULL;
	}
	console_ip.output->close_audio();
	unload_file();
}

static void console_pause(gshort p)
{
	console_ip.output->pause(p);
}

static int get_time(void)
{
	return console_ip_is_going ? console_ip.output->output_time() : -1;
}

static gint is_our_file_from_vfs( gchar* filename, VFSFile* fd )
{
	Url_Parser url( filename );
	if ( !url.path ) return false;
	
	// open file if not already open
	Vfs_File_Reader in;
	if ( !fd )
	{
		if ( log_err( in.open( url.path ) ) ) return false;
		fd = in.file();
		// in will be closed when function ends
	}
	
	// read header and identify type
	gchar header [4] = { };
	vfs_fread( header, sizeof header, 1, fd );
	gme_type_t type = gme_identify_extension( gme_identify_header( header ), gme_type_list() );
	
	gint result = 0;
	if ( type )
	{
		if ( url.track_specified || type->track_count == 1 )
		{
			// don't even need to read file if track is specified or
			// that file format can't have more than one track per file
			result = 1;
		}
		else
		{
			// format requires reading file info to get track count
			Music_Emu* emu = gme_new_info( type );
			vfs_rewind( fd );
			if ( !log_err( load_in_emu( emu, url.path, fd ) ) )
			{
				if ( emu->track_count() == 1 )
				{
					result = 1;
				}
				else
				{
					// for multi-track types, add each track to playlist
					for (int i = 0; i < emu->track_count(); i++)
					{
						gchar _buf[4096];
						g_snprintf(_buf, 4096, "%s?%d", url.path, i);

						playlist_add_url(_buf);
					}
					result = -1;
				}
			}
			delete emu;
		}
	}
	return result;
}

static gint is_our_file( gchar* filename )
{
	return is_our_file_from_vfs( filename, 0 );
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
						_("Console music decoder engine based on Game_Music_Emu 0.5.1.\n"
						"Audacious implementation by: William Pitcock <nenolod@nenolod.net>, \n"
						"        Shay Green <gblargg@gmail.com>"),
						_("Ok"),
						FALSE, NULL, NULL);
		gtk_signal_connect(GTK_OBJECT(aboutbox), "destroy",
					(GCallback)gtk_widget_destroyed, &aboutbox);
	}
}

InputPlugin console_ip =
{
	NULL,
	NULL,
	NULL,
	console_init,
	console_aboutbox,
	console_cfg_ui,
	is_our_file,
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
	NULL,
	get_song_info,
	NULL,
	NULL,
	get_song_tuple,
	NULL,
	NULL,
	is_our_file_from_vfs
};

extern "C" InputPlugin *get_iplugin_info(void)
{
	console_ip.description = g_strdup_printf(_("AY, GBS, GYM, HES, KSS, NSF, NSFE, SAP, SPC, VGM, VGZ module decoder"));
	return &console_ip;
}
