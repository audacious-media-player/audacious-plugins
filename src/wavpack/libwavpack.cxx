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
#include <audacious/configdb.h>
#include <audacious/main.h>
#include <audacious/util.h>
#include <audacious/vfs.h>
}
#include <glib.h>
#include <gtk/gtk.h>
#include <iconv.h>
#include <math.h>
#include "tags.h"
#include "../../config.h"
#ifndef M_LN10
#define M_LN10   2.3025850929940456840179914546843642
#endif

#ifdef DEBUG
# define DBG(format, args...) fprintf(stderr, format, ## args)
#else
# define DBG(format, args...)
#endif

#define BUFFER_SIZE 256 // read buffer size, in samples

static void wv_load_config();
static int wv_is_our_fd(gchar *filename, VFSFile *file);
static Tuple *wv_probe_for_tuple(gchar *filename, VFSFile *file);
static void wv_play(InputPlayback *);
static void wv_stop(InputPlayback *);
static void wv_pause(InputPlayback *, short);
static void wv_seek(InputPlayback *, int);
static int wv_get_time(InputPlayback *);
static void wv_get_song_info(char *, char **, int *);
static char *generate_title(const char *, WavpackContext *ctx);
static double isSeek;
static short paused;
static bool killDecodeThread;
static bool AudioError;
static GThread *thread_handle;
static Tuple *wv_get_song_tuple(char *);

// in ui.cpp
void wv_configure();
void wv_about_box(void);
void wv_file_info_box(char *);
extern gboolean clipPreventionEnabled;
extern gboolean dynBitrateEnabled;
extern gboolean replaygainEnabled;
extern gboolean albumReplaygainEnabled;
extern gboolean openedAudio;

const gchar *wv_fmts[] = { "wv", NULL };

InputPlugin mod = {
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
    NULL,                       //set eq
    wv_get_time,
    NULL,                       //get volume
    NULL,                       //set volume
    NULL,                       //cleanup
    NULL,                       //obsolete
    NULL,                       //add_vis
    NULL,
    wv_get_song_info,
    wv_file_info_box,           //info box
    NULL,                       //output
    wv_get_song_tuple,
    NULL,
    NULL,
    wv_is_our_fd,
    (gchar **)wv_fmts,
    NULL,			// high precision seeking
    wv_probe_for_tuple		// probe for a tuple
};

int32_t read_bytes (void *id, void *data, int32_t bcount)
{
    return aud_vfs_fread (data, 1, bcount, (VFSFile *) id);
}

uint32_t get_pos (void *id)
{
    return aud_vfs_ftell ((VFSFile *) id);
}

int set_pos_abs (void *id, uint32_t pos)
{
    return aud_vfs_fseek ((VFSFile *) id, pos, SEEK_SET);
}

int set_pos_rel (void *id, int32_t delta, int mode)
{
    return aud_vfs_fseek ((VFSFile *) id, delta, mode);
}

int push_back_byte (void *id, int c)
{
    return aud_vfs_ungetc (c, (VFSFile *) id);
}

uint32_t get_length (void *id)
{
    VFSFile *file = (VFSFile *) id;
    uint32_t sz = 0;

    if (file == NULL)
        return 0;

    aud_vfs_fseek(file, 0, SEEK_END);
    sz = aud_vfs_ftell(file);
    aud_vfs_fseek(file, 0, SEEK_SET);

    return sz;
}

/* XXX streams?? */
int can_seek (void *id)
{
    return 1;
}

int32_t write_bytes (void *id, void *data, int32_t bcount)
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
    InputPlugin *mod;
    int32_t *input;
    int16_t *output;
    int sample_rate;
    int num_channels;
    WavpackContext *ctx;
    char error_buff[4096]; // TODO: fixme!
    VFSFile *wv_Input, *wvc_Input;

    WavpackDecoder(InputPlugin *mod) : mod(mod)
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

    bool attach(const char *filename)
    {
        wv_Input = aud_vfs_fopen(filename, "rb");

        char *corrFilename = g_strconcat(filename, "c", NULL);

        wvc_Input = aud_vfs_fopen(corrFilename, "rb");

        g_free(corrFilename);

        ctx = WavpackOpenFileInputEx(&reader, wv_Input, wvc_Input, error_buff, OPEN_TAGS | OPEN_WVC, 0);

        if (ctx == NULL) {
            return false;
        }

        return true;
    }

    bool attach(gchar *filename, VFSFile *wvi)
    {
        ctx = WavpackOpenFileInputEx(&reader, wvi, NULL, error_buff, OPEN_TAGS, 0);

        if (ctx == NULL)
            return false;

        return true;
    }

    bool attach_to_play(InputPlayback *playback)
    {
        wv_Input = aud_vfs_fopen(playback->filename, "rb");

        char *corrFilename = g_strconcat(playback->filename, "c", NULL);

        wvc_Input = aud_vfs_fopen(corrFilename, "rb");

        g_free(corrFilename);

        ctx = WavpackOpenFileInputEx(&reader, wv_Input, wvc_Input, error_buff, OPEN_TAGS | OPEN_WVC, 0);

        if (ctx == NULL)
            return false;

        sample_rate = WavpackGetSampleRate(ctx);
        num_channels = WavpackGetNumChannels(ctx);
        input = (int32_t *)calloc(BUFFER_SIZE, num_channels * sizeof(int32_t));
        output = (int16_t *)calloc(BUFFER_SIZE, num_channels * sizeof(int16_t));
        playback->set_params(playback, generate_title(playback->filename, ctx),
                      (int) (WavpackGetNumSamples(ctx) / sample_rate) * 1000,
                      (int) WavpackGetAverageBitrate(ctx, num_channels),
                      (int) sample_rate, num_channels);
        return true;
    }

    bool open_audio()
    {
        return mod->output->open_audio(FMT_S16_NE, sample_rate, num_channels);
    }

    void process_buffer(InputPlayback *playback, size_t num_samples)
    {
        /* TODO: dithering */
        for (unsigned int i = 0; i < num_samples * num_channels; i++)
            output[i] = input[i];

        playback->pass_audio(playback, FMT_S16_NE, 
		num_channels, 
		num_samples * num_channels * sizeof(int16_t),
		output,
		NULL);
    }
};

InputPlugin *wv_iplist[] = { &mod, NULL };

DECLARE_PLUGIN(wavpack, NULL, NULL, wv_iplist, NULL, NULL, NULL, NULL,NULL);

static int
wv_is_our_fd(gchar *filename, VFSFile *file)
{
    WavpackDecoder d(&mod);

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
    ape_tag tag;
    char *filename = playback->filename;
    int bps_updateCounter = 0;
    int bps;
    int i;
    WavpackDecoder d(&mod);

    if (!d.attach_to_play(playback)) {
        killDecodeThread = true;
        return end_thread();
    }
    bps = WavpackGetBytesPerSample(d.ctx) * d.num_channels;
    DBG("reading %s at %d rate with %d channels\n", filename, d.sample_rate, d.num_channels);

    if (!d.open_audio()) {
        DBG("error opening audio channel\n");
        killDecodeThread = true;
        AudioError = true;
        openedAudio = false;
    }
    else {
        DBG("opened audio channel\n");
        openedAudio = true;
    }
    unsigned status;
    char *display = generate_title(filename, d.ctx);
    int length = (int) (1000 * WavpackGetNumSamples(d.ctx));

    while (!killDecodeThread) {
        if (isSeek != -1) {
            DBG("seeking to position %d\n", isSeek);
            WavpackSeekSample(d.ctx, (int)(isSeek * d.sample_rate));
            isSeek = -1;
        }
        if (paused == 0
            && (mod.output->buffer_free() >=
                (1152 * 2 *
                 (16 / 8)) << (mod.output->buffer_playing()? 1 : 0))) {
            status =
                WavpackUnpackSamples(d.ctx, d.input, BUFFER_SIZE);
            if (status == (unsigned) (-1)) {
                printf("wavpack: Error decoding file.\n");
                break;
            }
            else if (status == 0) {
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
aud_tuple_from_WavpackContext(const char *fn, WavpackContext *ctx)
{
    ape_tag tag;
    Tuple *ti;
    int sample_rate = WavpackGetSampleRate(ctx);

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

static char *
generate_title(const char *fn, WavpackContext *ctx)
{
    static char *displaytitle = NULL;
    Tuple *ti;

    ti = aud_tuple_from_WavpackContext(fn, ctx);

    displaytitle = aud_tuple_formatter_make_title_string(ti, aud_get_gentitle_format());
    if (!displaytitle || *displaytitle == '\0')
        displaytitle = g_strdup(fn);

    aud_tuple_free((void *) ti);

    return displaytitle;
}

static Tuple *
wv_get_song_tuple(char *filename)
{
    Tuple *ti;
    WavpackDecoder d(&mod);

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
    WavpackDecoder d(&mod);

    if (!d.attach(filename, file))
        return NULL;

    ti = aud_tuple_from_WavpackContext(filename, d.ctx);

    return ti;
}

static void
wv_get_song_info(char *filename, char **title, int *length)
{
    assert(filename != NULL);
    WavpackDecoder d(&mod);

    if (!d.attach(filename)) {
        printf("wavpack: Error opening file: \"%s\"\n", filename);
        return;
    }

    int sample_rate = WavpackGetSampleRate(d.ctx);
    int num_channels = WavpackGetNumChannels(d.ctx);
    DBG("reading %s at %d rate with %d channels\n", filename, sample_rate, num_channels);

    *length = (int)(WavpackGetNumSamples(d.ctx) / sample_rate) * 1000,
    *title = generate_title(filename, d.ctx);
    DBG("title for %s = %s\n", filename, *title);
}

static int
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
wv_seek(InputPlayback *data, int sec)
{
    isSeek = sec;
    data->output->flush((int) (1000 * isSeek));
}

static void
wv_pause(InputPlayback *data, short pause)
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
            mod.output->buffer_free();
            mod.output->close_audio();
        }
        openedAudio = false;
        if (AudioError)
            printf("Could not open Audio\n");
    }

}

static void
wv_load_config()
{
    ConfigDb *cfg;

    cfg = bmp_cfg_db_open();

    bmp_cfg_db_get_bool(cfg, "wavpack", "clip_prevention",
                          &clipPreventionEnabled);
    bmp_cfg_db_get_bool(cfg, "wavpack", "album_replaygain",
                          &albumReplaygainEnabled);
    bmp_cfg_db_get_bool(cfg, "wavpack", "dyn_bitrate", &dynBitrateEnabled);
    bmp_cfg_db_get_bool(cfg, "wavpack", "replaygain", &replaygainEnabled);
    bmp_cfg_db_close(cfg);

    openedAudio = false;
}
