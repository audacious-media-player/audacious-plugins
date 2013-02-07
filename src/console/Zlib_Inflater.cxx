// File_Extractor 0.4.0. http://www.slack.net/~ant/

#include "Zlib_Inflater.h"

#include <assert.h>
#include <string.h>

/* Copyright (C) 2006 Shay Green. This module is free software; you
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

static const char* get_zlib_err( int code )
{
	assert( code != Z_OK );
	if ( code == Z_MEM_ERROR )
		return "Out of memory";

	const char* str = zError( code );
	if ( code == Z_DATA_ERROR )
		str = "Zip data is corrupt";
	if ( !str )
		str = "Zip error";
	return str;
}

void Zlib_Inflater::end()
{
	if ( deflated_ )
	{
		deflated_ = false;
		if ( inflateEnd( &zbuf ) )
			check( false );
	}
	buf.clear();

	static z_stream const empty = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	memcpy( &zbuf, &empty, sizeof zbuf );
}

Zlib_Inflater::Zlib_Inflater()
{
	deflated_ = false;
	end(); // initialize things
}

Zlib_Inflater::~Zlib_Inflater()
{
	end();
}

blargg_err_t Zlib_Inflater::begin( mode_t mode, callback_t callback, void* user_data,
		long buf_size )
{
	end();

	if ( buf_size && buf.resize( buf_size ) )
		buf_size = 0; // not enough memory for requested size, so use default

	if ( !buf_size )
		RETURN_ERR( buf.resize( 16 * 1024L ) );

	// Fill buffer with some data, less than normal buffer size since caller might
	// just be examining beginning of file. 4K is a common disk block size.
	long count = (buf_size ? buf_size : 4096);
	RETURN_ERR( callback( user_data, buf.begin(), &count ) );
	zbuf.avail_in = count;
	zbuf.next_in  = buf.begin();

	if ( mode == mode_auto )
	{
		// examine buffer for gzip header
		mode = mode_copy;
		int const min_gzip_size = 2 + 8 + 8;
		if ( count >= min_gzip_size && buf [0] == 0x1F && buf [1] == 0x8B )
			mode = mode_ungz;
	}

	if ( mode != mode_copy )
	{
		int wb = MAX_WBITS + 16; // have zlib handle gzip header
		if ( mode == mode_raw_deflate )
			wb = -MAX_WBITS;

		int zerr = inflateInit2( &zbuf, wb );
		if ( zerr )
			return get_zlib_err( zerr );

		deflated_ = true;
	}
	return 0;
}


blargg_err_t Zlib_Inflater::read( void* out, long* count_io,
		callback_t callback, void* user_data )
{
	if ( !*count_io )
		return 0;

	if ( !deflated_ )
	{
		// copy buffered data
		long first = zbuf.avail_in;
		if ( first )
		{
			if ( first > *count_io )
				first = *count_io;
			memcpy( out, zbuf.next_in, first );
			zbuf.next_in  += first;
			zbuf.avail_in -= first;
			if ( !zbuf.avail_in )
				buf.clear(); // done with buffer
		}

		// read remaining directly
		long second = *count_io - first;
		if ( second )
		{
			long actual = second;
			RETURN_ERR( callback( user_data, (char*) out + first, &actual ) );
			*count_io -= second - actual;
		}
	}
	else
	{
		zbuf.next_out  = (Bytef*) out;
		zbuf.avail_out = *count_io;

		while ( 1 )
		{
			uInt old_avail_in = zbuf.avail_in;
			int err = inflate( &zbuf, Z_NO_FLUSH );
			if ( err == Z_STREAM_END )
			{
				*count_io -= zbuf.avail_out;
				end();
				break; // all data deflated
			}

			if ( err == Z_BUF_ERROR && !old_avail_in )
				err = 0; // we just need to provide more input

			if ( err )
				return get_zlib_err( err );

			if ( !zbuf.avail_out )
				break; // requested number of bytes deflated

			if ( zbuf.avail_in )
			{
				// inflate() should never leave input if there's still space for output
				assert( false );
				return "Corrupt zip data";
			}

			// refill buffer
			long count = buf.size();
			RETURN_ERR( callback( user_data, buf.begin(), &count ) );
			zbuf.next_in  = buf.begin();
			zbuf.avail_in = count;
			if ( !zbuf.avail_in )
				return "Corrupt zip data"; // stream didn't end but there's no more data
		}
	}
	return 0;
}
