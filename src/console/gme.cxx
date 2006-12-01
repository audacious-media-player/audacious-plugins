// Game_Music_Emu 0.5.1. http://www.slack.net/~ant/

#include "Music_Emu.h"

#include "Effects_Buffer.h"
#include "blargg_endian.h"
#include <string.h>
#include <ctype.h>

/* Copyright (C) 2003-2006 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

#include "blargg_source.h"

const char* gme_identify_header( void const* header )
{
	switch ( get_be32( header ) )
	{
		case BLARGG_4CHAR('Z','X','A','Y'):  return "AY";
		case BLARGG_4CHAR('G','B','S',0x01): return "GBS";
		case BLARGG_4CHAR('G','Y','M','X'):  return "GYM";
		case BLARGG_4CHAR('H','E','S','M'):  return "HES";
		case BLARGG_4CHAR('K','S','C','C'):
		case BLARGG_4CHAR('K','S','S','X'):  return "KSS";
		case BLARGG_4CHAR('N','E','S','M'):  return "NSF";
		case BLARGG_4CHAR('N','S','F','E'):  return "NSFE";
		case BLARGG_4CHAR('S','A','P',0x0D): return "SAP";
		case BLARGG_4CHAR('S','N','E','S'):  return "SPC";
		case BLARGG_4CHAR('V','g','m',' '):  return "VGM";
	}
	return "";
}

static void to_uppercase( const char* in, int len, char* out )
{
	for ( int i = 0; i < len; i++ )
	{
		if ( !(out [i] = toupper( in [i] )) )
			return;
	}
	*out = 0; // extension too long
}

gme_type_t gme_identify_extension( const char* extension_, gme_type_t const* types )
{
	char const* end = strrchr( extension_, '.' );
	if ( end )
		extension_ = end + 1;
	
	char extension [6];
	to_uppercase( extension_, sizeof extension, extension );
	
	for ( ; *types; types++ )
		if ( !strcmp( extension, (*types)->extension_ ) )
			return *types;
	return 0;
}

gme_err_t gme_identify_file( const char* path, gme_type_t const* types, gme_type_t* type_out )
{
	*type_out = gme_identify_extension( path, types );
	if ( !*type_out )
	{
		char header [4] = { };
		GME_FILE_READER in;
		RETURN_ERR( in.open( path ) );
		RETURN_ERR( in.read( header, sizeof header ) );
		*type_out = gme_identify_extension( gme_identify_header( header ), types );
	}
	return 0;   
}

Music_Emu* gme_new_emu( gme_type_t type, long rate )
{
	if ( type )
	{
		Music_Emu* me = type->new_emu();
		if ( me )
		{
			if ( type->flags_ & 1 )
			{
				me->effects_buffer = BLARGG_NEW Effects_Buffer;
				if ( me->effects_buffer )
					me->set_buffer( me->effects_buffer );
			}
			
			if ( !(type->flags_ & 1) || me->effects_buffer )
			{
				if ( !me->set_sample_rate( rate ) )
				{
					check( me->type() == type );
					return me;
				}
			}
			delete me;
		}
	}
	return 0;
}

Music_Emu* gme_new_info( gme_type_t type )
{
	if ( !type )
		return 0;
	
	return type->new_info();
}

const char* gme_load_file( Music_Emu* me, const char* path ) { return me->load_file( path ); }

const char* gme_load_data( Music_Emu* me, const char* data, long size )
{
	Mem_File_Reader in( data, size );
	return me->load( in );
}

gme_err_t gme_load_custom( Music_Emu* me, gme_reader_t func, long size, void* data )
{
	Callback_Reader in( func, size, data );
	return me->load( in );
}

void gme_delete( Music_Emu* me ) { delete me; }

gme_type_t gme_type( Music_Emu const* me ) { return me->type(); }

const char* gme_warning( Music_Emu* me ) { return me->warning(); }

int gme_track_count( Music_Emu const* me ) { return me->track_count(); }

const char* gme_track_info( Music_Emu const* me, track_info_t* out, int track )
{
	return me->track_info( out, track );
}

void gme_set_stereo_depth( Music_Emu* me, double depth )
{
	if ( me->effects_buffer )
		STATIC_CAST(Effects_Buffer*,me->effects_buffer)->set_depth( depth );
}

gme_err_t gme_start_track( Music_Emu* me, int index ) { return me->start_track( index ); }

gme_err_t gme_play( Music_Emu* me, long n, short* p ) { return me->play( n, p ); }

void gme_set_fade( Music_Emu* me, long start_msec ) { me->set_fade( start_msec ); }

int gme_track_ended( Music_Emu const* me ) { return me->track_ended(); }

long gme_tell( Music_Emu const* me ) { return me->tell(); }

gme_err_t gme_seek( Music_Emu* me, long msec ) { return me->seek( msec ); }

int gme_voice_count( Music_Emu const* me ) { return me->voice_count(); }

const char** gme_voice_names( Music_Emu const* me ) { return me->voice_names(); }

void gme_ignore_silence( Music_Emu* me, int disable ) { me->ignore_silence( disable != 0 ); }

void gme_set_tempo( Music_Emu* me, double t ) { me->set_tempo( t ); }

void gme_mute_voice( Music_Emu* me, int index, int mute ) { me->mute_voice( index, mute != 0 ); }

void gme_mute_voices( Music_Emu* me, int mask ) { me->mute_voices( mask ); }

gme_equalizer_t gme_equalizer( Music_Emu const* me ) { return me->equalizer(); }

void gme_set_equalizer( Music_Emu* me, gme_equalizer_t const* eq ) { me->set_equalizer( *eq ); }
