#ifndef CONVERT_H
#define CONVERT_H

#include "filewriter.h"

extern gpointer convert_output;

gboolean convert_init(gint input_fmt, gint output_fmt, gint channels);

gint convert_process(gpointer ptr, gint length);

void convert_free(void);

#endif
