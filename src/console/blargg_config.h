// Game_Music_Emu 0.5.1 user configuration file. Don't replace when updating library.

#ifndef BLARGG_CONFIG_H
#define BLARGG_CONFIG_H

// Uncomment to use zlib for transparent decompression of gzipped files
#define HAVE_ZLIB_H

// Uncomment to enable platform-specific (and possibly non-portable) optimizations.
//#define BLARGG_NONPORTABLE 1

// Uncomment if automatic byte-order determination doesn't work
//#define BLARGG_BIG_ENDIAN 1

// Uncomment if you get errors in the bool section of blargg_common.h
//#define BLARGG_COMPILER_HAS_BOOL 1

// Uncomment to disable out-of-memory exceptions
#define BLARGG_DISABLE_NOTHROW

// You can have the library use your own custom Data_Reader
//#define GME_FILE_READER Gzip_File_Reader
#include "Vfs_File.h"
#define GME_FILE_READER Vfs_File_Reader

#endif
