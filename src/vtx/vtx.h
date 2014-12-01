#ifndef VTX_H
#define VTX_H

#include <libaudcore/index.h>

class VFSFile;

/* info.cc */
void vtx_file_info (const char *filename, VFSFile &file);

/* lh5dec.cc */
bool lh5_decode(const Index<char> &in, Index<unsigned char> &out);

#endif
