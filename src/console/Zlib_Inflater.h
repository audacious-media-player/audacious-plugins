// Simplifies use of zlib for deflating data

// File_Extractor 0.4.0
#ifndef ZLIB_INFLATER_H
#define ZLIB_INFLATER_H

#include "blargg_common.h"
#include "zlib.h"

class Zlib_Inflater {
public:

	// Data reader callback
	typedef blargg_err_t (*callback_t)( void* user_data, void* out, long* count );

	// Begin inflation using specified mode and fill buffer using read callback.
	// Default buffer is 16K and filled to 4K, or specify buf_size for custom
	// buffer size and to read that much file data. Using mode_auto selects
	// between mode_copy and mode_ungz by examining first two bytes of buffer.
	enum mode_t { mode_copy, mode_ungz, mode_raw_deflate, mode_auto };
	blargg_err_t begin( mode_t, callback_t, void* user_data, long buf_size = 0 );

	// True if begin() has been called with mode_ungz or mode_raw_deflate
	bool deflated() const { return deflated_; }

	// Read/inflate at most *count_io bytes into out and set *count_io to actual
	// number of bytes read (less than requested if end of deflated data is reached).
	// Keeps buffer full with user-provided callback.
	blargg_err_t read( void* out, long* count_io, callback_t, void* user_data );

	// End inflation and free memory
	void end();

private:
	// noncopyable
	Zlib_Inflater( const Zlib_Inflater& );
	Zlib_Inflater& operator = ( const Zlib_Inflater& );

public:
	Zlib_Inflater();
	~Zlib_Inflater();
private:
	z_stream_s zbuf;
	blargg_vector<unsigned char> buf;
	bool deflated_;
};

#endif
