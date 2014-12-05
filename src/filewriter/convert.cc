#include "convert.h"

#include <string.h>

static int in_fmt;
static int out_fmt;

static Index<char> convert_output;
static Index<float> convert_temp;

void convert_init (int input_fmt, int output_fmt)
{
    in_fmt = input_fmt;
    out_fmt = output_fmt;
}

const Index<char> & convert_process (const void * ptr, int length)
{
    int samples = length / FMT_SIZEOF (in_fmt);

    convert_output.resize (FMT_SIZEOF (out_fmt) * samples);

    if (in_fmt == out_fmt)
        memcpy (convert_output.begin (), ptr, FMT_SIZEOF (in_fmt) * samples);
    else if (in_fmt == FMT_FLOAT)
        audio_to_int ((const float *) ptr, convert_output.begin (), out_fmt, samples);
    else if (out_fmt == FMT_FLOAT)
        audio_from_int (ptr, in_fmt, (float *) convert_output.begin (), samples);
    else
    {
        convert_temp.resize (samples);
        audio_from_int (ptr, in_fmt, convert_temp.begin (), samples);
        audio_to_int (convert_temp.begin (), convert_output.begin (), out_fmt, samples);
    }

    return convert_output;
}

void convert_free ()
{
    convert_output.clear ();
    convert_temp.clear ();
}
