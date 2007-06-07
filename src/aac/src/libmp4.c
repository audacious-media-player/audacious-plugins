#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include "faad.h"
#include "mp4ff.h"
#include "tagging.h"

#include <audacious/plugin.h>
#include <audacious/output.h>
#include <audacious/util.h>
#include <audacious/titlestring.h>
#include <audacious/vfs.h>
#include <audacious/i18n.h>

#define MP4_VERSION VERSION

#define SBR_DEC

extern VFSFile *vfs_buffered_file_new_from_uri(gchar *uri);

/*
 * BUFFER_SIZE is the highest amount of memory that can be pulled.
 * We use this for sanity checks, among other things, as mp4ff needs
 * a labotomy sometimes.
 */
#define BUFFER_SIZE FAAD_MIN_STREAMSIZE*64

/*
 * AAC_MAGIC is the pattern that marks the beginning of an MP4 container.
 */
#define AAC_MAGIC     (unsigned char [4]) { 0xFF, 0xF9, 0x5C, 0x80 }

static void        mp4_init(void);
static void        mp4_about(void);
static int         mp4_is_our_file(char *);
static void        mp4_play(InputPlayback *);
static void        mp4_stop(InputPlayback *);
static void        mp4_pause(InputPlayback *, short);
static void        mp4_seek(InputPlayback *, int);
static int         mp4_get_time(InputPlayback *);
static void        mp4_cleanup(void);
static void        mp4_get_song_title_len(char *filename, char **, int *);
static TitleInput* mp4_get_song_tuple(char *);
static int         mp4_is_our_fd(char *, VFSFile *);

static gchar *fmts[] = { "m4a", "mp4", "aac", NULL };

static void *   mp4_decode(void *);
static gchar *  mp4_get_song_title(char *filename);
static void     audmp4_file_info_box(gchar *);
gboolean        buffer_playing;

InputPlugin mp4_ip =
{
    .description = "MP4 Audio Plugin",
    .init = mp4_init,
    .about = mp4_about,
    .is_our_file = mp4_is_our_file,
    .play_file = mp4_play,
    .stop = mp4_stop,
    .pause = mp4_pause,
    .seek = mp4_seek,
    .cleanup = mp4_cleanup,
    .get_song_info = mp4_get_song_title_len,
    .get_song_tuple = mp4_get_song_tuple,
    .is_our_file_from_vfs = mp4_is_our_fd,
    .vfs_extensions = fmts,
};

InputPlugin *mp4_iplist[] = { &mp4_ip, NULL };

DECLARE_PLUGIN(mp4, NULL, NULL, mp4_iplist, NULL, NULL, NULL, NULL);

typedef struct  _mp4cfg
{
#define FILE_UNKNOWN    0
#define FILE_MP4        1
#define FILE_AAC        2
    gshort        file_type;
} Mp4Config;

static Mp4Config mp4cfg;
static GThread   *decodeThread;
GStaticMutex     mutex = G_STATIC_MUTEX_INIT;
static int       seekPosition = -1;

void getMP4info(char*);
int getAACTrack(mp4ff_t *);

static guint32 mp4_read_callback(void *data, void *buffer, guint32 len)
{
    if (data == NULL || buffer == NULL)
        return -1;

    return vfs_fread(buffer, 1, len, (VFSFile *) data);
}

static guint32 mp4_seek_callback(void *data, guint64 pos)
{
    if (data == NULL)
        return -1;

    return vfs_fseek((VFSFile *) data, pos, SEEK_SET);
}

static gchar *
extname(const char *filename)
{   
    gchar *ext = strrchr(filename, '.');

    if (ext != NULL)
        ++ext;

    return ext;
}

static void mp4_init(void)
{
    mp4cfg.file_type = FILE_UNKNOWN;
    seekPosition = -1;
    return;
}

static void mp4_play(InputPlayback *playback)
{
    buffer_playing = TRUE;
    playback->playing = 1; //XXX should acquire lock?
    decodeThread = g_thread_create((GThreadFunc)mp4_decode, playback, TRUE, NULL);
}

static void mp4_stop(InputPlayback *playback)
{
    if (buffer_playing)
    {
        buffer_playing = FALSE;
        playback->playing = 0; //XXX should acquire lock?
        g_thread_join(decodeThread);
        playback->output->close_audio();
    }
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
        int i = 0, sr, fl = 0, id;
        static int srates[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 0, 0, 0};

        if((buf[i] != 0xFF) || ((buf[i+1] & 0xF6) != 0xF0))
                return 0;

        id = (buf[i+1] >> 3) & 0x01;    //id=1 mpeg2, 0: mpeg4
        sr = (buf[i+2] >> 2)  & 0x0F;
        if(sr > 11)
                return 0;
        *srate = srates[sr];

        fl = ((buf[i+3] & 0x03) << 11) | (buf[i+4] << 3) | ((buf[i+5] >> 5) & 0x07);
        *num = (buf[i+6] & 0x02) + 1;

        return fl;
}

static gboolean parse_aac_stream(VFSFile *stream)
{
        int cnt = 0, c, len, srate, num;
        off_t init, probed;
	static guchar buf[8];

        init = probed = vfs_ftell(stream);
        while(probed-init <= 32768 && cnt < 8)
        {
                c = 0;
                while(probed-init <= 32768 && c != 0xFF)
                {
                        c = vfs_getc(stream);
                        if(c < 0)
                                return FALSE;
	                probed = vfs_ftell(stream);
                }
                buf[0] = 0xFF;
                if(vfs_fread(&(buf[1]), 1, 7, stream) < 7)
                        return FALSE;

                len = aac_parse_frame(buf, &srate, &num);
                if(len > 0)
                {
                        cnt++;
                        vfs_fseek(stream, len - 8, SEEK_CUR);
                }
                probed = vfs_ftell(stream);
        }

        if(cnt < 8)
                return FALSE;

        return TRUE;
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

static int mp4_is_our_file(char *filename)
{
  VFSFile *file;
  gchar* extension;
  gchar magic[8];

  memset(magic, '\0', 8);

  extension = strrchr(filename, '.');
  if ((file = vfs_fopen(filename, "rb"))) {
      vfs_fread(magic, 1, 8, file);
      vfs_rewind(file);
      if (parse_aac_stream(file) == TRUE) {
           vfs_fclose(file);
           return TRUE;
      }
      if (!memcmp(magic, "ID3", 3)) {       // ID3 tag bolted to the front, obfuscated magic bytes
           vfs_fclose(file);
           if (extension &&(
          !strcasecmp(extension, ".mp4") || // official extension
          !strcasecmp(extension, ".m4a") || // Apple mp4 extension
          !strcasecmp(extension, ".aac")    // old MPEG2/4-AAC extension
       ))
          return 1;
       else
          return 0;
      }
      if (!memcmp(&magic[4], "ftyp", 4)) {
           vfs_fclose(file);
           return 1;
      }
      vfs_fclose(file);
  }
  return 0;
}

static int mp4_is_our_fd(char *filename, VFSFile* file)
{
  gchar* extension;
  gchar magic[8];

  extension = strrchr(filename, '.');
  vfs_fread(magic, 1, 8, file);
  vfs_rewind(file);
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
    else
      return 0;
  }
  return 0;
}

static void mp4_about(void)
{
    static GtkWidget *aboutbox = NULL;
    aboutbox = xmms_show_message("About MP4 AAC player plugin",
                   "Using libfaad2-" FAAD2_VERSION " for decoding.\n"
		   "FAAD2 AAC/HE-AAC/HE-AACv2/DRM decoder (c) Nero AG, www.nero.com\n"
                   "Copyright (c) 2005-2006 Audacious team",
                   "Ok", FALSE, NULL, NULL);
    g_signal_connect(G_OBJECT(aboutbox), "destroy",
                     G_CALLBACK(gtk_widget_destroyed),
                     &aboutbox);
}

static void mp4_pause(InputPlayback *playback, short flag)
{
    playback->output->pause(flag);
}

static void mp4_seek(InputPlayback *data, int time)
{
    seekPosition = time;
    while(buffer_playing && seekPosition != -1)
        xmms_usleep(10000);
}

static int mp4_get_time(InputPlayback *playback)
{
    if(!buffer_playing)
        return (-1);
    else
        return (playback->output->output_time());
}

static void mp4_cleanup(void)
{
}

static TitleInput *mp4_get_song_tuple(char *fn)
{
    mp4ff_callback_t *mp4cb = g_malloc0(sizeof(mp4ff_callback_t));
    VFSFile *mp4fh;
    mp4ff_t *mp4file;
    TitleInput *input = NULL;
    gchar *filename = g_strdup(fn);
    gboolean remote = str_has_prefix_nocase(filename, "http:") ||
	              str_has_prefix_nocase(filename, "https:");

    mp4fh = remote ? vfs_buffered_file_new_from_uri(filename) : vfs_fopen(filename, "rb");

    /* check if this file is an ADTS stream, if so return a blank tuple */
    if (parse_aac_stream(mp4fh))
    {
        g_free(mp4cb);

        input = bmp_title_input_new();

        input->track_name = vfs_get_metadata(mp4fh, "track-name");
        input->album_name = vfs_get_metadata(mp4fh, "stream-name");

        input->file_name = g_path_get_basename(filename);
        input->file_path = g_path_get_dirname(filename);
        input->file_ext = extname(filename);

        input->mtime = 0;
        input->length = -1;

        return input;
    }

    vfs_rewind(mp4fh);

    mp4cb->read = mp4_read_callback;
    mp4cb->seek = mp4_seek_callback;
    mp4cb->user_data = mp4fh;   

    if (!(mp4file = mp4ff_open_read(mp4cb))) {
        g_free(mp4cb);
        vfs_fclose(mp4fh);
    } else {
        gint mp4track= getAACTrack(mp4file);
        gint numSamples = mp4ff_num_samples(mp4file, mp4track);
        guint framesize = 1024;
        gulong samplerate;
        guchar channels;
        gint msDuration;
        mp4AudioSpecificConfig mp4ASC;
        gchar *tmpval;
        guchar *buffer = NULL;
        guint bufferSize = 0;
        faacDecHandle decoder;

        if (mp4track == -1)
            return NULL;

        decoder = faacDecOpen();
        mp4ff_get_decoder_config(mp4file, mp4track, &buffer, &bufferSize);

        if ( !buffer ) {
            faacDecClose(decoder);
            return FALSE;
        }
        if ( faacDecInit2(decoder, buffer, bufferSize, 
                  &samplerate, &channels) < 0 ) {
            faacDecClose(decoder);

            return FALSE;
        }

        /* Add some hacks for SBR profile */
        if (AudioSpecificConfig(buffer, bufferSize, &mp4ASC) >= 0) {
            if (mp4ASC.frameLengthFlag == 1) framesize = 960;
            if (mp4ASC.sbr_present_flag == 1) framesize *= 2;
        }
            
        g_free(buffer);

        faacDecClose(decoder);

        msDuration = ((float)numSamples * (float)(framesize - 1.0)/(float)samplerate) * 1000;

        input = bmp_title_input_new();

        mp4ff_meta_get_title(mp4file, &input->track_name);
        mp4ff_meta_get_album(mp4file, &input->album_name);
        mp4ff_meta_get_artist(mp4file, &input->performer);
        mp4ff_meta_get_date(mp4file, &tmpval);
        mp4ff_meta_get_genre(mp4file, &input->genre);

        if (tmpval)
        {
            input->year = atoi(tmpval);
            free(tmpval);
        }

        input->file_name = g_path_get_basename(filename);
        input->file_path = g_path_get_dirname(filename);
        input->file_ext = extname(filename);
        input->length = msDuration;

        free (mp4cb);
        vfs_fclose(mp4fh);
    }

    return input;
}

static void mp4_get_song_title_len(char *filename, char **title, int *len)
{
    (*title) = mp4_get_song_title(filename);
    (*len) = -1;
}

static gchar   *mp4_get_song_title(char *filename)
{
    mp4ff_callback_t *mp4cb = g_malloc0(sizeof(mp4ff_callback_t));
    VFSFile *mp4fh;
    mp4ff_t *mp4file;
    gchar *title = NULL;

    mp4fh = vfs_fopen(filename, "rb");
    mp4cb->read = mp4_read_callback;
    mp4cb->seek = mp4_seek_callback;
    mp4cb->user_data = mp4fh;   

    if (!(mp4file = mp4ff_open_read(mp4cb))) {
        g_free(mp4cb);
        vfs_fclose(mp4fh);
    } else {
        TitleInput *input;
        gchar *tmpval;

        input = bmp_title_input_new();

        mp4ff_meta_get_title(mp4file, &input->track_name);
        mp4ff_meta_get_album(mp4file, &input->album_name);
        mp4ff_meta_get_artist(mp4file, &input->performer);
        mp4ff_meta_get_date(mp4file, &tmpval);
        mp4ff_meta_get_genre(mp4file, &input->genre);

        if (tmpval)
        {
            input->year = atoi(tmpval);
            free(tmpval);
        }

        input->file_name = g_path_get_basename(filename);
        input->file_path = g_path_get_dirname(filename);
        input->file_ext = extname(filename);

        title = xmms_get_titlestring(xmms_get_gentitle_format(), input);

        free (input->track_name);
        free (input->album_name);
        free (input->performer);
        free (input->genre);
        free (input->file_name);
        free (input->file_path);
        free (input);

        free (mp4cb);
        vfs_fclose(mp4fh);
    }

    if (!title)
    {
        title = g_path_get_basename(filename);
        if (extname(title))
            *(extname(title) - 1) = '\0';
    }

    return title;
}

static int my_decode_mp4( InputPlayback *playback, char *filename, mp4ff_t *mp4file )
{
    // We are reading an MP4 file
    gint mp4track= getAACTrack(mp4file);
    faacDecHandle   decoder;
    mp4AudioSpecificConfig mp4ASC;
    guchar      *buffer = NULL;
    guint       bufferSize = 0;
    gulong      samplerate;
    guchar      channels;
    gulong      msDuration;
    gulong      numSamples;
    gulong      sampleID = 1;
    guint       framesize = 1024;

    if (mp4track < 0)
    {
        g_print("Unsupported Audio track type\n");
        return TRUE;
    }

    gchar *xmmstitle = NULL;
    xmmstitle = mp4_get_song_title(filename);
    if(xmmstitle == NULL)
        xmmstitle = g_strdup(filename);

    decoder = faacDecOpen();
    mp4ff_get_decoder_config(mp4file, mp4track, &buffer, &bufferSize);
    if ( !buffer ) {
        faacDecClose(decoder);
        return FALSE;
    }
    if ( faacDecInit2(decoder, buffer, bufferSize, 
              &samplerate, &channels) < 0 ) {
        faacDecClose(decoder);

        return FALSE;
    }

    /* Add some hacks for SBR profile */
    if (AudioSpecificConfig(buffer, bufferSize, &mp4ASC) >= 0) {
        if (mp4ASC.frameLengthFlag == 1) framesize = 960;
        if (mp4ASC.sbr_present_flag == 1) framesize *= 2;
    }
        
    g_free(buffer);
    if( !channels ) {
        faacDecClose(decoder);

        return FALSE;
    }
    numSamples = mp4ff_num_samples(mp4file, mp4track);
    msDuration = ((float)numSamples * (float)(framesize - 1.0)/(float)samplerate) * 1000;
    playback->output->open_audio(FMT_S16_NE, samplerate, channels);
    playback->output->flush(0);

    mp4_ip.set_info(xmmstitle, msDuration, 
            mp4ff_get_avg_bitrate( mp4file, mp4track ), 
            samplerate,channels);

    while ( buffer_playing ) {
        void*           sampleBuffer;
        faacDecFrameInfo    frameInfo;    
        gint            rc;

        /* Seek if seek position has changed */
        if ( seekPosition!=-1 ) {
            sampleID =  (float)seekPosition*(float)samplerate/(float)(framesize - 1.0);
            playback->output->flush(seekPosition*1000);
            seekPosition = -1;
        }

        /* Otherwise continue playing */
        buffer=NULL;
        bufferSize=0;

        /* If we've run to the end of the file, we're done. */
        if(sampleID >= numSamples){
            /* Finish playing before we close the
               output. */
            while ( playback->output->buffer_playing() ) {
                xmms_usleep(10000);
            }

            playback->output->flush(seekPosition*1000);
            playback->output->close_audio();
            faacDecClose(decoder);

            g_static_mutex_lock(&mutex);
            buffer_playing = FALSE;
            g_static_mutex_unlock(&mutex);
            g_thread_exit(NULL);

            return FALSE;
        }
        rc= mp4ff_read_sample(mp4file, mp4track, 
                  sampleID++, &buffer, &bufferSize);

        /*g_print(":: %d/%d\n", sampleID-1, numSamples);*/

        /* If we can't read the file, we're done. */
        if((rc == 0) || (buffer== NULL) || (bufferSize == 0) || (bufferSize > BUFFER_SIZE)){
            g_print("MP4: read error\n");
            sampleBuffer = NULL;
            sampleID=0;
            playback->output->buffer_free();
            playback->output->close_audio();

            faacDecClose(decoder);

            return FALSE;
        }

/*          g_print(" :: %d/%d\n", bufferSize, BUFFER_SIZE); */

        sampleBuffer= faacDecDecode(decoder, 
                        &frameInfo, 
                        buffer, 
                        bufferSize);

        /* If there was an error decoding, we're done. */
        if(frameInfo.error > 0){
            g_print("MP4: %s\n",
                faacDecGetErrorMessage(frameInfo.error));
            playback->output->close_audio();
            faacDecClose(decoder);

            return FALSE;
        }
        if(buffer){
            g_free(buffer);
            buffer=NULL;
            bufferSize=0;
        }
        if (buffer_playing == FALSE)
        {
                playback->output->close_audio();
            return FALSE;
        }
        produce_audio(playback->output->written_time(),
                   FMT_S16_NE,
                   channels,
                   frameInfo.samples<<1,
                   sampleBuffer, &buffer_playing);
    }

    playback->output->close_audio();
    faacDecClose(decoder);

    return TRUE;
}

void my_decode_aac( InputPlayback *playback, char *filename, VFSFile *file )
{
    faacDecHandle   decoder = 0;
    guchar      streambuffer[BUFFER_SIZE];
    gulong      bufferconsumed = 0;
    gulong      samplerate = 0;
    guchar      channels;
    gulong      buffervalid = 0;
    TitleInput* input;
    gchar       *ttemp = NULL, *stemp = NULL;
    gchar       *temp = g_strdup(filename);
    gchar       *ext  = strrchr(temp, '.');
    gchar       *xmmstitle = NULL;
    faacDecConfigurationPtr config;
    gboolean    remote = str_has_prefix_nocase(filename, "http:") ||
			 str_has_prefix_nocase(filename, "https:");

    vfs_rewind(file);
    if((decoder = faacDecOpen()) == NULL){
        g_print("AAC: Open Decoder Error\n");
        vfs_fclose(file);
        buffer_playing = FALSE;
        g_static_mutex_unlock(&mutex);
        g_thread_exit(NULL);
    }
    if((buffervalid = vfs_fread(streambuffer, 1, BUFFER_SIZE, file))==0){
        g_print("AAC: Error reading file\n");
        vfs_fclose(file);
        buffer_playing = FALSE;
        faacDecClose(decoder);
        g_static_mutex_unlock(&mutex);
        g_thread_exit(NULL);
    }
    if(!strncmp((char*)streambuffer, "ID3", 3)){
        gint size = 0;

        vfs_fseek(file, 0, SEEK_SET);
        size = (streambuffer[6]<<21) | (streambuffer[7]<<14) | 
		(streambuffer[8]<<7) | streambuffer[9];
        size+=10;
        vfs_fread(streambuffer, 1, size, file);
        buffervalid = vfs_fread(streambuffer, 1, BUFFER_SIZE, file);
    }

    ttemp = vfs_get_metadata(file, "stream-name");

    if (ttemp != NULL)
    {
        xmmstitle = g_strdup(ttemp);
        g_free(ttemp);
    }
    else
        xmmstitle = g_strdup(g_basename(temp));

    bufferconsumed = aac_probe(streambuffer, buffervalid);
    if(bufferconsumed) {
      buffervalid -= bufferconsumed;
      memmove(streambuffer, &streambuffer[bufferconsumed], buffervalid);
      buffervalid += vfs_fread(&streambuffer[buffervalid], 1,
                     BUFFER_SIZE-buffervalid, file);
      bufferconsumed = 0;
    }

    bufferconsumed = faacDecInit(decoder,
                     streambuffer,
                     buffervalid,
                     &samplerate,
                     &channels);
#ifdef DEBUG
    g_print("samplerate: %d, channels: %d\n", samplerate, channels);
#endif
    if(playback->output->open_audio(FMT_S16_NE,samplerate,channels) == FALSE){
        g_print("AAC: Output Error\n");
        faacDecClose(decoder);
        vfs_fclose(file);
        playback->output->close_audio();
        g_free(xmmstitle);
        buffer_playing = FALSE;
        g_static_mutex_unlock(&mutex);
        g_thread_exit(NULL);
    }

    mp4_ip.set_info(xmmstitle, -1, -1, samplerate, channels);
    playback->output->flush(0);

    while(buffer_playing && buffervalid > 0 && streambuffer != NULL)
    {
        faacDecFrameInfo    finfo;
        unsigned long   samplesdecoded;
        char*       sample_buffer = NULL;

        if(bufferconsumed > 0)
        {
            buffervalid -= bufferconsumed;
            memmove(streambuffer, &streambuffer[bufferconsumed], buffervalid);
            buffervalid += vfs_fread(&streambuffer[buffervalid], 1,
                         BUFFER_SIZE-buffervalid, file);
            bufferconsumed = 0;

            ttemp = vfs_get_metadata(file, "stream-name");

            if (ttemp != NULL)
                stemp = vfs_get_metadata(file, "track-name");

            if (stemp != NULL)
            {
                static gchar *ostmp = NULL;

                if (ostmp == NULL || g_ascii_strcasecmp(stemp, ostmp))
                {
                    if (xmmstitle != NULL)
                        g_free(xmmstitle);

                    xmmstitle = g_strdup_printf("%s (%s)", stemp, ttemp);

                    if (ostmp != NULL)
                        g_free(ostmp);

                    ostmp = stemp;

                    mp4_ip.set_info(xmmstitle, -1, -1, samplerate, channels);
                }
            }

            g_free(ttemp);
            ttemp = NULL;
        }

        sample_buffer = faacDecDecode(decoder, &finfo, streambuffer, buffervalid);

        bufferconsumed += finfo.bytesconsumed;
        samplesdecoded = finfo.samples;

        if(finfo.error > 0 && remote != FALSE)
        {
	    buffervalid--;
            memmove(streambuffer, &streambuffer[1], buffervalid);
            if(buffervalid < BUFFER_SIZE) {
               buffervalid +=
                 vfs_fread(&streambuffer[buffervalid], 1, BUFFER_SIZE-buffervalid, file);
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
            g_print("AAC: decoded %d samples!\n", samplesdecoded);
#endif
            continue;
        }

        produce_audio(playback->output->written_time(),
                   FMT_S16_LE, channels,
                   samplesdecoded<<1, sample_buffer, &buffer_playing);
    }
    playback->output->buffer_free();
    playback->output->close_audio();
    buffer_playing = FALSE;
    faacDecClose(decoder);
    g_free(xmmstitle);
    vfs_fclose(file);
    seekPosition = -1;

    buffer_playing = FALSE;
    g_static_mutex_unlock(&mutex);
    g_thread_exit(NULL);
}

static void *mp4_decode( void *args )
{
    mp4ff_callback_t *mp4cb = g_malloc0(sizeof(mp4ff_callback_t));
    VFSFile *mp4fh;
    mp4ff_t *mp4file;
    gboolean ret;

    InputPlayback *playback = args;
    char *filename = playback->filename;

    mp4fh = vfs_buffered_file_new_from_uri(filename);

    g_static_mutex_lock(&mutex);
    seekPosition= -1;
    buffer_playing= TRUE;
    g_static_mutex_unlock(&mutex);

    if (mp4fh == NULL)
        g_thread_exit(NULL);

    ret = parse_aac_stream(mp4fh);
    vfs_fclose(mp4fh);

    mp4fh = vfs_fopen(filename, "rb");
    mp4cb->read = mp4_read_callback;
    mp4cb->seek = mp4_seek_callback;
    mp4cb->user_data = mp4fh;

    mp4file= mp4ff_open_read(mp4cb);
  
    if( ret == TRUE ) {
        g_free(mp4cb);
        my_decode_aac( playback, filename, mp4fh );
    }
    else
        my_decode_mp4( playback, filename, mp4file );

    return NULL;
}
