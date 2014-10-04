#ifndef CONVERT_H
#define CONVERT_H

#include "filewriter.h"

extern void * convert_output;

gboolean convert_init(int input_fmt, int output_fmt, int channels);

int convert_process (const void * ptr, int length);

void convert_free(void);

#endif
