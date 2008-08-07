#include "convert.h"

static SAD_dither_t *sad_state = NULL;
gpointer convert_output = NULL;
static gsize convert_output_length = 0;
static gint nch;
static AFormat in_fmt;
static AFormat out_fmt;

gboolean convert_init(AFormat input_fmt, AFormat output_fmt, gint channels)
{
    gint ret;
    SAD_buffer_format input_sad_fmt;
    SAD_buffer_format output_sad_fmt;


    input_sad_fmt.sample_format = aud_sadfmt_from_afmt(input_fmt);
    if (input_sad_fmt.sample_format < 0) return FALSE;
    input_sad_fmt.fracbits = FMT_FRACBITS(input_fmt);
    input_sad_fmt.channels = channels;
    input_sad_fmt.channels_order = SAD_CHORDER_INTERLEAVED;
    input_sad_fmt.samplerate = 0;

    output_sad_fmt.sample_format = aud_sadfmt_from_afmt(output_fmt);
    if (output_sad_fmt.sample_format < 0) return FALSE;
    output_sad_fmt.fracbits = FMT_FRACBITS(output_fmt);
    output_sad_fmt.channels = channels;
    output_sad_fmt.channels_order = SAD_CHORDER_INTERLEAVED;
    output_sad_fmt.samplerate = 0;

    sad_state = SAD_dither_init(&input_sad_fmt, &output_sad_fmt, &ret);
    if (sad_state == NULL) {
        AUDDBG("ditherer init failed\n");
        return FALSE;
    }

    in_fmt = input_fmt;
    out_fmt = output_fmt;
    nch = channels;

    SAD_dither_set_dither(sad_state, FALSE);
    return TRUE;
}

gint convert_process(gpointer ptr, gint length)
{
    gint frames, len;
    frames = length / nch / FMT_SIZEOF(in_fmt);
    len = frames * nch * FMT_SIZEOF(out_fmt);
    
    if (convert_output == NULL || convert_output_length < len)
    {
        convert_output_length = len;
        convert_output = aud_smart_realloc(convert_output, &convert_output_length);
    }

    SAD_dither_process_buffer(sad_state, ptr, convert_output, frames);
    return len;
}

void convert_free(void)
{
    if (sad_state != NULL) {SAD_dither_free(sad_state); sad_state = NULL;}
}
