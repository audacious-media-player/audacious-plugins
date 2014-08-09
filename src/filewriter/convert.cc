#include "convert.h"

void * convert_output = nullptr;
static int nch;
static int in_fmt;
static int out_fmt;

gboolean convert_init(int input_fmt, int output_fmt, int channels)
{
    in_fmt = input_fmt;
    out_fmt = output_fmt;
    nch = channels;

    return TRUE;
}

int convert_process(void * ptr, int length)
{
    int samples = length / FMT_SIZEOF (in_fmt);
    float * temp;

    convert_output = g_realloc (convert_output, FMT_SIZEOF (out_fmt) * samples);

    if (in_fmt == out_fmt)
        memcpy (convert_output, ptr, FMT_SIZEOF (in_fmt) * samples);
    else if (in_fmt == FMT_FLOAT)
        audio_to_int ((float *) ptr, convert_output, out_fmt, samples);
    else if (out_fmt == FMT_FLOAT)
        audio_from_int (ptr, in_fmt, (float *) convert_output, samples);
    else
    {
        temp = g_new (float, samples);
        audio_from_int (ptr, in_fmt, temp, samples);
        audio_to_int (temp, convert_output, out_fmt, samples);
        g_free (temp);
    }

    return FMT_SIZEOF (out_fmt) * samples;
}

void convert_free(void)
{
    g_free (convert_output);
    convert_output = nullptr;
}
