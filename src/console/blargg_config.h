// Custom Audacious configuration

#ifndef BLARGG_CONFIG_H
#define BLARGG_CONFIG_H

// Uncomment to enable platform-specific (and possibly non-portable) optimizations
//#define BLARGG_NONPORTABLE 1

// Uncomment if automatic byte-order determination doesn't work
//#define BLARGG_BIG_ENDIAN 1

// Uncomment if you get errors in the bool section of blargg_common.h
//#define BLARGG_COMPILER_HAS_BOOL 1

// Use non-exception-throwing operator new
#define BLARGG_DISABLE_NOTHROW

// Have GME use VFS file access
#define GME_FILE_READER Vfs_File_Reader
#define GME_FILE_READER_INCLUDE "Vfs_File.h"

#endif
