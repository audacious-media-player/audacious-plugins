#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wavpack/wputils.h>
extern "C" {
#include <audacious/plugin.h>
#include <audacious/output.h>
#include <audacious/configdb.h>
#include <audacious/titlestring.h>
#include <audacious/util.h>
}
#include <glib.h>
#include <gtk/gtk.h>
#include <iconv.h>
#include <math.h>
#include "tags.h"
#ifndef M_LN10
#define M_LN10   2.3025850929940456840179914546843642
#endif

#ifdef DEBUG
# define DBG(format, args...) fprintf(stderr, format, ## args)
#else
# define DBG(format, args...)
#endif

#define BUFFER_SIZE 256 // read buffer size, in samples

extern "C" InputPlugin * get_iplugin_info(void);
static void wv_load_config();
static int wv_is_our_file(char *);
static void wv_play(char *);
static void wv_stop(void);
static void wv_pause(short);
static void wv_seek(int);
static int wv_get_time(void);
static void wv_get_song_info(char *, char **, int *);
static char *generate_title(const char *, WavpackContext *ctx);
static double isSeek;
static short paused;
static bool killDecodeThread;
static bool AudioError;
static GThread *thread_handle;
static TitleInput *wv_get_song_tuple(char *);

// in ui.cpp
void wv_configure();
void wv_about_box(void);
void wv_file_info_box(char *);
extern gboolean clipPreventionEnabled;
extern gboolean dynBitrateEnabled;
extern gboolean replaygainEnabled;
extern gboolean albumReplaygainEnabled;
extern gboolean openedAudio;

InputPlugin mod = {
    NULL,                       //handle
    NULL,                       //filename
    NULL,
    wv_load_config,
    wv_about_box,
    wv_configure,
    wv_is_our_file,
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
    NULL,
    wv_get_song_info,
    wv_file_info_box,          //info box
    NULL,                       //output
    wv_get_song_tuple,
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

    WavpackDecoder(InputPlugin *mod) : mod(mod)
    {
        ctx = NULL;
        input = NULL;
        output = NULL;
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
            WavpackCloseFile(ctx);
            ctx = NULL;
        }
    }

    bool attach(const char *filename)
    {
        ctx = WavpackOpenFileInput(filename, error_buff, OPEN_TAGS | OPEN_WVC, 0);

        if (ctx == NULL) {
            return false;
        }

        sample_rate = WavpackGetSampleRate(ctx);
        num_channels = WavpackGetNumChannels(ctx);
        input = (int32_t *)calloc(BUFFER_SIZE, num_channels * sizeof(int32_t));
        output = (int16_t *)calloc(BUFFER_SIZE, num_channels * sizeof(int16_t));
        mod->set_info(generate_title(filename, ctx),
                      (int) (WavpackGetNumSamples(ctx) / sample_rate) * 1000,
                      (int) WavpackGetAverageBitrate(ctx, num_channels),
                      (int) sample_rate, num_channels);
        return true;
    }

    bool open_audio()
    {
        return mod->output->open_audio(FMT_S16_NE, sample_rate, num_channels);
    }

    void process_buffer(size_t num_samples)
    {
        for (int i = 0; i < num_samples * num_channels; i++) {
            output[i] = input[i];
        }
        produce_audio(mod->output->output_time(), FMT_S16_NE, 
		num_channels, 
		num_samples * num_channels * sizeof(int16_t),
		output,
		NULL);
    }
};

extern "C" InputPlugin *
get_iplugin_info(void)
{
    mod.description =
        g_strdup_printf(("Wavpack Decoder Plugin %s"), VERSION);
    return &mod;
}

static int
wv_is_our_file(char *filename)
{
    char *ext;

    ext = strrchr(filename, '.');
    if (ext) {
        if (!strcasecmp(ext, ".wv")) {
            return TRUE;
        }
    }
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

static char *
convertUTF8toLocale(char *utf8)
{
    // note - opens a new iconv descriptor for each call
    // will have to find a way to reuse the descriptor if this turns
    // out to be too slow
    iconv_t idesc = iconv_open("", "UTF-8");
    if (idesc == (iconv_t) -1) {
        perror("iconv_open failed");
        return g_strdup(utf8);
    }

    size_t in_left = strlen(utf8);
    size_t out_left = 2 * in_left + 1;
    char *buf = (char *)g_malloc(out_left);
    char *in = utf8;
    char *out = buf;

    memset(buf, 0, out_left);
    size_t err = iconv(idesc, &in, &in_left, &out, &out_left);
    iconv_close(idesc);
    return buf;
}

static void *
end_thread()
{
    return 0;
}

static void *
DecodeThread(void *a)
{
    ape_tag tag;
    char *filename = (char *) a;
    int bps_updateCounter = 0;
    int bps;
    int i;
    WavpackDecoder d(&mod);

    if (!d.attach(filename)) {
        printf("wavpack: Error opening file: \"%s\"\n", filename);
        killDecodeThread = true;
        return end_thread();
    }
    bps = WavpackGetBytesPerSample(d.ctx) * d.num_channels;
    DBG("reading %s at %d rate with %d channels\n", filename, d.sample_rate, d.num_channels);

    if (!d.open_audio()) {
        DBG("error opening xmms audio channel\n");
        killDecodeThread = true;
        AudioError = true;
        openedAudio = false;
    }
    else {
        DBG("opened xmms audio channel\n");
        openedAudio = true;
    }
    unsigned status;
    char *display = generate_title(filename, d.ctx);
    int length = (int) (1000 * WavpackGetNumSamples(d.ctx));

    while (!killDecodeThread) {
        if (isSeek != -1) {
            DBG("seeking to position %d\n", isSeek);
            WavpackSeekSample(d.ctx, isSeek * d.sample_rate);
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
                d.process_buffer(status);
            }
        }
        else {
            xmms_usleep(10000);
        }
    }
    return end_thread();
}

static void
wv_play(char *filename)
{
    paused = 0;
    isSeek = -1;
    killDecodeThread = false;
    AudioError = false;
    thread_handle = g_thread_create(DecodeThread, (void *) filename, TRUE, NULL);
    return;
}

static TitleInput *
tuple_from_WavpackContext(const char *fn, WavpackContext *ctx)
{
    ape_tag tag;
    TitleInput *ti;
    int sample_rate = WavpackGetSampleRate(ctx);

    ti = bmp_title_input_new();

    ti->file_name = g_path_get_basename(fn);
    ti->file_path = g_path_get_dirname(fn);
    ti->file_ext = "wv";

    load_tag(&tag, ctx);

    ti->track_name = g_strdup(tag.title);
    ti->performer = g_strdup(tag.artist);
    ti->album_name = g_strdup(tag.album);
    ti->date = g_strdup(tag.year);
    ti->track_number = atoi(tag.track);
    if (ti->track_number < 0)
        ti->track_number = 0;
    ti->year = atoi(tag.year);
    if (ti->year < 0)
        ti->year = 0;
    ti->genre = g_strdup(tag.genre);
    ti->comment = g_strdup(tag.comment);
    ti->length = (int)(WavpackGetNumSamples(ctx) / sample_rate) * 1000;

    return ti;
}

static char *
generate_title(const char *fn, WavpackContext *ctx)
{
    static char *displaytitle = NULL;
    TitleInput *ti;

    ti = tuple_from_WavpackContext(fn, ctx);

    displaytitle = xmms_get_titlestring(xmms_get_gentitle_format(), ti);
    if (!displaytitle || *displaytitle == '\0')
        displaytitle = g_strdup(fn);

    bmp_title_input_free(ti);

    return displaytitle;
}

static TitleInput *
wv_get_song_tuple(char *filename)
{
    TitleInput *ti;
    char error_buff[4096]; // TODO: fixme!
    WavpackContext *ctx = WavpackOpenFileInput(filename, error_buff, OPEN_TAGS | OPEN_WVC, 0);

    if (ctx == NULL) {
        printf("wavpack: Error opening file: \"%s: %s\"\n", filename, error_buff);
        return NULL;
    }

    ti = tuple_from_WavpackContext(filename, ctx);

    WavpackCloseFile(ctx);

    return ti;
}

static void
wv_get_song_info(char *filename, char **title, int *length)
{
    assert(filename != NULL);
    char error_buff[4096]; // TODO: fixme!
    WavpackContext *ctx = WavpackOpenFileInput(filename, error_buff, OPEN_TAGS | OPEN_WVC, 0);
    if (ctx == NULL) {
        printf("wavpack: Error opening file: \"%s: %s\"\n", filename, error_buff);
        return;
    }
    int sample_rate = WavpackGetSampleRate(ctx);
    int num_channels = WavpackGetNumChannels(ctx);
    DBG("reading %s at %d rate with %d channels\n", filename, sample_rate, num_channels);

    *length = (int)(WavpackGetNumSamples(ctx) / sample_rate) * 1000,
    *title = generate_title(filename, ctx);
    DBG("title for %s = %s\n", filename, *title);
    WavpackCloseFile(ctx);
}

static int
wv_get_time(void)
{
    if (!mod.output)
        return -1;
    if (AudioError)
        return -2;
    if (killDecodeThread && !mod.output->buffer_playing())
        return -1;
    return mod.output->output_time();
}


static void
wv_seek(int sec)
{
    isSeek = sec;
    mod.output->flush((int) (1000 * isSeek));
}

static void
wv_pause(short pause)
{
    mod.output->pause(paused = pause);
}

static void
wv_stop(void)
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
