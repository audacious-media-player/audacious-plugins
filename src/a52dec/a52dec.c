
/* 
 *   xmms-a52dec.c
 *
 *  xmms-a52dec is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  xmms-a52dec is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <math.h>
#include <inttypes.h>
#include <gtk/gtk.h>
#include <audacious/util.h>
#include <audacious/plugin.h>
#include <audacious/configdb.h>
#include <audacious/vfs.h>

#include <a52dec/a52.h>
#include <a52dec/mm_accel.h>

#include "a52dec.h"

#define DRC_BOTH 0
#define DRC_BOOST 1
#define DRC_REDUCE 2

#define SYNC_SEARCH_LIMIT 65536
#define COMPRESSION_FACTOR_DEFAULT 0.0
#define COMPRESSION_TYPE_DEFAULT DRC_BOTH
#define NODOWNMIX_SURROUND_DEFAULT FALSE
#define NODOWNMIX_LFE_DEFAULT FALSE
#define UPMIX_STEREO_DEFAULT FALSE
#define DUALMONO_DEFAULT 1
#define REQUESTED_OUTPUT_DEFAULT A52_STEREO

sample_t *sample;
char *buf;
long int lastset_time;
char *name;
int flags, sample_rate, bit_rate, frame_size, length;
VFSFile *in_file;
int a52_run, a52_not_eof;
gint output_type; // See also: requested_output
int output_nch;

// Configuration Options.

gdouble compression_factor;
gboolean compression_type;
gint requested_output; // See also: output_type
gint dualmono_channel;
gboolean nodownmix_surround, nodownmix_lfe, upmix_stereo;

static pthread_t decode_thread = (pthread_t) NULL;
sem_t play_loop_signal;
pthread_mutex_t infile_lock;

extern InputPlugin a52_ip;

static int is_our_file_a52 (char *);
static int synchronise_a52(VFSFile *, int *, int *, int *, int *);
sample_t dynamic_range(sample_t, void *);
static void init_a52 (void);
static void *play_loop (void *);
void convertsample(sample_t *, int16_t *, int);
int16_t float2int (float);
static void play_a52 (char *);
static void stop_a52 (void);
static int get_time_a52 (void);
static void get_song_info_a52 (char *, char **, int *);
static void pause_a52 (short);
static void seek_a52 (int);
static void about_a52 (void);
static void configure_a52 (void);
static void configure_ok(GtkWidget *, gpointer);
static void configure_apply(GtkWidget *, gpointer);
static void info_a52 (char *);

InputPlugin a52_ip = {
  NULL,
  NULL,
  "AC-3 / A52 Decoding Plugin",
  init_a52,
  about_a52,
  configure_a52,
  is_our_file_a52,
  NULL,
  play_a52,
  stop_a52,
  pause_a52,
  seek_a52,
  NULL,
  get_time_a52,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  get_song_info_a52,
  info_a52,
  NULL
};

InputPlugin *
get_iplugin_info (void)
{
  return &a52_ip;
}

static void
init_a52 (void)
{
  ConfigDb *config;

  // Retrieve settings from file, setting to default if not found.
  
  config = bmp_cfg_db_open();
  if (!bmp_cfg_db_get_double(config, "xmms-a52dec", "compression_factor",
                           &compression_factor))
  {
    bmp_cfg_db_set_double(config, "xmms-a52dec", "compression_factor",
                          COMPRESSION_FACTOR_DEFAULT);
    compression_factor = COMPRESSION_FACTOR_DEFAULT;
  }

  if (!bmp_cfg_db_get_int(config, "xmms-a52dec", "compression_type",
                           &compression_type))
  {
    bmp_cfg_db_set_int(config, "xmms-a52dec", "compression_type",
                       COMPRESSION_TYPE_DEFAULT);
    compression_type = COMPRESSION_TYPE_DEFAULT;
  }

  if (!bmp_cfg_db_get_bool(config, "xmms-a52dec", "nodownmix_surround",
                           &nodownmix_surround))
  {
    bmp_cfg_db_set_bool(config, "xmms-a52dec", "nodownmix_surround",
                           NODOWNMIX_SURROUND_DEFAULT);
    nodownmix_surround = NODOWNMIX_SURROUND_DEFAULT;
  }

  if (!bmp_cfg_db_get_bool(config, "xmms-a52dec", "nodownmix_lfe",
                           &nodownmix_lfe))
  {
    bmp_cfg_db_set_bool(config, "xmms-a52dec", "nodownmix_lfe",
                           NODOWNMIX_LFE_DEFAULT);
    nodownmix_lfe = NODOWNMIX_LFE_DEFAULT;
  }

  if (!bmp_cfg_db_get_bool(config, "xmms-a52dec", "upmix_stereo",
                           &upmix_stereo))
  {
    bmp_cfg_db_set_bool(config, "xmms-a52dec", "upmix_stereo",
                           UPMIX_STEREO_DEFAULT);
    upmix_stereo = UPMIX_STEREO_DEFAULT;
  }

  if (!bmp_cfg_db_get_int(config, "xmms-a52dec", "dualmono_channel",
                           &dualmono_channel))
  {
    bmp_cfg_db_set_int(config, "xmms-a52dec", "dualmono_channel",
                           DUALMONO_DEFAULT);
    requested_output = DUALMONO_DEFAULT;
  }
                               
  bmp_cfg_db_close(config);
}

static int
is_our_file_a52 (char *filename)
{
  int l_flags, l_sample_rate, l_bit_rate, l_frame_size;
  gchar *extension;
  VFSFile *temp_file;

  temp_file = vfs_fopen (filename, "r");
  if (!temp_file)
    return FALSE;

  extension = strrchr (filename, '.');

  if (extension)
    if (!strcasecmp (extension, ".a52") || !strcasecmp (extension, ".ac3"))
        if (synchronise_a52(temp_file, &l_flags, &l_sample_rate, 
                        &l_bit_rate, &l_frame_size) == 0)
        {
	  vfs_fclose (temp_file);
	  return TRUE;
        }

  vfs_fclose (temp_file);
  return FALSE;
}

/* Synchronises a52 bitstream and set parameters.

   Starting from the current file pointer position, search up to
   SYNC_SEARCH_LIMIT bytes for a valid frame, and set file pointer
   to start of frame. file pointer will be set to an undefined position if
   no valid frame found.

   Return 0 if successful.
   Return -1 if failed. */

static int
synchronise_a52(VFSFile *file, int *l_flags, int *l_sample_rate,
                int *l_bit_rate, int *l_frame_size)
{
  int count;
  char temp_buf[7];
  
  for (count=0; count <= SYNC_SEARCH_LIMIT; count++)
  {
    if (vfs_fread(temp_buf, 7, 1, file) != 1)
      return -1;

    if (vfs_fseek(file, -6, SEEK_CUR))
      return -1;

    *l_frame_size = a52_syncinfo (temp_buf, l_flags, l_sample_rate, l_bit_rate);
    if (*l_frame_size != 0)
      break;
  }

  // No valid frame found.

  if (count == SYNC_SEARCH_LIMIT)
    return -1;

  // Valid frame found. Go back 1 bytes to return to start of frame.

  if (vfs_fseek(file, -1, SEEK_CUR))
    return -1;
  
  return 0;
}

sample_t dynamic_range(sample_t recommend, void *nothing)
{
  switch (compression_type)
  {
   case DRC_BOOST:
    if (recommend > 1)
     return pow(recommend, (double) compression_factor);
    else
     return 1;
   case DRC_REDUCE:
    if (recommend < 1)
     return pow(recommend, (double) compression_factor);
    else
     return 1;
   case DRC_BOTH:
   default:
     return pow(recommend, (double) compression_factor);
  }
}

static void *
play_loop (void *arg)
{
  int old_frame_size, old_bit_rate;
  a52_state_t *dec_state;
  char *filebuf;
  int16_t outbuf[256*6];
  int sample_type;
  int  first_run=1;
  int tempcount;
  sample_t level=32766.0;

  /* Re-initialise the a52 library. Without this, there will be
     some noise when switching songs */

  dec_state = a52_init(0);
  sample = a52_samples(dec_state);

  a52_run = 1;
  lastset_time = 0;

  old_frame_size = frame_size;
  old_bit_rate = bit_rate;
  filebuf = malloc(frame_size);

  while (a52_run)
    {
      a52_not_eof = 1;

      /* Lock the mutex before synch and read.

         This prevent a52_seek from calling an vfs_fseek between a
         synch and read. */

      pthread_mutex_lock(&infile_lock);
      
      // Re-synchronise before reading each frame.

      if (synchronise_a52(in_file, &flags, &sample_rate,
                          &bit_rate, &frame_size) == -1)
      {
        pthread_mutex_unlock(&infile_lock);
        a52_not_eof = 0;
        xmms_usleep(1000);
        continue;
      }

      // Check if frame_size has changed. If yes, realloc filebuf.
      
      if (frame_size != old_frame_size)
      {
        filebuf = realloc(filebuf, frame_size);
        old_frame_size = frame_size;
      }

      if(!vfs_fread(filebuf, frame_size, 1, in_file))
      {
        pthread_mutex_unlock(&infile_lock);
        a52_not_eof = 0;
        xmms_usleep(1000);
        continue;
      }

      pthread_mutex_unlock(&infile_lock);
      
      if (bit_rate != old_bit_rate)
      {
        a52_ip.set_info (name, length / (bit_rate / 8 / 1000),
                         bit_rate, sample_rate, output_nch);
        old_bit_rate = bit_rate;
      }

      if (nodownmix_surround == TRUE)
       sample_type = A52_3F2R | A52_ADJUST_LEVEL;
      else
       sample_type = output_type | A52_ADJUST_LEVEL;
      
      if (nodownmix_lfe == TRUE)
       sample_type |= A52_LFE;
      else
       sample_type &= A52_CHANNEL_MASK;

      // The above is for non-5.1, for 5.1 we add in the LFE channel.

      sample_type |= output_type & A52_LFE;

      if(a52_frame(dec_state, filebuf, &sample_type, &level, 0.0))
        break; // Don't know how to deal with this properly.

      a52_dynrng (dec_state, dynamic_range, NULL);

      for (tempcount = 0; tempcount < 6; tempcount++)
      {
        a52_block(dec_state);

	convertsample(sample, outbuf, sample_type);

        /* Sleep while the buffer is full */
        while (a52_run && (a52_ip.output->buffer_free () < 512 * output_nch))
	  xmms_usleep (10000);

        a52_ip.output->write_audio (outbuf, 512 * output_nch);
        
        /* Signal the play_a52 thread to let it know that we have 
           finished writing to buffer. It can safely exit now.

           Without this, get_time_a52 may run before we have started
           playing anything. get_time_a52 will find that the buffer
           is not playing (yet) and return -1 causing stop_a52 to
           stop play_loop. 
           
           We must only run this once, because play_a52 will destroy
           the semaphore once it receive the signal. */
           
        if (first_run)
        {
          sem_post(&play_loop_signal);
          first_run=0;
        }

        /* Add visualisation data. Do not use a52_ip.gettime()
           for the time. Due to buffering, the time for which the
           data is submitted and the current playing time is not
           the same. The vis data is sent in advance and must be
           set for the correct time. */

        a52_ip.add_vis_pcm (vfs_ftell(in_file) / (bit_rate / 8 / 1000),
        		    FMT_S16_LE, output_nch, 512 * output_nch, outbuf);
      }
    }

  /* If sem_post didn't run above, we'll run it now */
  
  if (first_run)
  {
    sem_post(&play_loop_signal);
    first_run=0;
  }

  // Free memory allocated by a52_init.
  
  a52_free(dec_state);

  /* If we break from the loop due to end of file, the music
     is probably still playing, so we'll sleep and wait for
     it to end. We'll set a52_not_eof to 0 to let get_time_a52
     know that we have reached the end of file.*/

  vfs_fclose (in_file);
  a52_not_eof=0;

  pthread_exit (NULL);
}

void
convertsample(sample_t *sample, int16_t *outbuf, int sample_type)
{
 int count;
 int lfe_offset;
 
 if (sample_type & A52_LFE)
  lfe_offset = 256;
 else
  lfe_offset = 0;

 for (count = 0; count < 256; count++)
  if (output_type == A52_STEREO)
   switch (sample_type & A52_CHANNEL_MASK)
   {
    case A52_MONO:
    case A52_CHANNEL1:
    case A52_CHANNEL2:
     outbuf[count * 2] = float2int (sample[count + lfe_offset]);
     outbuf[count * 2 + 1] = outbuf[count * 2];
     break;
    case A52_CHANNEL:
     if (dualmono_channel == 1)
     {
      outbuf[count * 2] = float2int (sample[count + lfe_offset]);
      outbuf[count * 2 + 1] = outbuf[count * 2];
     }
     else
     {
      outbuf[count * 2] = float2int (sample[count + 256 + lfe_offset]);
      outbuf[count * 2 + 1] = outbuf[count * 2];
     }
     break;
    case A52_STEREO:
    case A52_DOLBY:
    case A52_2F1R:
    case A52_2F2R:
     outbuf[count * 2] = float2int (sample[count + lfe_offset]);
     outbuf[count * 2 + 1] = float2int (sample[count + 256 + lfe_offset]);
     break;
    case A52_3F:
    case A52_3F1R:
    case A52_3F2R:
     outbuf[count * 2] = float2int (sample[count + lfe_offset]);
     outbuf[count * 2 + 1] = float2int (sample[count + 512 + lfe_offset]);
     break;
   }
  else if (output_type == A52_2F2R)
   switch (sample_type & A52_CHANNEL_MASK)
   {
    case A52_MONO:
    case A52_CHANNEL1:
    case A52_CHANNEL2:
     outbuf[count * 4] = float2int (sample[count + lfe_offset]);
     outbuf[count * 4 + 1] = outbuf[count * 4];
     if (upmix_stereo == TRUE)
     {
      outbuf[count * 4 + 2] = outbuf[count * 4];
      outbuf[count * 4 + 3] = outbuf[count * 4];
     }
     else
     {
      outbuf[count * 4 + 2] = 0;
      outbuf[count * 4 + 3] = 0;
     }
     break;
    case A52_CHANNEL:
     if (dualmono_channel == 1)
     {
      outbuf[count * 4] = float2int (sample[count + lfe_offset]);
      outbuf[count * 4 + 1] = outbuf[count * 4];
      if (upmix_stereo == TRUE)
      {
       outbuf[count * 4 + 2] = outbuf[count * 4];
       outbuf[count * 4 + 3] = outbuf[count * 4];
      }
      else
      {
       outbuf[count * 4 + 2] = 0;
       outbuf[count * 4 + 3] = 0;
      }
     }
     else
     {
      outbuf[count * 4] = float2int (sample[count + 256 + lfe_offset]);
      outbuf[count * 4 + 1] = outbuf[count * 4];
      if (upmix_stereo == TRUE)
      {
       outbuf[count * 4 + 2] = outbuf[count * 4];
       outbuf[count * 4 + 3] = outbuf[count * 4];
      }
      else
      {
       outbuf[count * 4 + 2] = 0;
       outbuf[count * 4 + 3] = 0;
      }
     }
     break;
    case A52_STEREO:
    case A52_DOLBY:
     outbuf[count * 4] = float2int (sample[count + lfe_offset]);
     outbuf[count * 4 + 1] = float2int (sample[count + 256 + lfe_offset]);
     if (upmix_stereo == TRUE)
     {
      outbuf[count * 4 + 2] = outbuf[count * 4];
      outbuf[count * 4 + 3] = outbuf[count * 4 + 1];
     }
     else
     {
      outbuf[count * 4 + 2] = 0;
      outbuf[count * 4 + 3] = 0;
     }
     break;
    case A52_3F:
     outbuf[count * 4] = float2int (sample[count + lfe_offset]);
     outbuf[count * 4 + 1] = float2int (sample[count + 512 + lfe_offset]);
     if (upmix_stereo == TRUE)
     {
      outbuf[count * 4 + 2] = outbuf[count * 4];
      outbuf[count * 4 + 3] = outbuf[count * 4 + 1];
     }
     else
     {
      outbuf[count * 4 + 2] = 0;
      outbuf[count * 4 + 3] = 0;
     }
     break;
    case A52_2F1R:
     outbuf[count * 4] = float2int (sample[count + lfe_offset]);
     outbuf[count * 4 + 1] = float2int (sample[count + 256 + lfe_offset]);
     outbuf[count * 4 + 2] = float2int (sample[count + 512 + lfe_offset]);
     outbuf[count * 4 + 3] = outbuf[count * 4 + 2];
     break;
    case A52_2F2R:
     outbuf[count * 4] = float2int (sample[count + lfe_offset]);
     outbuf[count * 4 + 1] = float2int (sample[count + 256 + lfe_offset]);
     outbuf[count * 4 + 2] = float2int (sample[count + 512 + lfe_offset]);
     outbuf[count * 4 + 3] = float2int (sample[count + 768 + lfe_offset]);
     break;
    case A52_3F1R:
     outbuf[count * 4] = float2int (sample[count + lfe_offset]);
     outbuf[count * 4 + 1] = float2int (sample[count + 512 + lfe_offset]);
     outbuf[count * 4 + 2] = float2int (sample[count + 768 + lfe_offset]);
     outbuf[count * 4 + 3] = outbuf[count * 4 + 2];
     break;
    case A52_3F2R:
     outbuf[count * 4] = float2int (sample[count + lfe_offset]);
     outbuf[count * 4 + 1] = float2int (sample[count + 512 + lfe_offset]);
     outbuf[count * 4 + 2] = float2int (sample[count + 768 + lfe_offset]);
     outbuf[count * 4 + 3] = float2int (sample[count + 1024 + lfe_offset]);
     break;
   }
  else if (output_type == A52_3F2R)
   switch (sample_type & A52_CHANNEL_MASK)
   {
    case A52_MONO:
    case A52_CHANNEL1:
    case A52_CHANNEL2:
     outbuf[count * 5] = float2int (sample[count + lfe_offset]);
     outbuf[count * 5 + 1] = outbuf[count * 5];
     if (upmix_stereo == TRUE)
     {
      outbuf[count * 5 + 2] = outbuf[count * 5];
      outbuf[count * 5 + 3] = outbuf[count * 5];
     }
     else
     {
      outbuf[count * 5 + 2] = 0;
      outbuf[count * 5 + 3] = 0;
     }
     outbuf[count * 5 + 4] = outbuf[count * 5];
     break;
    case A52_CHANNEL:
     if (dualmono_channel == 1)
     {
      outbuf[count * 5] = float2int (sample[count + lfe_offset]);
      outbuf[count * 5 + 1] = outbuf[count * 5];
      if (upmix_stereo == TRUE)
      {
       outbuf[count * 5 + 2] = outbuf[count * 5];
       outbuf[count * 5 + 3] = outbuf[count * 5];
      }
      else
      {
       outbuf[count * 5 + 2] = 0;
       outbuf[count * 5 + 3] = 0;
      }
     }
     else
     {
      outbuf[count * 5] = float2int (sample[count + 256 + lfe_offset]);
      outbuf[count * 5 + 1] = outbuf[count * 5];
      if (upmix_stereo == TRUE)
      {
       outbuf[count * 5 + 2] = outbuf[count * 5];
       outbuf[count * 5 + 3] = outbuf[count * 5];
      }
      else
      {
       outbuf[count * 5 + 2] = 0;
       outbuf[count * 5 + 3] = 0;
      }
     }
     break;
    case A52_STEREO:
    case A52_DOLBY:
     outbuf[count * 5] = float2int (sample[count + lfe_offset]);
     outbuf[count * 5 + 1] = float2int (sample[count + 256 + lfe_offset]);
     if (upmix_stereo == TRUE)
     {
      outbuf[count * 5 + 2] = outbuf[count * 5];
      outbuf[count * 5 + 3] = outbuf[count * 5 + 1];
     }
     else
     {
      outbuf[count * 5 + 2] = 0;
      outbuf[count * 5 + 3] = 0;
     }
     outbuf[count * 5 + 4] = 0;
     break;
    case A52_3F:
     outbuf[count * 5] = float2int (sample[count + lfe_offset]);
     outbuf[count * 5 + 1] = float2int (sample[count + 512 + lfe_offset]);
     if (upmix_stereo == TRUE)
     {
      outbuf[count * 5 + 2] = outbuf[count * 5];
      outbuf[count * 5 + 3] = outbuf[count * 5 + 1];
     }
     else
     {
      outbuf[count * 5 + 2] = 0;
      outbuf[count * 5 + 3] = 0;
     }
     outbuf[count * 5 + 4] = float2int (sample[count + 256 + lfe_offset]);
     break;
    case A52_2F1R:
     outbuf[count * 5] = float2int (sample[count + lfe_offset]);
     outbuf[count * 5 + 1] = float2int (sample[count + 256 + lfe_offset]);
     outbuf[count * 5 + 2] = float2int (sample[count + 512 + lfe_offset]);
     outbuf[count * 5 + 3] = outbuf[count * 5 + 2];
     outbuf[count * 5 + 4] = 0;
     break;
    case A52_2F2R:
     outbuf[count * 5] = float2int (sample[count + lfe_offset]);
     outbuf[count * 5 + 1] = float2int (sample[count + 256 + lfe_offset]);
     outbuf[count * 5 + 2] = float2int (sample[count + 512 + lfe_offset]);
     outbuf[count * 5 + 3] = float2int (sample[count + 768 + lfe_offset]);
     outbuf[count * 5 + 4] = 0;
     break;
    case A52_3F1R:
     outbuf[count * 5] = float2int (sample[count + lfe_offset]);
     outbuf[count * 5 + 1] = float2int (sample[count + 512 + lfe_offset]);
     outbuf[count * 5 + 2] = float2int (sample[count + 768 + lfe_offset]);
     outbuf[count * 5 + 3] = outbuf[count * 5 + 2];
     outbuf[count * 5 + 4] = float2int (sample[count + 256 + lfe_offset]);
     break;
    case A52_3F2R:
     outbuf[count * 5] = float2int (sample[count + lfe_offset]);
     outbuf[count * 5 + 1] = float2int (sample[count + 512 + lfe_offset]);
     outbuf[count * 5 + 2] = float2int (sample[count + 768 + lfe_offset]);
     outbuf[count * 5 + 3] = float2int (sample[count + 1024 + lfe_offset]);
     outbuf[count * 5 + 4] = float2int (sample[count + 256 + lfe_offset]);
     break;
   }
  else if (output_type == (A52_3F2R | A52_LFE))
   switch (sample_type & A52_CHANNEL_MASK)
   {
    case A52_MONO:
    case A52_CHANNEL1:
    case A52_CHANNEL2:
     outbuf[count * 6] = float2int (sample[count + lfe_offset]);
     outbuf[count * 6 + 1] = outbuf[count * 6];
     if (upmix_stereo == TRUE)
     {
      outbuf[count * 6 + 2] = outbuf[count * 6];
      outbuf[count * 6 + 3] = outbuf[count * 6];
     }
     else
     {
      outbuf[count * 6 + 2] = 0;
      outbuf[count * 6 + 3] = 0;
     }
     outbuf[count * 6 + 4] = outbuf[count * 6];
     if (lfe_offset)
      outbuf[count * 6 + 5] = float2int (sample[count]);
     else
      outbuf[count * 6 + 5] = 0;
     break;
    case A52_CHANNEL:
     if (dualmono_channel == 1)
     {
      outbuf[count * 6] = float2int (sample[count + lfe_offset]);
      outbuf[count * 6 + 1] = outbuf[count * 6];
      if (upmix_stereo == TRUE)
      {
       outbuf[count * 6 + 2] = outbuf[count * 6];
       outbuf[count * 6 + 3] = outbuf[count * 6];
      }
      else
      {
       outbuf[count * 6 + 2] = 0;
       outbuf[count * 6 + 3] = 0;
      }
     }
     else
     {
      outbuf[count * 6] = float2int (sample[count + 256 + lfe_offset]);
      outbuf[count * 6 + 1] = outbuf[count * 6];
      if (upmix_stereo == TRUE)
      {
       outbuf[count * 6 + 2] = outbuf[count * 6];
       outbuf[count * 6 + 3] = outbuf[count * 6];
      }
      else
      {
       outbuf[count * 6 + 2] = 0;
       outbuf[count * 6 + 3] = 0;
      }
     }
     break;
    case A52_STEREO:
    case A52_DOLBY:
     outbuf[count * 6] = float2int (sample[count + lfe_offset]);
     outbuf[count * 6 + 1] = float2int (sample[count + 256 + lfe_offset]);
     if (upmix_stereo == TRUE)
     {
      outbuf[count * 6 + 2] = outbuf[count * 6];
      outbuf[count * 6 + 3] = outbuf[count * 6 + 1];
     }
     else
     {
      outbuf[count * 6 + 2] = 0;
      outbuf[count * 6 + 3] = 0;
     }
     outbuf[count * 6 + 4] = 0;
     if (lfe_offset)
      outbuf[count * 6 + 5] = float2int (sample[count]);
     else
      outbuf[count * 6 + 5] = 0;
     break;
    case A52_3F:
     outbuf[count * 6] = float2int (sample[count + lfe_offset]);
     outbuf[count * 6 + 1] = float2int (sample[count + 512 + lfe_offset]);
     if (upmix_stereo == TRUE)
     {
      outbuf[count * 6 + 2] = outbuf[count * 6];
      outbuf[count * 6 + 3] = outbuf[count * 6 + 1];
     }
     else
     {
      outbuf[count * 6 + 2] = 0;
      outbuf[count * 6 + 3] = 0;
     }
     outbuf[count * 6 + 4] = float2int (sample[count + 256 + lfe_offset]);
     if (lfe_offset)
      outbuf[count * 6 + 5] = float2int (sample[count]);
     else
      outbuf[count * 6 + 5] = 0;
     break;
    case A52_2F1R:
     outbuf[count * 6] = float2int (sample[count + lfe_offset]);
     outbuf[count * 6 + 1] = float2int (sample[count + 256 + lfe_offset]);
     outbuf[count * 6 + 2] = float2int (sample[count + 512 + lfe_offset]);
     outbuf[count * 6 + 3] = outbuf[count * 6 + 2];
     outbuf[count * 6 + 4] = 0;
     if (lfe_offset)
      outbuf[count * 6 + 5] = float2int (sample[count]);
     else
      outbuf[count * 6 + 5] = 0;
     break;
    case A52_2F2R:
     outbuf[count * 6] = float2int (sample[count + lfe_offset]);
     outbuf[count * 6 + 1] = float2int (sample[count + 256 + lfe_offset]);
     outbuf[count * 6 + 2] = float2int (sample[count + 512 + lfe_offset]);
     outbuf[count * 6 + 3] = float2int (sample[count + 768 + lfe_offset]);
     outbuf[count * 6 + 4] = 0;
     if (lfe_offset)
      outbuf[count * 6 + 5] = float2int (sample[count]);
     else
      outbuf[count * 6 + 5] = 0;
     break;
    case A52_3F1R:
     outbuf[count * 6] = float2int (sample[count + lfe_offset]);
     outbuf[count * 6 + 1] = float2int (sample[count + 512 + lfe_offset]);
     outbuf[count * 6 + 2] = float2int (sample[count + 768 + lfe_offset]);
     outbuf[count * 6 + 3] = outbuf[count * 6 + 2];
     outbuf[count * 6 + 4] = float2int (sample[count + 256 + lfe_offset]);
     if (lfe_offset)
      outbuf[count * 6 + 5] = float2int (sample[count]);
     else
      outbuf[count * 6 + 5] = 0;
     break;
    case A52_3F2R:
     outbuf[count * 6] = float2int (sample[count + lfe_offset]);
     outbuf[count * 6 + 1] = float2int (sample[count + 512 + lfe_offset]);
     outbuf[count * 6 + 2] = float2int (sample[count + 768 + lfe_offset]);
     outbuf[count * 6 + 3] = float2int (sample[count + 1024 + lfe_offset]);
     outbuf[count * 6 + 4] = float2int (sample[count + 256 + lfe_offset]);
     if (lfe_offset)
      outbuf[count * 6 + 5] = float2int (sample[count]);
     else
      outbuf[count * 6 + 5] = 0;
     break;
   }
  else if (output_type == A52_DOLBY)
   switch (sample_type & A52_CHANNEL_MASK)
   {
    case A52_CHANNEL1:
    case A52_CHANNEL2:
     outbuf[count * 2] = float2int (sample[count + lfe_offset]);
     outbuf[count * 2 + 1] = outbuf[count * 2];
     break;
    case A52_CHANNEL:
     if (dualmono_channel == 1)
     {
      outbuf[count * 2] = float2int (sample[count + lfe_offset]);
      outbuf[count * 2 + 1] = outbuf[count * 2];
     }
     else
     {
      outbuf[count * 2] = float2int (sample[count + 256 + lfe_offset]);
      outbuf[count * 2 + 1] = outbuf[count * 2];
     }
     break;
    case A52_STEREO:
    case A52_DOLBY:
     outbuf[count * 2] = float2int (sample[count + lfe_offset]);
     outbuf[count * 2 + 1] = float2int (sample[count + 256 + lfe_offset]);
     break;
   }
}

int16_t
float2int (float sample)
{
 if (sample > 32767.0)
  return 32767;
 else if (sample < -32767.0)
  return -32767;
 else
  return (int16_t) sample;
}

static void
play_a52 (char *filename)
{
  char *temp;

  if (a52_run)
    return;

  in_file = vfs_fopen (filename, "r");

  if (!in_file)
    return;

  if (synchronise_a52(in_file, &flags, &sample_rate,
                      &bit_rate, &frame_size) == -1)
  {
    vfs_fclose(in_file);
    return;
  }

  output_type = requested_output;
  
  if ((output_type & A52_STEREO) || (output_type & A52_DOLBY))
  {
   output_nch = a52_ip.output->open_audio (FMT_S16_NE, sample_rate, 2);
   if (output_nch == 0)
    {
      vfs_fclose (in_file);
      return;
    }
  }

    if (output_type != A52_DOLBY)
     output_type = A52_STEREO;

  /* don't ask. -nenolod */
  if (output_nch == 1)
   output_nch = 2;

  // Get song title

  temp = strrchr (filename, '/');
  if (!temp)
    temp = filename;
  else
    temp++;
  name = malloc (strlen (temp) + 1);
  strcpy (name, temp);
  *strrchr (name, '.') = '\0';

  // Get song length

  vfs_fseek (in_file, 0, SEEK_END);

  /* If we can get the file length, we divide it by byterate (not bitrate)
     and multiply by 1000 to get time in milliseconds.
     If we can't get file length, we'll set it to -1

     length (the variable) : file length
     length (passed to set_info) : milliseconds
     bit_rate : bits/second
     sample_rate : sample/second */

  if ((length = vfs_ftell (in_file)) != -1)
    a52_ip.set_info (name, length / (bit_rate / 8 / 1000), 
                     bit_rate, sample_rate, 2);
  else
    a52_ip.set_info (name, -1, bit_rate, sample_rate, 2);
 
  free(name);

  // Return to start of file.

  vfs_fseek (in_file, 0, SEEK_SET);

  // Mutex will be used in play_loop.

  pthread_mutex_destroy(&infile_lock);
  pthread_mutex_init(&infile_lock, NULL);

  /* Ensure that play_loop has written to the buffer before exiting.
     See play_loop for further details. */

  sem_init(&play_loop_signal, 0, 0);
  pthread_create (&decode_thread, NULL, play_loop, NULL);
  sem_wait(&play_loop_signal);
  sem_destroy(&play_loop_signal);

  return;
}

static void
stop_a52 (void)
{
  if (a52_run)
    {
      a52_run = 0;
      pthread_join (decode_thread, NULL);
      a52_ip.output->close_audio ();
    }
}

static int
get_time_a52 (void)
{
  /* lastset_time is set when playing starts and
     when seeking. We can't use it directly because
     it's based on file position for reading. Due to
     buffering, the file position is always ahead of
     the play position.

     Instead, we set lastset_time when playing and
     seeking, and use the output_time() as an offset. */

  a52_ip.output->buffer_free();

  if ((a52_run && a52_not_eof) || a52_ip.output->buffer_playing ())
      return lastset_time + a52_ip.output->output_time ();

  return -1;
}

static void
pause_a52 (short paused)
{
  a52_ip.output->pause (paused);
}

static void
get_song_info_a52 (gchar * filename, gchar ** title, gint * info_length)
{
  int l_flags, l_sample_rate, l_bit_rate, l_frame_size;
  VFSFile *temp_file;
  char *temp_name, *temp;

  // Set the song title

  temp = strrchr (filename, '/');
  if (!temp)
    temp = filename;
  else
    temp++;
  temp_name = malloc (strlen (temp) + 1);
  strcpy (temp_name, temp);
  *strrchr (temp_name, '.') = '\0';
  (*title) = temp_name;

  // Determine the song length from filesize.

  temp_file = vfs_fopen (filename, "r");

  if (synchronise_a52(temp_file, &l_flags, &l_sample_rate,
                      &l_bit_rate, &l_frame_size) == -1)
  {
    *info_length = -1;
    return;
  }

  vfs_fseek (temp_file, 0, SEEK_END);

  if ((*info_length = vfs_ftell (temp_file)) == -1)
    {
      // Can't get file position
      *info_length = -1;
      return;
    }

  // Song length in milliseconds

  *info_length = *info_length / (l_bit_rate / 8 / 1000);
}

static void
seek_a52 (gint time)
{
  // time is in seconds.
  
  // Don't seek while play_loop is reading/synching.
  
  pthread_mutex_lock(&infile_lock);
  vfs_fseek(in_file, time * bit_rate / 8, SEEK_SET);
  pthread_mutex_unlock(&infile_lock);

  a52_ip.output->flush (0);
  lastset_time = time * 1000;
}

void
about_a52 (void)
{
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  GtkStyle *style;
  static GtkWidget *dialog, *button, *label, *pixmapwid;

  if (dialog)
    return;

  dialog = gtk_dialog_new ();
  gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
		      GTK_SIGNAL_FUNC (gtk_widget_destroyed), &dialog);
  gtk_window_set_title (GTK_WINDOW (dialog), "About xmms-a52dec 1.0");
  gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, FALSE, FALSE);
  gtk_container_border_width (GTK_CONTAINER (dialog), 5);
  label = gtk_label_new ("\nXMMS-AC3DEC VERSION 1.0\n\n \
An A52/AC3 decoder plugin for xmms based on the a52dec package.\n \
\nCoded by Cort <ccwee@cyberway.com.sg>\nEnhanced by David Weisgerber <tnt@md.2y.net>");

  /* now for the pixmap from gdk */
  style = gtk_widget_get_style (dialog);
  pixmap = gdk_pixmap_create_from_xpm_d (dialog->window, &mask,
					 &style->bg[GTK_STATE_NORMAL],
					 (gchar **) a52dec_xpm);
  /* a pixmap widget to contain the pixmap */
  pixmapwid = gtk_pixmap_new (pixmap, NULL);
  gtk_widget_show (pixmapwid);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), pixmapwid, TRUE,
		      TRUE, 0);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), label, TRUE, TRUE,
		      0);
  gtk_widget_show (label);

  button = gtk_button_new_with_label (" Close ");
  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
			     GTK_SIGNAL_FUNC (gtk_widget_destroy),
			     GTK_OBJECT (dialog));
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->action_area), button,
		      FALSE, FALSE, 0);

  gtk_widget_show (button);
  gtk_widget_show (dialog);
  gtk_widget_grab_focus (button);
}

static GtkWidget *drc_scale, *drc_both, *drc_boost, *drc_reduce;
static GtkWidget *effect_surround, *effect_lfe, *effect_upmix;
static GtkWidget *dualmono_channel1, *dualmono_channel2;

void
configure_a52 (void)
{
  static GtkWidget *dialog;
  static GtkWidget *button_cancel, *button_apply, *button_ok;
  static GtkWidget *hbox, *vbox, *vbox2;
  static GtkWidget *effect_vbox, *button_hbox, *dualmono_vbox;
  static GtkWidget *drc_vbox;
  static GtkWidget *range_frame, *effect_frame, *output_frame, *dualmono_frame;
  static GtkTooltips *tooltips;
  
  if (dialog)
    return;
  
  tooltips = gtk_tooltips_new();

  // Create dialog and set window parameters.
  
  dialog = gtk_dialog_new ();
  gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
		      GTK_SIGNAL_FUNC (gtk_widget_destroyed), &dialog);
  gtk_window_set_title (GTK_WINDOW (dialog), "xmms-a52dec Configuration");
  gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, FALSE, FALSE);
  gtk_container_border_width (GTK_CONTAINER (dialog), 5);

  // Prepare all the vbox and hbox.
  
  hbox = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, TRUE, TRUE,
                      0);
  vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
  vbox2 = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 0);

  // Prepare all the frames.
  
  range_frame = gtk_frame_new("Dynamic Range Compression");
  effect_frame = gtk_frame_new("Effect");
  output_frame = gtk_frame_new("Output");
  dualmono_frame = gtk_frame_new("Dual Mono");
  
  // Pack the frames in.
  
  gtk_box_pack_start (GTK_BOX (vbox), range_frame, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), effect_frame, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox2), output_frame, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox2), dualmono_frame, TRUE, TRUE, 0);

  // Create Dynamic Range Compression options.

  drc_vbox = gtk_vbox_new(FALSE, 0);
  drc_scale = gtk_hscale_new (GTK_ADJUSTMENT
                          (gtk_adjustment_new
                           (compression_factor, 0, 1, 0.01, 0.1, 0)));
  gtk_scale_set_digits (GTK_SCALE (drc_scale), 2);
  drc_both = gtk_radio_button_new_with_label (NULL, "Boost and Reduce");
  drc_boost = gtk_radio_button_new_with_label_from_widget (
                 GTK_RADIO_BUTTON(drc_both), "Boost only");
  drc_reduce = gtk_radio_button_new_with_label_from_widget (
                 GTK_RADIO_BUTTON(drc_both), "Reduce only");
  gtk_box_pack_start (GTK_BOX (drc_vbox), drc_scale, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (drc_vbox), drc_both, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (drc_vbox), drc_boost, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (drc_vbox), drc_reduce, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER(range_frame), drc_vbox);

  if (compression_type == DRC_BOTH)
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(drc_both), TRUE);
  else if (compression_type == DRC_BOOST)
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(drc_boost), TRUE);
  else if (compression_type == DRC_REDUCE)
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(drc_reduce), TRUE);


  // Create effect toggle button.

  effect_vbox = gtk_vbox_new(FALSE, 0);
  effect_surround = gtk_check_button_new_with_label("No downmix of surround");
  effect_lfe = gtk_check_button_new_with_label("No downmix of LFE channel");
  effect_upmix = gtk_check_button_new_with_label("Upmix stereo to surround");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(effect_surround),
                               nodownmix_surround);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(effect_lfe), nodownmix_lfe);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(effect_upmix), upmix_stereo);
  gtk_box_pack_start (GTK_BOX (effect_vbox), effect_surround, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (effect_vbox), effect_lfe, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (effect_vbox), effect_upmix, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER(effect_frame), effect_vbox);

  // Create dualmono channel radio button.
  
  dualmono_vbox = gtk_vbox_new(FALSE, 0);
  dualmono_channel1 = gtk_radio_button_new_with_label (NULL, "Play First channel");
  dualmono_channel2 = gtk_radio_button_new_with_label_from_widget (
                      GTK_RADIO_BUTTON(dualmono_channel1), "Play Second channel");

  if (dualmono_channel == 1)
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dualmono_channel1), TRUE);
  else
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dualmono_channel2), TRUE);

  gtk_box_pack_start (GTK_BOX (dualmono_vbox), dualmono_channel1, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (dualmono_vbox), dualmono_channel2, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER(dualmono_frame), dualmono_vbox);
  
  // Create hbuttonbox for buttons.

  button_hbox = gtk_hbutton_box_new();
  
  // Create cancel button.
  
  button_cancel = gtk_button_new_with_label ("Cancel");
  gtk_signal_connect_object (GTK_OBJECT (button_cancel), "clicked",
			     GTK_SIGNAL_FUNC (gtk_widget_destroy),
			     GTK_OBJECT (dialog));
  gtk_box_pack_start(GTK_BOX(button_hbox), button_cancel, TRUE, TRUE, 0);

  // Create apply button.

  button_apply = gtk_button_new_with_label ("Apply");
  gtk_signal_connect_object (GTK_OBJECT (button_apply), "clicked",
			     GTK_SIGNAL_FUNC (configure_apply),
			     GTK_OBJECT (dialog));
  gtk_box_pack_start(GTK_BOX(button_hbox), button_apply, TRUE, TRUE, 0);

  // Create ok button.

  button_ok = gtk_button_new_with_label ("Ok");
  gtk_signal_connect_object (GTK_OBJECT (button_ok), "clicked",
			     GTK_SIGNAL_FUNC (configure_ok),
			     GTK_OBJECT (dialog));
  gtk_box_pack_start(GTK_BOX(button_hbox), button_ok, TRUE, TRUE, 0);
  
  // Pack hbuttonbox into dialog.
  
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->action_area),
                      button_hbox, TRUE, TRUE,0);

  // Set tooltips.
  
  gtk_tooltips_set_tip(GTK_TOOLTIPS (tooltips), drc_scale,
  		       "Dynamic range compression is used to lower the\
 volume of loud sounds and increase the volume of\
 soft sounds.\nSet to 0.00 if you wish to disable this feature.", NULL);

  gtk_tooltips_set_tip(GTK_TOOLTIPS (tooltips), drc_both,
  		       "Boost the volume of soft sounds and reduce the volume\
 of loud sounds.", NULL);

  gtk_tooltips_set_tip(GTK_TOOLTIPS (tooltips), drc_boost,
  		       "Boost the volume of soft sounds, but leave loud sounds\
 unchanged.", NULL);

  gtk_tooltips_set_tip(GTK_TOOLTIPS (tooltips), drc_reduce,
  		       "Reduce the volume of loud sounds, but leave soft sounds\
 unchanged.", NULL);

  gtk_tooltips_set_tip(GTK_TOOLTIPS (tooltips), dualmono_channel1,
  		       "Dualmono tracks contain 2 seperate mono channels. Select\
 this option to play the first channels.", NULL);

  gtk_tooltips_set_tip(GTK_TOOLTIPS (tooltips), dualmono_channel2,
  		       "Dualmono tracks contain 2 seperate mono channels. Select\
 this option to play the second channels.", NULL);

  // Show all of the above.

  gtk_widget_show (hbox);
  gtk_widget_show (vbox);
  gtk_widget_show (vbox2);
  gtk_widget_show (range_frame);
  gtk_widget_show (drc_scale);
  gtk_widget_show (drc_both);
  gtk_widget_show (drc_boost);
  gtk_widget_show (drc_reduce);
  gtk_widget_show (drc_vbox);
  gtk_widget_show (effect_frame);
  gtk_widget_show (effect_surround);
  gtk_widget_show (effect_lfe);
  gtk_widget_show (effect_upmix);
  gtk_widget_show (effect_vbox);
  gtk_widget_show (dualmono_vbox);
  gtk_widget_show (dualmono_channel1);
  gtk_widget_show (dualmono_channel2);
  gtk_widget_show (dualmono_frame);
  gtk_widget_show (button_hbox);
  gtk_widget_show (button_cancel);
  gtk_widget_show (button_apply);
  gtk_widget_show (button_ok);
  gtk_widget_show (dialog);
  gtk_widget_grab_focus (button_ok);
}

static void configure_ok(GtkWidget * w, gpointer data)
{
  // Let configure_apply set and save compression_factor.
  
  configure_apply(NULL, NULL);
  
  // Close the window.
  
  gtk_widget_destroy(w);
}

static void configure_apply(GtkWidget * w, gpointer data)
{
  GtkAdjustment *scale_adj;
  ConfigDb *config;
  
  scale_adj = gtk_range_get_adjustment(GTK_RANGE(drc_scale));
  compression_factor = (gdouble) scale_adj->value;

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(drc_both)) == TRUE)
   compression_type = DRC_BOTH;
  else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(drc_boost)) == TRUE)
   compression_type = DRC_BOOST;
  else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(drc_reduce)) == TRUE)
   compression_type = DRC_REDUCE;

  nodownmix_surround = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(effect_surround));
  nodownmix_lfe = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(effect_lfe));
  upmix_stereo = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(effect_upmix));
  requested_output = A52_STEREO;

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dualmono_channel1)) == TRUE)
   dualmono_channel = 1;
  else
   dualmono_channel = 2;
  
  config = bmp_cfg_db_open();
  bmp_cfg_db_set_double(config, "xmms-a52dec", "compression_factor",
                        compression_factor);
  bmp_cfg_db_set_int(config, "xmms-a52dec", "compression_type",
                     compression_type);
  bmp_cfg_db_set_bool(config, "xmms-a52dec", "nodownmix_surround",
                         nodownmix_surround);
  bmp_cfg_db_set_bool(config, "xmms-a52dec", "nodownmix_lfe",
                         nodownmix_lfe);
  bmp_cfg_db_set_bool(config, "xmms-a52dec", "upmix_stereo",
                         upmix_stereo);
  bmp_cfg_db_set_int(config, "xmms-a52dec", "requested_output",
                     requested_output);
  bmp_cfg_db_close(config);
}

void
info_a52 (char *filename)
{
  static GtkWidget *window;
  static GtkWidget *button_ok;
  static GtkWidget *hbox, *vbox, *label, *label2, *entry, *frame;
  static char *temp_text, *label2_text, *temp, *temp_name;
  VFSFile *temp_file;
  int l_flags, l_sample_rate, l_bit_rate, l_frame_size, l_length;
  int no_window = 0;
  
  if (!window)
   no_window = 1;

  // Create window and set parameters.

  if (no_window)
  {
   window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
   gtk_signal_connect (GTK_OBJECT (window), "destroy",
 		       GTK_SIGNAL_FUNC (gtk_widget_destroyed), &window);
   gtk_window_set_default_size (GTK_WINDOW (window), 485, -1);
   gtk_window_set_policy (GTK_WINDOW (window), FALSE, FALSE, TRUE);
   gtk_container_border_width (GTK_CONTAINER (window), 5);
  }

  temp = strrchr (filename, '/');
  if (!temp)
    temp = filename;
  else
    temp++;
  temp_name = malloc (strlen (temp) + 1);
  strcpy (temp_name, temp);
  *strrchr (temp_name, '.') = '\0';

  sprintf(temp_text, "File Info - %s", temp_name);
  gtk_window_set_title (GTK_WINDOW (window), temp_text);

  free(temp_name);
  free(temp_text);

  // Create vbuttonbox for filename and fileinfo frame.

  if (no_window)
  {
   vbox = gtk_vbox_new(FALSE, 0);

   // Create label with a text entry containing filename.
  
   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 8);
   label = gtk_label_new ("Filename: ");
   gtk_label_set_justify (GTK_LABEL(label), GTK_JUSTIFY_LEFT);
   gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
   entry = gtk_entry_new ();
   gtk_widget_set_usize (entry, 400, -1);
   gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
   gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
  }
  gtk_entry_set_text (GTK_ENTRY(entry), filename);

  // Determine the song length from filesize.

  temp_file = vfs_fopen (filename, "r");

  if (synchronise_a52(temp_file, &l_flags, &l_sample_rate,
                      &l_bit_rate, &l_frame_size) == -1)
  {
    l_length = -1;
    return;
  }

  vfs_fseek (temp_file, 0, SEEK_END);

  if ((l_length = vfs_ftell (temp_file)) == -1)
    {
      // Can't get file position
      l_length = -1;
      return;
    }

  // Determine channel type.
  
  switch (l_flags & A52_CHANNEL_MASK)
  {
  	case A52_CHANNEL:
  	 sprintf(temp_text, "Channel");
         break;
  	case A52_MONO:
  	 sprintf(temp_text, "Mono");
         break;
  	case A52_STEREO:
  	 sprintf(temp_text, "Stereo");
         break;
  	case A52_3F:
  	 sprintf(temp_text, "3 Front");
         break;
  	case A52_2F1R:
  	 sprintf(temp_text, "2 Front 1 Rear");
         break;
  	case A52_3F1R:
  	 sprintf(temp_text, "3 Front 1 Rear");
         break;
  	case A52_2F2R:
  	 sprintf(temp_text, "2 Front 2 Rear");
         break;
  	case A52_3F2R:
  	 sprintf(temp_text, "3 Front 2 Rear");
         break;
  	case A52_CHANNEL1:
  	 sprintf(temp_text, "Channel1");
         break;
  	case A52_CHANNEL2:
  	 sprintf(temp_text, "Channel2");
         break;
  	case A52_DOLBY:
  	 sprintf(temp_text, "Dolby");
         break;
  }

  // Create frame for file info.

  if (no_window)
  {
   frame = gtk_frame_new ("A/52 Info:");
   gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 8);

   // Create label inside frame.

   label2 = gtk_label_new (NULL);
   gtk_container_add(GTK_CONTAINER(frame), label2);
   gtk_misc_set_alignment (GTK_MISC (label2), 0, 0);
   gtk_label_set_justify (GTK_LABEL(label2), GTK_JUSTIFY_LEFT);

  }

  // Set text of label inside frame.

  if (l_flags & A52_LFE)
    sprintf(label2_text, "\n  Bitrate: %d kb/s\n  Samplerate: %d Hz\n  Channel Type: %s\n  Low Frequency Enhancement: Yes\n  %d frames\n  Filesize: %d B\n",
             l_bit_rate / 1000, l_sample_rate, temp_text, l_length / l_frame_size, l_length);
  else
    sprintf(label2_text, "\n  Bitrate: %d kb/s\n  Samplerate: %d Hz\n  Channel Type: %s\n  Low Frequency Enhancement: No\n  %d frames\n  Filesize: %d B\n",
             l_bit_rate / 1000, l_sample_rate, temp_text, l_length / l_frame_size, l_length);

  gtk_label_set_text (GTK_LABEL(label2), label2_text);

  if (no_window)
  {
   // Create ok button.

   button_ok = gtk_button_new_with_label ("Ok");
   gtk_signal_connect_object (GTK_OBJECT (button_ok), "clicked",
			     GTK_SIGNAL_FUNC (gtk_widget_destroy),
			     GTK_OBJECT (window));
   gtk_widget_set_usize (button_ok, 100, -1);
   gtk_box_pack_start(GTK_BOX (vbox), button_ok,
                      TRUE, FALSE, 8);
  
   // Pack vbuttonbox into window.

   gtk_container_add (GTK_CONTAINER (window), vbox);

   // Show all of the above.

   gtk_widget_show (label2);
   gtk_widget_show (label);
   gtk_widget_show (entry);
   gtk_widget_show (frame);
   gtk_widget_show (hbox);
   gtk_widget_show (button_ok);
   gtk_widget_show (vbox);
   gtk_widget_show (window);
   gtk_widget_grab_focus (button_ok);
  }
}
