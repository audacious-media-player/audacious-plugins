// Sets up common environment for Shay Green's libraries.
// To change configuration options, modify blargg_config.h, not this file.

#ifndef BLARGG_COMMON_H
#define BLARGG_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#include <new>

// Have GME use VFS file access
#define GME_FILE_READER Vfs_File_Reader
#define GME_FILE_READER_INCLUDE "Vfs_File.h"

#define STATIC_CAST(T,expr) static_cast<T>(expr)

#define BLARGG_NEW new(std::nothrow)

// blargg_err_t (0 on success, otherwise error string)
typedef const char* blargg_err_t;

// blargg_long/blargg_ulong = at least 32 bits
typedef int blargg_long;
typedef unsigned blargg_ulong;

// blargg_vector - very lightweight vector of POD types (no constructor/destructor)
template<class T>
class blargg_vector {
	T* begin_;
	size_t size_;
public:
	blargg_vector() : begin_( 0 ), size_( 0 ) { }
	~blargg_vector() { free( begin_ ); }
	size_t size() const { return size_; }
	T* begin() const { return begin_; }
	T* end() const { return begin_ + size_; }
	blargg_err_t resize( size_t n )
	{
		void* p = realloc( begin_, n * sizeof (T) );
		if ( !p && n )
			return "Out of memory";
		begin_ = (T*) p;
		size_ = n;
		return 0;
	}
	void clear() { void* p = begin_; begin_ = 0; size_ = 0; free( p ); }
	T& operator [] ( size_t n ) const
	{
		assert( n <= size_ ); // <= to allow past-the-end value
		return begin_ [n];
	}
};

// BLARGG_4CHAR('a','b','c','d') = 'abcd' (four character integer constant)
#define BLARGG_4CHAR( a, b, c, d ) \
	((a&0xFF)*0x1000000L + (b&0xFF)*0x10000L + (c&0xFF)*0x100L + (d&0xFF))

// BOOST_STATIC_ASSERT( expr ): Generates compile error if expr is 0.
#ifndef BOOST_STATIC_ASSERT
	#ifdef _MSC_VER
		// MSVC6 (_MSC_VER < 1300) fails for use of __LINE__ when /Zl is specified
		#define BOOST_STATIC_ASSERT( expr ) \
			void blargg_failed_( int (*arg) [2 / (int) !!(expr) - 1] )
	#else
		// Some other compilers fail when declaring same function multiple times in class,
		// so differentiate them by line
		#define BOOST_STATIC_ASSERT( expr ) \
			void blargg_failed_( int (*arg) [2 / !!(expr) - 1] [__LINE__] )
	#endif
#endif

#endif
