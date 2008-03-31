#ifndef FORMATTER_H
#define FORMATTER_H

#include <glib.h>

typedef struct {
    gchar *values[256];
} Formatter;

Formatter *formatter_new(void);
void formatter_destroy(Formatter * formatter);
void formatter_associate(Formatter * formatter, const guchar id, const gchar * value);
void formatter_dissociate(Formatter * formatter, const guchar id);
gchar *formatter_format(Formatter * formatter, const gchar * format);

#endif
