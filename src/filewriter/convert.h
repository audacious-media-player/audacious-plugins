#ifndef CONVERT_H
#define CONVERT_H

#include <libaudcore/index.h>

void convert_init (int input_fmt, int output_fmt);
const Index<char> & convert_process (const void * ptr, int length);
void convert_free ();

#endif
