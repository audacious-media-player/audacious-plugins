//#define AUD_DEBUG

#include <string>

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
extern "C" {
#include <wavpack/wavpack.h>
#include <audacious/plugin.h>
#include <audacious/output.h>
}
#include <glib.h>
#include <gtk/gtk.h>
#include <math.h>
#include "tags.h"
#include "../../config.h"
#ifndef M_LN10
#define M_LN10   2.3025850929940456840179914546843642
#endif

#define BUFFER_SIZE 256 // read buffer size, in samples
#define SAMPLE_SIZE(a) (a == 8 ? sizeof(guint8) : (a == 16 ? sizeof(guint16) : sizeof(guint32)))
#define SAMPLE_FMT(a) (a == 8 ? FMT_S8 : (a == 16 ? FMT_S16_NE : (a == 24 ? FMT_S24_NE : FMT_S32_NE)))

static void wv_load_config();
static gint wv_is_our_fd(gchar *filename, VFSFile *file);
static Tuple *wv_probe_for_tuple(gchar *filename, VFSFile *file);
static void wv_play(InputPlayback *);
static void wv_stop(InputPlayback *);
static void wv_pause(InputPlayback *, gshort);
static void wv_seek(InputPlayback *, gint);
static gint wv_get_time(InputPlayback *);
static void wv_get_song_info(gchar *, gchar **, gint *);
static gchar *generate_title(const gchar *, WavpackContext *ctx);
static gint isSeek;
static gshort paused;
static gboolean killDecodeThread;
static gboolean AudioError;
static GThread *thread_handle;
static Tuple *wv_get_song_tuple(gchar *);

// in ui.cxx
void wv_configure();
void wv_about_box(void);
void wv_file_info_box(gchar *);
extern gboolean clipPreventionEnabled;
extern gboolean dynBitrateEnabled;
extern gboolean replaygainEnabled;
extern gboolean albumReplaygainEnabled;
extern gboolean openedAudio;

const gchar *wv_fmts[] = { "wv", NULL };

InputPlugin wvpack = {
    NULL,                       //handle
    NULL,                       //filename
    (gchar *)"WavPack Audio Plugin",
    wv_load_config,
    NULL,
    wv_about_box,
    wv_configure,
    FALSE,
    NULL,
    NULL,                       //no use
    wv_play,
    wv_stop,
    wv_pause,
    wv_seek,
    wv_get_time,
    NULL,                       //get volume
    NULL,                       //set volume
    NULL,                       //cleanup
    NULL,                       //obsolete
    NULL,                       //add_vis
    NULL,
    wv_get_song_info,
    wv_file_info_box,           //info box
    wv_get_song_tuple,
    wv_is_our_fd,
    (gchar **)wv_fmts,
    NULL,			// high precision seeking
    wv_probe_for_tuple		// probe for a tuple
};

gint32 read_bytes (void *id, void *data, gint32 bcount)
{
    return aud_vfs_fread (data, 1, bcount, (VFSFile *) id);
}

guint32 get_pos (void *id)
{
    return aud_vfs_ftell ((VFSFile *) id);
}

gint set_pos_abs (void *id, guint32 pos)
{
    return aud_vfs_fseek ((VFSFile *) id, pos, SEEK_SET);
}

gint set_pos_rel (void *id, gint32 delta, gint mode)
{
    return aud_vfs_fseek ((VFSFile *) id, delta, mode);
}

gint push_back_byte (void *id, gint c)
{
    return aud_vfs_ungetc (c, (VFSFile *) id);
}

guint32 get_length (void *id)
{
    VFSFile *file = (VFSFile *) id;

    if (file == NULL)
        return 0;

    return aud_vfs_fsize (file);
}

gint can_seek (void *id)
{
    return (aud_vfs_is_streaming ((VFSFile *) id) == FALSE);
}

gint32 write_bytes (void *id, void *data, gint32 bcount)
{
    return aud_vfs_fwrite (data, 1, bcount, (VFSFile *) id);
}

WavpackStreamReader reader = {
    read_bytes, get_pos, set_pos_abs, set_pos_rel, push_back_byte, get_length, can_seek,
    write_bytes
};

class WavpackDecoder
{
public:
    InputPlugin *wvpack;
    gint32 *input;
    void *output;
    gint sample_rate;
    gint num_channels;
    guint num_samples;
    guint length;
    gint bits_per_sample;
    WavpackContext *ctx;
    gchar error_buff[80]; //string space is allocated by the caller and must be at least 80 chars
    VFSFile *wv_Input, *wvc_Input;

    WavpackDecoder(InputPlugin *wvpack) : wvpack(wvpack)
    {
        ctx = NULL;
        input = NULL;
        output = NULL;
        wv_Input = NULL;
        wvc_Input = NULL;
    }

    ~WavpackDecoder()
    {
        if (input != NULL) {
            free(input);
            input = NULL;
        }
        if (output != NULL) {
            free(output);
            output = NULL;
        }
        if (ctx != NULL) {
            if (wv_Input)
                aud_vfs_fclose(wv_Input);

            if (wvc_Input)
                aud_vfs_fclose(wvc_Input);
            g_free(ctx);
            ctx = NULL;
        }
    }

    gboolean attach(const gchar *filename)
    {
        wv_Input = aud_vfs_fopen(filename, "rb");

        gchar *corrFilename = g_strconcat(filename, "c", NULL);

        wvc_Input = aud_vfs_fopen(corrFilename, "rb");

        g_free(corrFilename);

        ctx = WavpackOpenFileInputEx(&reader, wv_Input, wvc_Input, error_buff, OPEN_TAGS | OPEN_WVC, 0);

        if (ctx == NULL) {
            return false;
        }

        return true;
    }

    gboolean attach(gchar *filename, VFSFile *wvi)
    {
        ctx = WavpackOpenFileInputEx(&reader, wvi, NULL, error_buff, OPEN_TAGS, 0);

        if (ctx == NULL)
            return false;

        return true;
    }

    gboolean attach_to_play(InputPlayback *playback)
    {
        wv_Input = aud_vfs_fopen(playback->filename, "rb");

        gchar *corrFilename = g_strconcat(playback->filename, "c", NULL);

        wvc_Input = aud_vfs_fopen(corrFilename, "rb");

        g_free(corrFilename);

        ctx = WavpackOpenFileInputEx(&reader, wv_Input, wvc_Input, error_buff, OPEN_TAGS | OPEN_WVC, 0);

        if (ctx == NULL)
            return false;

        sample_rate = WavpackGetSampleRate(ctx);
        num_channels = WavpackGetNumChannels(ctx);
        bits_per_sample = WavpackGetBitsPerSample(ctx);
        num_samples = WavpackGetNumSamples(ctx);
        length = num_samples / sample_rate;
        input = (gint32 *) malloc(BUFFER_SIZE * num_channels * sizeof(guint32));
        output = malloc(BUFFER_SIZE * num_channels * SAMPLE_SIZE(bits_per_sample));        
        playback->set_params(playback, generate_title(playback->filename, ctx),
                      length * 1000,
                      (gint) WavpackGetAverageBitrate(ctx, num_channels),
                      (gint) sample_rate, num_channels);
        return true;
    }

    gboolean open_audio(InputPlayback *playback)
    {
        return playback->output->open_audio(SAMPLE_FMT(bits_per_sample), sample_rate, num_channels);
    }

    void process_buffer(InputPlayback *playback, guint32 num_samples)
    {
        guint32 i;
        gint32* rp = input;
        gint8* wp = reinterpret_cast<gint8*>(output);
        gint16* wp2 = reinterpret_cast<gint16*>(output);
        gint32* wp4 = reinterpret_cast<gint32*>(output);

        if (bits_per_sample % 8 != 0) {
            AUDDBG("Can not convert to %d bps: not a multiple of 8\n", bits_per_sample);
        }

        if (bits_per_sample == 8) {
            for (i=0; i<num_samples * num_channels; i++, wp++, rp++) {
                *wp = *rp & 0xff;
            }
        } else if (bits_per_sample == 16) {
            for (i=0; i<num_samples * num_channels; i++, wp2++, rp++) {
                *wp2 = *rp & 0xffff;
            }
        } else if (bits_per_sample == 24 || bits_per_sample == 32) { /* 24bit value stored in lowest 3 bytes */
           for (i=0; i<num_samples * num_channels; i++, wp4++, rp++) {
               *wp4 = *rp;
           }
        }

        playback->pass_audio(playback, SAMPLE_FMT(bits_per_sample), 
		num_channels, 
		num_samples * num_channels * SAMPLE_SIZE(bits_per_sample),
		output,
		NULL);
    }
};

InputPlugin *wv_iplist[] = { &wvpack, NULL };

DECLARE_PLUGIN(wavpack, NULL, NULL, wv_iplist, NULL, NULL, NULL, NULL,NULL);

static gint
wv_is_our_fd(gchar *filename, VFSFile *file)
{
    WavpackDecoder d(&wvpack);

    if (d.attach(filename, file))
        return TRUE;

    return FALSE;
}

void
load_tag(ape_tag *tag, WavpackContext *ctx) 
{
    memset(tag, 0, sizeof(ape_tag));
    WavpackGetTagItem(ctx, "Album", tag->album, sizeof(tag->album));
    WavpackGetTagItem(ctx, "Artist", tag->artist, sizeof(tag->artist));
    WavpackGetTagItem(ctx, "Comment", tag->comment, sizeof(tag->comment));
    WavpackGetTagItem(ctx, "Genre", tag->genre, sizeof(tag->genre));
    WavpackGetTagItem(ctx, "Title", tag->title, sizeof(tag->title));
    WavpackGetTagItem(ctx, "Track", tag->track, sizeof(tag->track));
    WavpackGetTagItem(ctx, "Year", tag->year, sizeof(tag->year));
}

static void *
end_thread()
{
    return 0;
}

static void *
DecodeThread(InputPlayback *playback)
{
    gint bps;
    WavpackDecoder d(&wvpack);

    if (!d.attach_to_play(playback)) {
        killDecodeThread = true;
        return end_thread();
    }
    bps = WavpackGetBytesPerSample(d.ctx) * d.num_channels;

    AUDDBG("reading WavPack file, %dHz, %d channels and %dbits\n", d.sample_rate, d.num_channels, d.bits_per_sample);

    if (!d.open_audio(playback)) {
        AUDDBG("error opening audio channel\n");
        killDecodeThread = true;
        AudioError = true;
        openedAudio = false;
    }
    else {
        AUDDBG("opened audio channel\n");
        openedAudio = true;
    }
    guint32 status;
    guint samples_left;

    while (!killDecodeThread) {
        if (isSeek != -1) {
            AUDDBG("seeking to position %d\n", isSeek);
            WavpackSeekSample(d.ctx, (gint)(isSeek * d.sample_rate));
            isSeek = -1;
        }

        samples_left = d.num_samples-WavpackGetSampleIndex(d.ctx);
        //AUDDBG("samples left: %d\n", samples_left);
        if (paused == 0) {
            status = WavpackUnpackSamples(d.ctx, d.input, BUFFER_SIZE);
            if (status == (guint32) -1) {
                printf("wavpack: Error decoding file.\n");
                break;
            }
            else if (samples_left == 0 && playback->output->buffer_playing() == 0) {
                killDecodeThread = true;
                break;
            }
            else {
                d.process_buffer(playback, status);
            }
        }
        else {
            g_usleep(10000);
        }
    }
    return end_thread();
}

static void
wv_play(InputPlayback *data)
{
    paused = 0;
    isSeek = -1;
    killDecodeThread = false;
    AudioError = false;
    thread_handle = g_thread_self();
    data->set_pb_ready(data);
    DecodeThread(data);
    return;
}

static std::string
WavpackPluginGetQualityString(WavpackContext *ctx)
{
    int mode = WavpackGetMode(ctx);

    if (mode & MODE_LOSSLESS)
        return "lossless";

    if (mode & MODE_HYBRID)
        return "lossy (hybrid)";

    return "lossy";
}

static Tuple *
aud_tuple_from_WavpackContext(const gchar *fn, WavpackContext *ctx)
{
    ape_tag tag;
    Tuple *ti;
    gint sample_rate = WavpackGetSampleRate(ctx);

    ti = aud_tuple_new_from_filename(fn);

    load_tag(&tag, ctx);

    aud_tuple_associate_string(ti, FIELD_TITLE, NULL, tag.title);
    aud_tuple_associate_string(ti, FIELD_ARTIST, NULL, tag.artist);
    aud_tuple_associate_string(ti, FIELD_ALBUM, NULL, tag.album);
    aud_tuple_associate_string(ti, FIELD_GENRE, NULL, tag.genre);
    aud_tuple_associate_string(ti, FIELD_COMMENT, NULL, tag.comment);
    aud_tuple_associate_string(ti, FIELD_DATE, NULL, tag.year);
    aud_tuple_associate_string(ti, FIELD_QUALITY, NULL, WavpackPluginGetQualityString(ctx).c_str());
    aud_tuple_associate_string(ti, FIELD_CODEC, NULL, "WavPack");

    aud_tuple_associate_int(ti, FIELD_TRACK_NUMBER, NULL, atoi(tag.track));
    aud_tuple_associate_int(ti, FIELD_YEAR, NULL, atoi(tag.year));
    aud_tuple_associate_int(ti, FIELD_LENGTH, NULL, (int)(WavpackGetNumSamples(ctx) / sample_rate) * 1000);

    return ti;
}

static gchar *
generate_title(const gchar *fn, WavpackContext *ctx)
{
    static gchar *displaytitle = NULL;
    Tuple *ti;

    ti = aud_tuple_from_WavpackContext(fn, ctx);

    displaytitle = aud_tuple_formatter_make_title_string(ti, aud_get_gentitle_format());
    if (!displaytitle || *displaytitle == '\0')
        displaytitle = g_strdup(fn);

    aud_tuple_free((void *) ti);

    return displaytitle;
}

static Tuple *
wv_get_song_tuple(gchar *filename)
{
    Tuple *ti;
    WavpackDecoder d(&wvpack);

    if (!d.attach(filename)) {
        printf("wavpack: Error opening file: \"%s\"\n", filename);
        return NULL;
    }

    ti = aud_tuple_from_WavpackContext(filename, d.ctx);

    return ti;
}

static Tuple *
wv_probe_for_tuple(gchar *filename, VFSFile *file)
{
    Tuple *ti;
    WavpackDecoder d(&wvpack);

    if (!d.attach(filename, file))
        return NULL;

    ti = aud_tuple_from_WavpackContext(filename, d.ctx);

    return ti;
}

static void
wv_get_song_info(gchar *filename, gchar **title, gint *length)
{
    assert(filename != NULL);
    WavpackDecoder d(&wvpack);

    if (!d.attach(filename)) {
        printf("wavpack: Error opening file: \"%s\"\n", filename);
        return;
    }

    gint sample_rate = WavpackGetSampleRate(d.ctx);
#ifdef AUD_DEBUG
    gint num_channels = WavpackGetNumChannels(d.ctx);
#endif
    AUDDBG("reading %s at %d rate with %d channels\n", filename, sample_rate, num_channels);

    *length = (gint)(WavpackGetNumSamples(d.ctx) / sample_rate) * 1000,
    *title = generate_title(filename, d.ctx);
    AUDDBG("title for %s = %s\n", filename, *title);
}

static gint
wv_get_time(InputPlayback *data)
{
    if (data->output == NULL)
        return -1;
    if (AudioError)
        return -2;
    if (killDecodeThread && !data->output->buffer_playing())
        return -1;
    return data->output->output_time();
}

static void
wv_seek(InputPlayback *data, gint sec)
{
    isSeek = sec;
    data->output->flush((gint) (1000 * isSeek));
}

static void
wv_pause(InputPlayback *data, gshort pause)
{
    data->output->pause(paused = pause);
}

static void
wv_stop(InputPlayback *data)
{
    killDecodeThread = true;
    if (thread_handle != 0) {
        g_thread_join(thread_handle);
        if (openedAudio) {
            data->output->buffer_free();
            data->output->close_audio();
        }
        openedAudio = false;
        if (AudioError)
            printf("Could not open Audio\n");
    }

}

static void
wv_load_config()
{
    mcs_handle_t *cfg;

    cfg = aud_cfg_db_open();

    aud_cfg_db_get_bool(cfg, "wavpack", "clip_prevention",
                          &clipPreventionEnabled);
    aud_cfg_db_get_bool(cfg, "wavpack", "album_replaygain",
                          &albumReplaygainEnabled);
    aud_cfg_db_get_bool(cfg, "wavpack", "dyn_bitrate", &dynBitrateEnabled);
    aud_cfg_db_get_bool(cfg, "wavpack", "replaygain", &replaygainEnabled);
    aud_cfg_db_close(cfg);

    openedAudio = false;
}
