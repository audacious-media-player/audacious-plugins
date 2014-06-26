#ifndef FORMATTER_H
#define FORMATTER_H

#include <glib.h>

typedef struct {
    char *values[256];
} Formatter;

Formatter *formatter_new(void);
void formatter_destroy(Formatter * formatter);
void formatter_associate(Formatter * formatter, const unsigned char id, const char * value);
void formatter_dissociate(Formatter * formatter, const unsigned char id);
char *formatter_format(Formatter * formatter, const char * format);

#endif
