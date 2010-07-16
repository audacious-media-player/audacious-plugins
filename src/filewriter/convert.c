#include "convert.h"

gpointer convert_output = NULL;
static gint nch;
static gint in_fmt;
static gint out_fmt;

gboolean convert_init(gint input_fmt, gint output_fmt, gint channels)
{
    in_fmt = input_fmt;
    out_fmt = output_fmt;
    nch = channels;

    return TRUE;
}

gint convert_process(gpointer ptr, gint length)
{
    gint samples = length / FMT_SIZEOF (in_fmt);
    gfloat * temp;

    convert_output = g_realloc (convert_output, FMT_SIZEOF (out_fmt) * samples);

    if (in_fmt == out_fmt)
        memcpy (convert_output, ptr, FMT_SIZEOF (in_fmt) * samples);
    else if (in_fmt == FMT_FLOAT)
        audio_to_int (ptr, convert_output, out_fmt, samples);
    else if (out_fmt == FMT_FLOAT)
        audio_from_int (ptr, in_fmt, convert_output, samples);
    else
    {
        temp = g_malloc (sizeof (gfloat) * samples);
        audio_from_int (ptr, in_fmt, temp, samples);
        audio_to_int (temp, convert_output, out_fmt, samples);
        g_free (temp);
    }

    return FMT_SIZEOF (out_fmt) * samples;
}

void convert_free(void)
{
    g_free (convert_output);
    convert_output = NULL;
}
