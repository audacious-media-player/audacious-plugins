#include "config.h"
#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include "neaacdec.h"
#include "mp4ff.h"
#include "tagging.h"

#include <audacious/plugin.h>
#include <audacious/output.h>
#include <audacious/i18n.h>

#define MP4_VERSION VERSION
#define SBR_DEC

/*
 * BUFFER_SIZE is the highest amount of memory that can be pulled.
 * We use this for sanity checks, among other things, as mp4ff needs
 * a labotomy sometimes.
 */
#define BUFFER_SIZE (FAAD_MIN_STREAMSIZE*64)

/*
 * AAC_MAGIC is the pattern that marks the beginning of an MP4 container.
 */
#define AAC_MAGIC     (unsigned char [4]) { 0xFF, 0xF9, 0x5C, 0x80 }

static void        mp4_init(void);
static void        mp4_about(void);
static void        mp4_play(InputPlayback *);
static void        mp4_cleanup(void);
static gint        mp4_is_our_fd(const char *, VFSFile *);

static gchar *fmts[] = { "m4a", "mp4", "aac", NULL };

static void *   mp4_decode(void *);

static GMutex * seek_mutex;
static GCond * seek_cond;
static gboolean pause_flag;
static gint seek_value;

typedef struct  _mp4cfg
{
#define FILE_UNKNOWN    0
#define FILE_MP4        1
#define FILE_AAC        2
    gshort        file_type;
} Mp4Config;

static Mp4Config mp4cfg;

void getMP4info(char*);
int getAACTrack(mp4ff_t *);

static guint32 mp4_read_callback(void *data, void *buffer, guint32 len)
{
    if (data == NULL || buffer == NULL)
        return -1;

    return aud_vfs_fread(buffer, 1, len, (VFSFile *) data);
}

static guint32 mp4_seek_callback (void * data, guint64 pos)
{
    g_return_val_if_fail (data != NULL, -1);
    g_return_val_if_fail (pos <= G_MAXLONG, -1);

    return aud_vfs_fseek((VFSFile *) data, pos, SEEK_SET);
}

static void mp4_init(void)
{
    mp4cfg.file_type = FILE_UNKNOWN;

    seek_mutex = g_mutex_new ();
    seek_cond = g_cond_new ();
}

static void mp4_play(InputPlayback *playback)
{
    seek_value = -1;
    pause_flag = FALSE;
    playback->playing = TRUE;

    playback->set_pb_ready(playback);
    mp4_decode(playback);
}

static void mp4_stop (InputPlayback * playback)
{
    g_mutex_lock (seek_mutex);

    if (playback->playing)
    {
        playback->playing = FALSE;
        g_cond_signal (seek_cond);
        g_mutex_unlock (seek_mutex);
        g_thread_join (playback->thread);
        playback->thread = NULL;
    }
    else
        g_mutex_unlock (seek_mutex);
}

static void mp4_pause (InputPlayback * playback, gshort p)
{
    g_mutex_lock (seek_mutex);

    if (playback->playing)
    {
        pause_flag = p;
        g_cond_signal (seek_cond);
        g_cond_wait (seek_cond, seek_mutex);
    }

    g_mutex_unlock (seek_mutex);
}

static void mp4_seek (InputPlayback * playback, gint time)
{
    g_mutex_lock (seek_mutex);

    if (playback->playing)
    {
        seek_value = time;
        g_cond_signal (seek_cond);
        g_cond_wait (seek_cond, seek_mutex);
    }

    g_mutex_unlock (seek_mutex);
}

/*
 * These routines are derived from MPlayer.
 */

/// \param srate (out) sample rate
/// \param num (out) number of audio frames in this ADTS frame
/// \return size of the ADTS frame in bytes
/// aac_parse_frames needs a buffer at least 8 bytes long
int aac_parse_frame(guchar *buf, int *srate, int *num)
{
        int i = 0, sr, fl = 0;
        static int srates[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 0, 0, 0};

        if((buf[i] != 0xFF) || ((buf[i+1] & 0xF6) != 0xF0))
                return 0;

/* We currently have no use for the id below.
        id = (buf[i+1] >> 3) & 0x01;    //id=1 mpeg2, 0: mpeg4
*/
        sr = (buf[i+2] >> 2)  & 0x0F;
        if(sr > 11)
                return 0;
        *srate = srates[sr];

        fl = ((buf[i+3] & 0x03) << 11) | (buf[i+4] << 3) | ((buf[i+5] >> 5) & 0x07);
        *num = (buf[i+6] & 0x02) + 1;

        return fl;
}

#define PROBE_DEBUG(...)

static gboolean parse_aac_stream (VFSFile * stream)
{
    guchar data[8192];
    gint offset, length, srate, num, found;

    if (aud_vfs_fread (data, 1, sizeof data, stream) != sizeof data)
    {
        PROBE_DEBUG ("Read failed.\n");
        return FALSE;
    }

    for (offset = 0; offset <= sizeof data - 8; offset ++)
    {
        if (data[offset] != 255)
            continue;

        length = aac_parse_frame (data + offset, & srate, & num);

        if (length < 8)
            continue;

        offset += length;
        goto FOUND;
    }

    PROBE_DEBUG ("No ADTS header.\n");
    return FALSE;

FOUND:
    for (found = 1; found < 3; found ++)
    {
        if (offset > sizeof data - 8)
            goto FAIL;

        length = aac_parse_frame (data + offset, & srate, & num);

        if (length < 8)
            goto FAIL;

        offset += length;
    }

    PROBE_DEBUG ("Accepted.\n");
    return TRUE;

FAIL:
    PROBE_DEBUG ("Only %d ADTS headers.\n", found);
    return FALSE;
}

static int aac_probe(unsigned char *buffer, int len)
{
  int i = 0, pos = 0;
#ifdef DEBUG
  g_print("\nAAC_PROBE: %d bytes\n", len);
#endif
  while(i <= len-4) {
    if(
       ((buffer[i] == 0xff) && ((buffer[i+1] & 0xf6) == 0xf0)) ||
       (buffer[i] == 'A' && buffer[i+1] == 'D' && buffer[i+2] == 'I' && buffer[i+3] == 'F')
    ) {
      pos = i;
      break;
    }
#ifdef DEBUG
    g_print("AUDIO PAYLOAD: %x %x %x %x\n",
	buffer[i], buffer[i+1], buffer[i+2], buffer[i+3]);
#endif
    i++;
  }
#ifdef DEBUG
  g_print("\nAAC_PROBE: ret %d\n", pos);
#endif
  return pos;
}

static gint mp4_is_our_fd(const gchar *filename, VFSFile* file)
{
  gchar* extension;
  gchar magic[8];

  extension = strrchr(filename, '.');
  aud_vfs_fread(magic, 1, 8, file);
  aud_vfs_rewind(file);
  if (parse_aac_stream(file) == TRUE)
    return 1;
  if (!memcmp(&magic[4], "ftyp", 4))
    return 1;
  if (!memcmp(magic, "ID3", 3)) {       // ID3 tag bolted to the front, obfuscated magic bytes
    if (extension &&(
      !strcasecmp(extension, ".mp4") || // official extension
      !strcasecmp(extension, ".m4a") || // Apple mp4 extension
      !strcasecmp(extension, ".aac")    // old MPEG2/4-AAC extension
    ))
      return 1;
  }
  return 0;
}

static void mp4_about(void)
{
    static GtkWidget *aboutbox = NULL;

    if (aboutbox == NULL)
    {
        gchar *about_text = g_strdup_printf(
            _("Using libfaad2-%s for decoding.\n"
            "FAAD2 AAC/HE-AAC/HE-AACv2/DRM decoder (c) Nero AG, www.nero.com\n"
            "Copyright (c) 2005-2006 Audacious team"), FAAD2_VERSION);

        aboutbox = audacious_info_dialog(
            _("About MP4 AAC decoder plugin"),
            about_text, _("Ok"), FALSE, NULL, NULL);

        g_signal_connect(G_OBJECT(aboutbox), "destroy",
            G_CALLBACK(gtk_widget_destroyed), &aboutbox);

        g_free(about_text);
    }
}

static void mp4_cleanup(void)
{
    g_mutex_free (seek_mutex);
    g_cond_free (seek_cond);
}

static Tuple * aac_get_tuple (const gchar * filename, VFSFile * handle)
{
    Tuple * tuple;
    gchar * temp;

    temp = aud_vfs_get_metadata (handle, "track-name");
    if (temp == NULL)
    {
        fprintf (stderr, "aac: No metadata for %s.\n", filename);
        return NULL;
    }

    tuple = tuple_new_from_filename (filename);
    tuple_associate_string (tuple, FIELD_CODEC, NULL, "MPEG-2/4 AAC");
    tuple_associate_string (tuple, FIELD_TITLE, NULL, temp);

    temp = aud_vfs_get_metadata (handle, "stream-name");
    if (temp != NULL)
    {
        tuple_associate_string (tuple, FIELD_ALBUM, NULL, temp);
        g_free (temp);
    }

    temp = aud_vfs_get_metadata (handle, "content-bitrate");
    if (temp != NULL)
    {
        tuple_associate_int (tuple, FIELD_BITRATE, NULL, atoi (temp) / 1000);
        g_free (temp);
    }

    return tuple;
}

static gboolean aac_title_changed (const gchar * filename, VFSFile * handle,
 Tuple * tuple)
{
    const gchar * old = tuple_get_string (tuple, FIELD_TITLE, NULL);
    gchar * new = aud_vfs_get_metadata (handle, "track-name");
    gboolean changed = FALSE;

    changed = (new != NULL && (old == NULL || strcmp (old, new)));
    if (changed)
        tuple_associate_string (tuple, FIELD_TITLE, NULL, new);

    g_free (new);
    return changed;
}

static void read_and_set_string (mp4ff_t * mp4, gint (* func) (const mp4ff_t *
 mp4, gchar * * string), Tuple * tuple, gint field)
{
    gchar * string = NULL;

    func (mp4, & string);

    if (string != NULL)
        tuple_associate_string (tuple, field, NULL, string);

    free (string);
}

static Tuple * generate_tuple (const gchar * filename, mp4ff_t * mp4, gint track)
{
    Tuple * tuple = tuple_new_from_filename (filename);
    gint64 length;
    gint scale, rate, channels, bitrate;
    gchar * year = NULL, * cd_track = NULL;
    gchar scratch [32];

    tuple_associate_string (tuple, FIELD_CODEC, NULL, "MPEG-2/4 AAC");

    length = mp4ff_get_track_duration (mp4, track);
    scale = mp4ff_time_scale (mp4, track);

    if (length > 0 && scale > 0)
        tuple_associate_int (tuple, FIELD_LENGTH, NULL, length * 1000 / scale);

    rate = mp4ff_get_sample_rate (mp4, track);
    channels = mp4ff_get_channel_count (mp4, track);

    if (rate > 0 && channels > 0)
    {
        snprintf (scratch, sizeof scratch, "%d kHz, %s", rate / 1000, channels
         == 1 ? "mono" : channels == 2 ? "stereo" : "surround");
        tuple_associate_string (tuple, FIELD_QUALITY, NULL, scratch);
    }

    bitrate = mp4ff_get_avg_bitrate (mp4, track);

    if (bitrate > 0)
        tuple_associate_int (tuple, FIELD_BITRATE, NULL, bitrate / 1000);

    read_and_set_string (mp4, mp4ff_meta_get_title, tuple, FIELD_TITLE);
    read_and_set_string (mp4, mp4ff_meta_get_album, tuple, FIELD_ALBUM);
    read_and_set_string (mp4, mp4ff_meta_get_artist, tuple, FIELD_ARTIST);
    read_and_set_string (mp4, mp4ff_meta_get_comment, tuple, FIELD_COMMENT);
    read_and_set_string (mp4, mp4ff_meta_get_genre, tuple, FIELD_GENRE);

    mp4ff_meta_get_date (mp4, & year);

    if (year != NULL)
        tuple_associate_int (tuple, FIELD_YEAR, NULL, atoi (year));

    free (year);

    mp4ff_meta_get_track (mp4, & cd_track);

    if (cd_track != NULL)
        tuple_associate_int (tuple, FIELD_TRACK_NUMBER, NULL, atoi (cd_track));

    free (cd_track);

    return tuple;
}

static Tuple * mp4_get_tuple (const gchar * filename, VFSFile * handle)
{
    mp4ff_callback_t mp4cb;
    mp4ff_t * mp4;
    gint track;
    Tuple * tuple;

    if (parse_aac_stream (handle))
        return aac_get_tuple (filename, handle);

    aud_vfs_fseek (handle, 0, SEEK_SET);

    mp4cb.read = mp4_read_callback;
    mp4cb.seek = mp4_seek_callback;
    mp4cb.user_data = handle;

    mp4 = mp4ff_open_read (& mp4cb);

    if (mp4 == NULL)
        return NULL;

    track = getAACTrack (mp4);

    if (track < 0)
    {
        mp4ff_close (mp4);
        return NULL;
    }

    tuple = generate_tuple (filename, mp4, track);
    mp4ff_close (mp4);
    return tuple;
}

static int my_decode_mp4( InputPlayback *playback, char *filename, mp4ff_t *mp4file )
{
    // We are reading an MP4 file
    gint mp4track= getAACTrack(mp4file);
    NeAACDecHandle   decoder;
    mp4AudioSpecificConfig mp4ASC;
    guchar      *buffer = NULL;
    guint       bufferSize = 0;
    gulong      samplerate = 0;
    guchar      channels = 0;
    gulong      msDuration;
    guint       numSamples;
    gulong      sampleID = 1;
    guint       framesize = 1024;
    gboolean paused = FALSE;

    if (mp4track < 0)
    {
        g_print("Unsupported Audio track type\n");
        return TRUE;
    }

    decoder = NeAACDecOpen();
    mp4ff_get_decoder_config(mp4file, mp4track, &buffer, &bufferSize);
    if ( !buffer ) {
        NeAACDecClose(decoder);
        return FALSE;
    }
    if ( NeAACDecInit2(decoder, buffer, bufferSize,
              &samplerate, &channels) < 0 ) {
        NeAACDecClose(decoder);

        return FALSE;
    }

    /* Add some hacks for SBR profile */
    if (AudioSpecificConfig(buffer, bufferSize, &mp4ASC) >= 0) {
        if (mp4ASC.frameLengthFlag == 1) framesize = 960;
        if (mp4ASC.sbr_present_flag == 1) framesize *= 2;
    }

    g_free(buffer);
    if( !channels ) {
        NeAACDecClose(decoder);

        return FALSE;
    }
    numSamples = mp4ff_num_samples(mp4file, mp4track);
    msDuration = ((float)numSamples * (float)(framesize - 1.0)/(float)samplerate) * 1000;

    if (! playback->output->open_audio (FMT_S16_NE, samplerate, channels))
    {
        NeAACDecClose (decoder);
        playback->playing = FALSE;
        playback->error = TRUE;
        return FALSE;
    }

    playback->set_tuple (playback, generate_tuple (filename, mp4file, mp4track));
    playback->set_params(playback, NULL, 0,
            mp4ff_get_avg_bitrate( mp4file, mp4track ),
            samplerate,channels);

    while (playback->playing)
    {
        void*           sampleBuffer;
        NeAACDecFrameInfo    frameInfo;
        gint            rc;

        g_mutex_lock (seek_mutex);

        if (seek_value >= 0)
        {
            sampleID = (gint64) seek_value * samplerate / (framesize - 1);
            playback->output->flush (seek_value * 1000);
            seek_value = -1;
            g_cond_signal (seek_cond);
        }

        if (pause_flag != paused)
        {
            playback->output->pause (pause_flag);
            paused = pause_flag;
            g_cond_signal (seek_cond);
        }

        if (paused)
        {
            g_cond_wait (seek_cond, seek_mutex);
            g_mutex_unlock (seek_mutex);
            continue;
        }

        g_mutex_unlock (seek_mutex);

        buffer=NULL;
        bufferSize=0;

        /* If we've run to the end of the file, we're done. */
        if(sampleID >= numSamples){
            /* Finish playing before we close the
               output. */
            while ( playback->output->buffer_playing() ) {
                g_usleep(10000);
            }

            playback->output->close_audio();
            NeAACDecClose(decoder);

            playback->playing = FALSE;
            return FALSE;
        }
        rc= mp4ff_read_sample(mp4file, mp4track,
                  sampleID++, &buffer, &bufferSize);

        /*g_print(":: %d/%d\n", sampleID-1, numSamples);*/

        /* If we can't read the file, we're done. */
        if((rc == 0) || (buffer== NULL) || (bufferSize == 0) || (bufferSize > BUFFER_SIZE)){
            g_print("MP4: read error\n");
            sampleBuffer = NULL;
            playback->output->close_audio();

            NeAACDecClose(decoder);

            return FALSE;
        }

/*          g_print(" :: %d/%d\n", bufferSize, BUFFER_SIZE); */

        sampleBuffer= NeAACDecDecode(decoder,
                        &frameInfo,
                        buffer,
                        bufferSize);

        /* If there was an error decoding, we're done. */
        if(frameInfo.error > 0){
            g_print("MP4: %s\n",
                NeAACDecGetErrorMessage(frameInfo.error));
            playback->output->close_audio();
            NeAACDecClose(decoder);

            return FALSE;
        }
        if(buffer){
            g_free(buffer);
            buffer=NULL;
            bufferSize=0;
        }

        playback->pass_audio (playback, FMT_S16_NE, channels, 2 *
         frameInfo.samples, sampleBuffer, NULL);
    }

    playback->output->close_audio();
    NeAACDecClose(decoder);

    return TRUE;
}

void my_decode_aac( InputPlayback *playback, char *filename, VFSFile *file )
{
    NeAACDecHandle   decoder = 0;
    guchar      streambuffer[BUFFER_SIZE];
    gulong      bufferconsumed = 0;
    gulong      samplerate = 0;
    guchar      channels = 0;
    gulong      buffervalid = 0;
    gulong	ret = 0;
    gboolean    remote = aud_str_has_prefix_nocase(filename, "http:") ||
			 aud_str_has_prefix_nocase(filename, "https:");
    gboolean paused = FALSE;
    Tuple * tuple;
    gint bitrate = 0;

    tuple = aac_get_tuple (filename, file);
    if (tuple != NULL)
    {
        mowgli_object_ref (tuple);
        playback->set_tuple (playback, tuple);

        bitrate = tuple_get_int (tuple, FIELD_BITRATE, NULL);
        bitrate = 1000 * MAX (0, bitrate);
    }

    aud_vfs_rewind(file);
    if((decoder = NeAACDecOpen()) == NULL){
        g_print("AAC: Open Decoder Error\n");
        aud_vfs_fclose(file);

        playback->playing = FALSE;
        return;
    }
    if((buffervalid = aud_vfs_fread(streambuffer, 1, BUFFER_SIZE, file))==0){
        g_print("AAC: Error reading file\n");
        aud_vfs_fclose(file);
        NeAACDecClose(decoder);

        playback->playing = FALSE;
        return;
    }
    if(!strncmp((char*)streambuffer, "ID3", 3)){
        gint size = 0;

        aud_vfs_fseek(file, 0, SEEK_SET);
        size = (streambuffer[6]<<21) | (streambuffer[7]<<14) |
		(streambuffer[8]<<7) | streambuffer[9];
        size+=10;
        aud_vfs_fread(streambuffer, 1, size, file);
        buffervalid = aud_vfs_fread(streambuffer, 1, BUFFER_SIZE, file);
    }

    bufferconsumed = aac_probe(streambuffer, buffervalid);
    if(bufferconsumed) {
      buffervalid -= bufferconsumed;
      memmove(streambuffer, &streambuffer[bufferconsumed], buffervalid);
      buffervalid += aud_vfs_fread(&streambuffer[buffervalid], 1,
                     BUFFER_SIZE-buffervalid, file);
    }

    bufferconsumed = NeAACDecInit(decoder,
                     streambuffer,
                     buffervalid,
                     &samplerate,
                     &channels);
#ifdef DEBUG
    g_print("samplerate: %lu, channels: %d\n", samplerate, channels);
#endif
    if(playback->output->open_audio(FMT_S16_NE,samplerate,channels) == FALSE){
        NeAACDecClose(decoder);
        aud_vfs_fclose(file);
        playback->playing = FALSE;
        playback->error = TRUE;
        return;
    }

    playback->set_params (playback, NULL, 0, bitrate, samplerate, channels);
    playback->output->flush(0);

    while (playback->playing && buffervalid > 0 && streambuffer != NULL)
    {
        NeAACDecFrameInfo    finfo;
        unsigned long   samplesdecoded;
        char*       sample_buffer = NULL;

        g_mutex_lock (seek_mutex);

        if (seek_value >= 0)
        {
            seek_value = -1;
            g_cond_signal (seek_cond);
        }

        if (pause_flag != paused)
        {
            playback->output->pause (pause_flag);
            paused = pause_flag;
            g_cond_signal (seek_cond);
        }

        if (paused)
        {
            g_cond_wait (seek_cond, seek_mutex);
            g_mutex_unlock (seek_mutex);
            continue;
        }

        g_mutex_unlock (seek_mutex);

        if(bufferconsumed > 0)
        {
            buffervalid -= bufferconsumed;
            memmove(streambuffer, &streambuffer[bufferconsumed], buffervalid);
            ret = aud_vfs_fread(&streambuffer[buffervalid], 1,
                         BUFFER_SIZE-buffervalid, file);
            buffervalid += ret;
            bufferconsumed = 0;

            /* XXX: buffer underrun on a shoutcast stream, well this is unpleasant. --nenolod */
            if (ret == 0 && remote == TRUE)
                break;

            if (tuple != NULL && aac_title_changed (filename, file, tuple))
            {
                mowgli_object_ref (tuple);
                playback->set_tuple (playback, tuple);
            }
        }

        sample_buffer = NeAACDecDecode(decoder, &finfo, streambuffer, buffervalid);

        bufferconsumed += finfo.bytesconsumed;
        samplesdecoded = finfo.samples;

        if(finfo.error > 0 && remote != FALSE)
        {
	    buffervalid--;
            memmove(streambuffer, &streambuffer[1], buffervalid);
            if(buffervalid < BUFFER_SIZE) {
               buffervalid +=
                 aud_vfs_fread(&streambuffer[buffervalid], 1, BUFFER_SIZE-buffervalid, file);
	    }
            bufferconsumed = aac_probe(streambuffer, buffervalid);
            if(bufferconsumed) {
               buffervalid -= bufferconsumed;
               memmove(streambuffer, &streambuffer[bufferconsumed], buffervalid);
               bufferconsumed = 0;
            }
            continue;
        }

        if((samplesdecoded <= 0) && !sample_buffer){
#ifdef DEBUG
            g_print("AAC: decoded %lu samples!\n", samplesdecoded);
#endif
            continue;
        }

        playback->pass_audio (playback, FMT_S16_LE, channels, 2 *
         samplesdecoded, sample_buffer, NULL);
    }
    playback->output->close_audio();
    NeAACDecClose(decoder);
    aud_vfs_fclose(file);

    if (tuple != NULL)
        mowgli_object_unref (tuple);

    playback->playing = FALSE;
}

static void *mp4_decode( void *args )
{
    mp4ff_callback_t *mp4cb = g_malloc0(sizeof(mp4ff_callback_t));
    VFSFile *mp4fh;
    mp4ff_t *mp4file;
    gboolean ret;

    InputPlayback *playback = args;
    char *filename = playback->filename;

    mp4fh = aud_vfs_buffered_file_new_from_uri(filename);

    if (mp4fh == NULL)
        return NULL;

    ret = parse_aac_stream(mp4fh);

    if( ret == TRUE )
        aud_vfs_fseek(mp4fh, 0, SEEK_SET);
    else {
        aud_vfs_fclose(mp4fh);
        mp4fh = aud_vfs_fopen(filename, "rb");
    }

    mp4cb->read = mp4_read_callback;
    mp4cb->seek = mp4_seek_callback;
    mp4cb->user_data = mp4fh;

    mp4file= mp4ff_open_read(mp4cb);

    if( ret == TRUE ) {
        g_free(mp4cb);
        my_decode_aac( playback, filename, mp4fh );
    }
    else /* I think there's a file descriptor leak here? -jlindgren */
        my_decode_mp4( playback, filename, mp4file );

    return NULL;
}

InputPlugin mp4_ip =
{
    .description = "MP4 AAC decoder",
    .init = mp4_init,
    .about = mp4_about,
    .play_file = mp4_play,
    .stop = mp4_stop,
    .pause = mp4_pause,
    .seek = mp4_seek,
    .cleanup = mp4_cleanup,
    .is_our_file_from_vfs = mp4_is_our_fd,
    .probe_for_tuple = mp4_get_tuple,
    .vfs_extensions = fmts,
};

InputPlugin * mp4_iplist[] = {& mp4_ip, NULL};

SIMPLE_INPUT_PLUGIN (mp4, mp4_iplist)
