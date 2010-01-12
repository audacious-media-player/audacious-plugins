//#define AUD_DEBUG

#include "config.h"
#include "common.h"
#include <audacious/plugin.h>
#include <audacious/i18n.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>

#define BUFFER_SIZE 256 /* read buffer size, in samples / frames */
#define SAMPLE_SIZE(a) (a == 8 ? sizeof(guint8) : (a == 16 ? sizeof(guint16) : sizeof(guint32)))
#define SAMPLE_FMT(a) (a == 8 ? FMT_S8 : (a == 16 ? FMT_S16_NE : (a == 24 ? FMT_S24_NE : FMT_S32_NE)))


/* Global mutexes etc.
 */
static GMutex *ctrl_mutex = NULL;
static GCond *ctrl_cond = NULL;
static gint64 seek_value = -1;
static gboolean pause_flag;


/* Audacious VFS wrappers for Wavpack stream reading
 */
static gint32
wv_read_bytes(void *id, void *data, gint32 bcount)
{
    return aud_vfs_fread(data, 1, bcount, (VFSFile *) id);
}

static guint32
wv_get_pos(void *id)
{
    return aud_vfs_ftell((VFSFile *) id);
}

static gint
wv_set_pos_abs(void *id, guint32 pos)
{
    return aud_vfs_fseek((VFSFile *) id, pos, SEEK_SET);
}

static gint
wv_set_pos_rel(void *id, gint32 delta, gint mode)
{
    return aud_vfs_fseek((VFSFile *) id, delta, mode);
}

static gint
wv_push_back_byte(void *id, gint c)
{
    return aud_vfs_ungetc(c, (VFSFile *) id);
}

static guint32
wv_get_length(void *id)
{
    VFSFile *file = (VFSFile *) id;

    if (file == NULL)
        return 0;

    return aud_vfs_fsize(file);
}

static gint wv_can_seek(void *id)
{
    return (aud_vfs_is_streaming((VFSFile *) id) == FALSE);
}

static gint32 wv_write_bytes(void *id, void *data, gint32 bcount)
{
    return aud_vfs_fwrite(data, 1, bcount, (VFSFile *) id);
}

WavpackStreamReader wv_readers = {
    wv_read_bytes,
    wv_get_pos,
    wv_set_pos_abs,
    wv_set_pos_rel,
    wv_push_back_byte,
    wv_get_length,
    wv_can_seek,
    wv_write_bytes
};


void wv_get_tags(WavPackTag * tag, WavpackContext * ctx)
{
    memset(tag, 0, sizeof(WavPackTag));
    WavpackGetTagItem(ctx, "Album", tag->album, sizeof(tag->album));
    WavpackGetTagItem(ctx, "Artist", tag->artist, sizeof(tag->artist));
    WavpackGetTagItem(ctx, "Comment", tag->comment, sizeof(tag->comment));
    WavpackGetTagItem(ctx, "Genre", tag->genre, sizeof(tag->genre));
    WavpackGetTagItem(ctx, "Title", tag->title, sizeof(tag->title));
    WavpackGetTagItem(ctx, "Track", tag->track, sizeof(tag->track));
    WavpackGetTagItem(ctx, "Year", tag->year, sizeof(tag->year));
}

gboolean wv_attach(const gchar *filename, VFSFile **wv_input, VFSFile **wvc_input, WavpackContext **ctx, gchar *error, gint flags)
{
    gchar *corrFilename;

    *wv_input = aud_vfs_fopen(filename, "rb");
    if (*wv_input == NULL)
        return FALSE;

    if (flags & OPEN_WVC)
    {
        corrFilename = g_strconcat(filename, "c", NULL);
        *wvc_input = aud_vfs_fopen(corrFilename, "rb");
        g_free(corrFilename);
    }

    *ctx = WavpackOpenFileInputEx(&wv_readers, *wv_input, *wvc_input, error, flags, 0);

    if (ctx == NULL)
        return FALSE;
    else
        return TRUE;
}

void wv_deattach(VFSFile *wv_input, VFSFile *wvc_input, WavpackContext *ctx)
{
    if (wv_input != NULL)
        aud_vfs_fclose(wv_input);
    if (wvc_input != NULL)
        aud_vfs_fclose(wvc_input);
    WavpackCloseFile(ctx);
}

void wv_play(InputPlayback * playback)
{
    gshort paused = 0;
    gint32 *input = NULL;
    void *output = NULL;
    gint sample_rate, num_channels, bits_per_sample;
    guint num_samples, length;
    gchar error[1024];  // fixme?! FIX ME halb apua hilfe scheisse --ccr
    WavpackContext *ctx = NULL;
    VFSFile *wv_input = NULL, *wvc_input = NULL;

    if (!wv_attach(playback->filename, &wv_input, &wvc_input, &ctx, (gchar *)&error, OPEN_TAGS | OPEN_WVC))
    {
        g_warning("Error opening Wavpack file '%s'.", playback->filename);
        playback->error = 2;
        goto error_exit;
    }

    sample_rate = WavpackGetSampleRate(ctx);
    num_channels = WavpackGetNumChannels(ctx);
    bits_per_sample = WavpackGetBitsPerSample(ctx);
    num_samples = WavpackGetNumSamples(ctx);
    length = num_samples / sample_rate;

//    fprintf(stderr, "reading WavPack file, %dHz, %d channels and %dbits, num_samples=%d\n", sample_rate, num_channels, bits_per_sample, num_samples);

    if (!playback->output->open_audio(SAMPLE_FMT(bits_per_sample), sample_rate, num_channels))
    {
        g_warning("Error opening audio output.");
        playback->error = 1;
        goto error_exit;
    }

    input = g_malloc(BUFFER_SIZE * num_channels * sizeof(guint32));
    output = g_malloc(BUFFER_SIZE * num_channels * SAMPLE_SIZE(bits_per_sample));
    if (input == NULL || output == NULL)
        goto error_exit;

    g_mutex_lock(ctrl_mutex);

    playback->set_params(playback, NULL, 0,
        (gint) WavpackGetAverageBitrate(ctx, num_channels),
        sample_rate, num_channels);

    playback->playing = TRUE;
    playback->eof = FALSE;
    playback->set_pb_ready(playback);

    g_mutex_unlock(ctrl_mutex);

    while (playback->playing && !playback->eof)
    {
        gint ret;
        guint samples_left;

        /* Handle seek and pause requests */
        g_mutex_lock(ctrl_mutex);

        if (seek_value >= 0)
        {
            playback->output->flush(seek_value * 1000);
            WavpackSeekSample(ctx, (gint) (seek_value * sample_rate));
            seek_value = -1;
            g_cond_signal(ctrl_cond);
        }

        if (pause_flag != paused)
        {
            playback->output->pause(pause_flag);
            paused = pause_flag;
            g_cond_signal(ctrl_cond);
        }

        if (paused)
        {
            g_cond_wait(ctrl_cond, ctrl_mutex);
            g_mutex_unlock(ctrl_mutex);
            continue;
        }

        g_mutex_unlock(ctrl_mutex);

        /* Decode audio data */
        samples_left = num_samples - WavpackGetSampleIndex(ctx);

        ret = WavpackUnpackSamples(ctx, input, BUFFER_SIZE);
        if (samples_left == 0)
            playback->eof = TRUE;
        else if (ret < 0)
        {
            g_warning("Error decoding file.\n");
            break;
        }
        else
        {
            /* Perform audio data conversion and output */
            guint i;
            gint32 *rp = input;
            gint8 *wp = output;
            gint16 *wp2 = output;
            gint32 *wp4 = output;

            if (bits_per_sample == 8)
            {
                for (i = 0; i < ret * num_channels; i++, wp++, rp++)
                    *wp = *rp & 0xff;
            }
            else if (bits_per_sample == 16)
            {
                for (i = 0; i < ret * num_channels; i++, wp2++, rp++)
                    *wp2 = *rp & 0xffff;
            }
            else if (bits_per_sample == 24 || bits_per_sample == 32)
            {
                for (i = 0; i < ret * num_channels; i++, wp4++, rp++)
                    *wp4 = *rp;
            }

            playback->pass_audio(playback, SAMPLE_FMT(bits_per_sample),
                num_channels, ret * num_channels * SAMPLE_SIZE(bits_per_sample),
                output, NULL);
        }
    }

    /* Flush buffer */
    g_mutex_lock(ctrl_mutex);

    while (playback->playing && playback->output->buffer_playing())
        g_usleep(20000);

    g_cond_signal(ctrl_cond);
    g_mutex_unlock(ctrl_mutex);

error_exit:

    g_free(input);
    g_free(output);
    wv_deattach(wv_input, wvc_input, ctx);

    playback->playing = FALSE;
    playback->output->close_audio();
}

static void
wv_stop(InputPlayback * playback)
{
    g_mutex_lock(ctrl_mutex);
    playback->playing = FALSE;
    g_cond_signal(ctrl_cond);
    g_mutex_unlock(ctrl_mutex);
    g_thread_join(playback->thread);
    playback->thread = NULL;
}

static void
wv_pause(InputPlayback * playback, gshort p)
{
    g_mutex_lock(ctrl_mutex);

    if (playback->playing)
    {
        pause_flag = p;
        g_cond_signal(ctrl_cond);
        g_cond_wait(ctrl_cond, ctrl_mutex);
    }

    g_mutex_unlock(ctrl_mutex);
}

static void
wv_seek(InputPlayback * playback, gint time)
{
    g_mutex_lock(ctrl_mutex);

    if (playback->playing)
    {
        seek_value = time;
        g_cond_signal(ctrl_cond);
        g_cond_wait(ctrl_cond, ctrl_mutex);
    }

    g_mutex_unlock(ctrl_mutex);
}

static gchar *
wv_get_quality(WavpackContext *ctx)
{
    gint mode = WavpackGetMode(ctx);
    const gchar *quality;

    if (mode & MODE_LOSSLESS)
        quality = "lossless";
    else if (mode & MODE_HYBRID)
        quality = "lossy (hybrid)";
    else
        quality = "lossy";
    
    return g_strdup_printf("%s%s%s", quality,
        (mode & MODE_WVC) ? " (wvc corrected)" : "",
#ifdef MODE_DNS /* WavPack 4.50 or later */
        (mode & MODE_DNS) ? " (dynamic noise shaped)" :
#endif
        "");
}

static Tuple *
wv_get_tuple_from_file(const gchar * filename, VFSFile * fd, gchar *error)
{
    WavpackContext *ctx;
    WavPackTag tag;
    Tuple *res;

    ctx = WavpackOpenFileInputEx(&wv_readers, fd, NULL, error, OPEN_TAGS, 0);
    if (ctx == NULL)
        return NULL;

    res = aud_tuple_new_from_filename(filename);

    wv_get_tags(&tag, ctx);

    aud_tuple_associate_string_rel(res, FIELD_TITLE, NULL, aud_str_to_utf8(tag.title));
    aud_tuple_associate_string_rel(res, FIELD_ARTIST, NULL, aud_str_to_utf8(tag.artist));
    aud_tuple_associate_string_rel(res, FIELD_ALBUM, NULL, aud_str_to_utf8(tag.album));
    aud_tuple_associate_string_rel(res, FIELD_GENRE, NULL, aud_str_to_utf8(tag.genre));
    aud_tuple_associate_string_rel(res, FIELD_COMMENT, NULL, aud_str_to_utf8(tag.comment));
    aud_tuple_associate_string_rel(res, FIELD_DATE, NULL, aud_str_to_utf8(tag.year));
    aud_tuple_associate_string_rel(res, FIELD_QUALITY, NULL, wv_get_quality(ctx));
    aud_tuple_associate_string(res, FIELD_CODEC, NULL, "WavPack");

    aud_tuple_associate_int(res, FIELD_TRACK_NUMBER, NULL, atoi(tag.track));
    aud_tuple_associate_int(res, FIELD_YEAR, NULL, atoi(tag.year));
    aud_tuple_associate_int(res, FIELD_LENGTH, NULL, 
        ((guint64) WavpackGetNumSamples(ctx) * 1000) / (guint64) WavpackGetSampleRate(ctx));

    WavpackCloseFile(ctx);

    return res;
}

static Tuple *
wv_get_song_tuple(const gchar * filename)
{
    VFSFile *fd = NULL;
    gchar error[1024];
    Tuple *tuple;
    
    fd = aud_vfs_fopen(filename, "rb");
    if (fd == NULL)
        return NULL;

    tuple = wv_get_tuple_from_file(filename, fd, (gchar *) &error);

    vfs_fclose(fd);

    return tuple;
}

static Tuple *
wv_probe_for_tuple(const gchar * filename, VFSFile * fd)
{
    gchar error[1024];

    return wv_get_tuple_from_file(filename, fd, (gchar *) &error);
}

static void
wv_about_box()
{
    static GtkWidget *about_window = NULL;

    if (about_window == NULL)
    {
        about_window =
            audacious_info_dialog(g_strdup_printf
            (_("Wavpack Decoder Plugin %s"), VERSION),
            (_("Copyright (c) 2006 William Pitcock <nenolod -at- nenolod.net>\n\n"
            "Some of the plugin code was by Miles Egan\n"
            "Visit the Wavpack site at http://www.wavpack.com/\n")),
            (_("Ok")), FALSE, NULL, NULL);
        g_signal_connect(G_OBJECT(about_window), "destroy",
            G_CALLBACK(gtk_widget_destroyed), &about_window);
    }
}

WavPackConfig wv_cfg;

static void
wv_init(void)
{
    memset(&wv_cfg, 0, sizeof(wv_cfg));
    wv_read_config();
    ctrl_mutex = g_mutex_new();
    ctrl_cond = g_cond_new();
}

static void
wv_cleanup(void)
{
    g_mutex_free(ctrl_mutex);
    g_cond_free(ctrl_cond);
}

static gchar *wv_fmts[] = { "wv", NULL };

extern PluginPreferences preferences;

static InputPlugin wvpack = {
    .description = "WavPack decoder",
    .init = wv_init,
    .cleanup = wv_cleanup,
    .about = wv_about_box,
    .settings = &preferences,
    .play_file = wv_play,
    .stop = wv_stop,
    .pause = wv_pause,
    .seek = wv_seek,
    .file_info_box = wv_file_info_box,
    .get_song_tuple = wv_get_song_tuple,
    .vfs_extensions = wv_fmts,
    .probe_for_tuple = wv_probe_for_tuple,
};

InputPlugin *wv_iplist[] = { &wvpack, NULL };

DECLARE_PLUGIN(wavpack, NULL, NULL, wv_iplist, NULL, NULL, NULL, NULL,NULL);
