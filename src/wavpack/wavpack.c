#include "config.h"
#include <wavpack/wavpack.h>

#include <audacious/debug.h>
#include <audacious/plugin.h>
#include <audacious/i18n.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>
#include <audacious/audtag.h>
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
static gboolean stop_flag = FALSE;

/* Audacious VFS wrappers for Wavpack stream reading
 */

static gint32
wv_read_bytes(void *id, void *data, gint32 bcount)
{
    return vfs_fread(data, 1, bcount, (VFSFile *) id);
}

static guint32
wv_get_pos(void *id)
{
    return vfs_ftell((VFSFile *) id);
}

static gint
wv_set_pos_abs(void *id, guint32 pos)
{
    return vfs_fseek((VFSFile *) id, pos, SEEK_SET);
}

static gint
wv_set_pos_rel(void *id, gint32 delta, gint mode)
{
    return vfs_fseek((VFSFile *) id, delta, mode);
}

static gint
wv_push_back_byte(void *id, gint c)
{
    return vfs_ungetc(c, (VFSFile *) id);
}

static guint32
wv_get_length(void *id)
{
    VFSFile *file = (VFSFile *) id;

    if (file == NULL)
        return 0;

    return vfs_fsize(file);
}

static gint wv_can_seek(void *id)
{
    return (vfs_is_streaming((VFSFile *) id) == FALSE);
}

static gint32 wv_write_bytes(void *id, void *data, gint32 bcount)
{
    return vfs_fwrite(data, 1, bcount, (VFSFile *) id);
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

static gboolean wv_attach (const gchar * filename, VFSFile * wv_input,
 VFSFile * * wvc_input, WavpackContext * * ctx, gchar * error, gint flags)
{
    gchar *corrFilename;

    if (flags & OPEN_WVC)
    {
        corrFilename = g_strconcat(filename, "c", NULL);
        *wvc_input = vfs_fopen(corrFilename, "rb");
        g_free(corrFilename);
    }

    * ctx = WavpackOpenFileInputEx (& wv_readers, wv_input, * wvc_input, error,
     flags, 0);

    if (ctx == NULL)
        return FALSE;
    else
        return TRUE;
}

static void wv_deattach (VFSFile * wvc_input, WavpackContext * ctx)
{
    if (wvc_input != NULL)
        vfs_fclose(wvc_input);
    WavpackCloseFile(ctx);
}

static gboolean wv_play (InputPlayback * playback, const gchar * filename,
 VFSFile * file, gint start_time, gint stop_time, gboolean pause)
{
    if (file == NULL)
        return FALSE;

    gint32 *input = NULL;
    void *output = NULL;
    gint sample_rate, num_channels, bits_per_sample;
    guint num_samples;
    WavpackContext *ctx = NULL;
    VFSFile *wvc_input = NULL;
    gboolean error = FALSE;

    if (! wv_attach (filename, file, & wvc_input, & ctx, NULL, OPEN_TAGS |
     OPEN_WVC))
    {
        g_warning("Error opening Wavpack file '%s'.", filename);
        error = TRUE;
        goto error_exit;
    }

    sample_rate = WavpackGetSampleRate(ctx);
    num_channels = WavpackGetNumChannels(ctx);
    bits_per_sample = WavpackGetBitsPerSample(ctx);
    num_samples = WavpackGetNumSamples(ctx);

    if (!playback->output->open_audio(SAMPLE_FMT(bits_per_sample), sample_rate, num_channels))
    {
        g_warning("Error opening audio output.");
        error = TRUE;
        goto error_exit;
    }

    if (pause)
        playback->output->pause(TRUE);

    input = g_malloc(BUFFER_SIZE * num_channels * sizeof(guint32));
    output = g_malloc(BUFFER_SIZE * num_channels * SAMPLE_SIZE(bits_per_sample));
    if (input == NULL || output == NULL)
        goto error_exit;

    playback->set_gain_from_playlist(playback);

    g_mutex_lock(ctrl_mutex);

    playback->set_params(playback, (gint) WavpackGetAverageBitrate(ctx, num_channels),
        sample_rate, num_channels);

    seek_value = (start_time > 0) ? start_time : -1;
    stop_flag = FALSE;

    playback->set_pb_ready(playback);

    g_mutex_unlock(ctrl_mutex);

    while (!stop_flag && (stop_time < 0 ||
     playback->output->written_time () < stop_time))
    {
        gint ret;
        guint samples_left;

        /* Handle seek and pause requests */
        g_mutex_lock(ctrl_mutex);

        if (seek_value >= 0)
        {
            playback->output->flush (seek_value);
            WavpackSeekSample (ctx, (gint64) seek_value * sample_rate / 1000);
            seek_value = -1;
            g_cond_signal(ctrl_cond);
        }

        g_mutex_unlock(ctrl_mutex);

        /* Decode audio data */
        samples_left = num_samples - WavpackGetSampleIndex(ctx);

        ret = WavpackUnpackSamples(ctx, input, BUFFER_SIZE);
        if (samples_left == 0)
            stop_flag = TRUE;
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

            playback->output->write_audio(output, ret * num_channels * SAMPLE_SIZE(bits_per_sample));
        }
    }

    /* Flush buffer */
    g_mutex_lock(ctrl_mutex);

    while (!stop_flag && playback->output->buffer_playing())
        g_usleep(20000);

    g_cond_signal(ctrl_cond);
    g_mutex_unlock(ctrl_mutex);

error_exit:

    g_free(input);
    g_free(output);
    wv_deattach (wvc_input, ctx);

    stop_flag = TRUE;
    playback->output->close_audio();
    return ! error;
}

static void
wv_stop(InputPlayback * playback)
{
    g_mutex_lock(ctrl_mutex);

    if (!stop_flag)
    {
        stop_flag = TRUE;
        playback->output->abort_write();
        g_cond_signal(ctrl_cond);
    }

    g_mutex_unlock (ctrl_mutex);
}

static void
wv_pause(InputPlayback * playback, gboolean pause)
{
    g_mutex_lock(ctrl_mutex);

    if (!stop_flag)
        playback->output->pause(pause);

    g_mutex_unlock(ctrl_mutex);
}

static void wv_seek (InputPlayback * playback, gint time)
{
    g_mutex_lock(ctrl_mutex);

    if (!stop_flag)
    {
        seek_value = time;
        playback->output->abort_write();
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
wv_probe_for_tuple(const gchar * filename, VFSFile * fd)
{
    WavpackContext *ctx;
    Tuple *tu;
    gchar error[1024];

    ctx = WavpackOpenFileInputEx(&wv_readers, fd, NULL, error, OPEN_TAGS, 0);

    if (ctx == NULL)
        return NULL;

	AUDDBG("starting probe of %p\n", fd);

	vfs_rewind(fd);
	tu = tuple_new_from_filename(filename);

	vfs_rewind(fd);
	tag_tuple_read(tu, fd);

	tuple_associate_int(tu, FIELD_LENGTH, NULL,
        ((guint64) WavpackGetNumSamples(ctx) * 1000) / (guint64) WavpackGetSampleRate(ctx));
    tuple_associate_string(tu, FIELD_CODEC, NULL, "WavPack");
    tuple_associate_string(tu, FIELD_QUALITY, NULL, wv_get_quality(ctx));

    WavpackCloseFile(ctx);

	AUDDBG("returning tuple %p for file %p\n", tu, fd);
	return tu;
}

static void
wv_about_box()
{
    static GtkWidget *about_window = NULL;

    audgui_simple_message(&about_window, GTK_MESSAGE_INFO,
    g_strdup_printf(_("Wavpack Decoder Plugin %s"), VERSION),
    _("Copyright (c) 2006 William Pitcock <nenolod -at- nenolod.net>\n\n"
    "Some of the plugin code was by Miles Egan\n"
    "Visit the Wavpack site at http://www.wavpack.com/\n"));
}

static gboolean wv_init (void)
{
    ctrl_mutex = g_mutex_new();
    ctrl_cond = g_cond_new();
    return TRUE;
}

static void
wv_cleanup(void)
{
    g_mutex_free(ctrl_mutex);
    g_cond_free(ctrl_cond);
}

static gboolean wv_write_tag (const Tuple * tuple, VFSFile * handle)
{
    return tag_tuple_write(tuple, handle, TAG_TYPE_APE);
}

static const gchar *wv_fmts[] = { "wv", NULL };

extern PluginPreferences preferences;

AUD_INPUT_PLUGIN
(
    .name = "WavPack decoder",
    .init = wv_init,
    .cleanup = wv_cleanup,
    .about = wv_about_box,
    .play = wv_play,
    .stop = wv_stop,
    .pause = wv_pause,
    .mseek = wv_seek,
    .extensions = wv_fmts,
    .probe_for_tuple = wv_probe_for_tuple,
    .update_song_tuple = wv_write_tag,
)
