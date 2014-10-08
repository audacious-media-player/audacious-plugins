/*
 * Copyright 2003-2006 Chris Morgan <cmorgan@alum.wpi.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* NOTE: All functions that take a jack_driver_t* do NOT lock the device, in order to get a */
/*       jack_driver_t* you must call getDriver() which will pthread_mutex_lock() */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <unistd.h>
#include <inttypes.h>
#include <jack/jack.h>
#include <jack/ringbuffer.h>
#include <pthread.h>
#include <sys/time.h>

#include <glib.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/runtime.h>

#include "bio2jack.h"

using aud::min;
using aud::max;

/* enable/disable TRACING through the JACK_Callback() function */
/* this can sometimes be too much information */
#define TRACE_CALLBACK          0

/* set to 1 for verbose output */
#define VERBOSE_OUTPUT          0

/* set to 1 to enable debug messages */
#define DEBUG_OUTPUT            0

/* set to 1 to enable tracing */
#define TRACE_ENABLE            0

/* set to 1 to enable the function timers */
#define TIMER_ENABLE            0

/* set to 1 to enable tracing of getDriver() and releaseDriver() */
#define TRACE_getReleaseDevice  0

#define DEFAULT_RB_SIZE         4096

#define OUTFILE STDERR_FILENO

#if TIMER_ENABLE
/* This seemingly construct makes timing arbitrary functions really easy
   all you have to do is place a 'TIMER("start\n")' at the beginning and
   a 'TIMER("stop\n")' at the end of any function and this does the rest
   (naturally you can place any printf-compliant text you like in the argument
   along with the associated values). */
//static struct timeval timer_now;
//#define TIMER(format,args...) gettimeofday(&timer_now,0);
//  fprintf(OUTFILE, "%ld.%06ld: %s::%s(%d) " format, timer_now.tv_sec, timer_now.tv_usec, __FILE__, __FUNCTION__, __LINE__, ##args)
#else
#define TIMER(...)
#endif

#if TRACE_ENABLE
#define TRACE AUDDBG
#else
#define TRACE(...)
#endif

#if DEBUG_OUTPUT
#define DEBUG AUDDBG
#else
#define DEBUG(...)
#endif

#if TRACE_CALLBACK
//#define CALLBACK_TRACE(format,args...) fprintf(OUTFILE, "%s::%s(%d) " format, __FILE__, __FUNCTION__, __LINE__,##args)
#else
#define CALLBACK_TRACE(...)
#endif

#define WARN AUDWARN
#define ERR AUDERR

#define MAX_OUTPUT_PORTS  10

#define SAMPLE_FMT_INTEGER 0 /* Regular integer samples */
#define SAMPLE_FMT_PACKED_24B 1 /* 24 bit samples packed to 32 bit values */
#define SAMPLE_FMT_FLOAT 2 /* 32 bit floating point samples */

typedef struct jack_driver_s
{
  bool allocated;                       /* whether or not this device has been allocated */

  int deviceID;                         /* id of this device */
  int clientCtr;                        /* to prevent overlapping client ids */
  long jack_sample_rate;                /* jack samples(frames) per second */

  long client_sample_rate;              /* client samples(frames) per second */

  unsigned long num_output_channels;    /* number of output channels(1 is mono, 2 stereo etc..) */

  unsigned long bits_per_channel;       /* number of bits per channel (8, 16, 24 or 32 supported) */
  int sample_format;                    /* special sample format or SAMPLE_FMT_INTEGER for signed native-endian integers using all bits_per_channel bit  */

  unsigned long bytes_per_output_frame; /* (num_output_channels * bits_per_channel) / 8 */

  unsigned long bytes_per_jack_output_frame;    /* (num_output_channels * bits_per_channel) / 8 */

  long clientBytesInJack;       /* number of INPUT bytes(from the client of bio2jack) we wrote to jack(not necessary the number of bytes we wrote to jack) */
  long jack_buffer_size;        /* size of the buffer jack will pass in to the process callback */

  unsigned long callback_buffer2_size;  /* number of bytes in the buffer allocated for processing data in JACK_Callback */
  char *callback_buffer2;

  unsigned long rw_buffer1_size;        /* number of bytes in the buffer allocated for processing data in JACK_(Read|Write) */
  char *rw_buffer1;

  struct timeval previousTime;  /* time of last JACK_Callback() write to jack, allows for MS accurate bytes played  */

  unsigned long written_client_bytes;   /* input bytes we wrote to jack, not necessarily actual bytes we wrote to jack due to channel and other conversion */
  unsigned long played_client_bytes;    /* input bytes that jack has played */

  unsigned long client_bytes;   /* total bytes written by the client of bio2jack via JACK_Write() */

  jack_port_t *output_port[MAX_OUTPUT_PORTS];   /* output ports */

  jack_client_t *client;        /* pointer to jack client */

  unsigned long jack_output_port_flags; /* flags to be passed to jack when opening the output ports */

  jack_ringbuffer_t *pPlayPtr;  /* the playback ringbuffer */

  enum status_enum state;       /* one of PLAYING, PAUSED, STOPPED, CLOSED, RESET etc */

  unsigned int volume[MAX_OUTPUT_PORTS];        /* percentage of sample value to preserve, 100 would be no attenuation */

  long position_byte_offset;    /* an offset that we will apply to returned position queries to achieve */
                                /* the position that the user of the driver desires set */

  bool in_use;                  /* true if this device is currently in use */

  pthread_mutex_t mutex;        /* mutex to lock this specific device */

  /* variables used for trying to restart the connection to jack */
  bool jackd_died;              /* true if jackd has died and we should try to restart it */
  struct timeval last_reconnect_attempt;
} jack_driver_t;


static bool init_done = 0;      /* just to prevent clients from calling JACK_Init twice, that would be very bad */

static enum JACK_PORT_CONNECTION_MODE port_connection_mode = CONNECT_ALL;

/* enable/disable code that allows us to close a device without actually closing the jack device */
/* this works around the issue where jack doesn't always close devices by the time the close function call returns */
#define JACK_CLOSE_HACK 1

typedef jack_default_audio_sample_t sample_t;
typedef jack_nframes_t nframes_t;

/* allocate devices for output */
/* if you increase this past 10, you might want to update 'out_client_name = ... ' in JACK_OpenDevice */
#define MAX_OUTDEVICES 10
static jack_driver_t outDev[MAX_OUTDEVICES];

static pthread_mutex_t device_mutex = PTHREAD_MUTEX_INITIALIZER;        /* this is to lock the entire outDev array
                                                                           to make managing it in a threaded
                                                                           environment sane */

#if JACK_CLOSE_HACK
static void JACK_CloseDevice(jack_driver_t * drv, bool close_client);
#else
static void JACK_CloseDevice(jack_driver_t * drv);
#endif


/* Prototypes */
static int JACK_OpenDevice(jack_driver_t * drv);
static unsigned long JACK_GetBytesFreeSpaceFromDriver(jack_driver_t * drv);
static void JACK_ResetFromDriver(jack_driver_t * drv);
static long JACK_GetPositionFromDriver(jack_driver_t * drv);
static void JACK_CleanupDriver(jack_driver_t * drv);


/* Return the difference between two timeval structures in terms of milliseconds */
long
TimeValDifference(struct timeval *start, struct timeval *end)
{
  double long ms;               /* milliseconds value */

  ms = end->tv_sec - start->tv_sec;     /* compute seconds difference */
  ms *= (double) 1000;          /* convert to milliseconds */

  ms += (double) (end->tv_usec - start->tv_usec) / (double) 1000;       /* add on microseconds difference */

  return (long) ms;
}

/* get a device and lock the devices mutex */
/* */
/* also attempt to reconnect to jack since this function is called from */
/* most other bio2jack functions it provides a good point to attempt reconnection */
/* */
/* Ok, I know this looks complicated and it kind of is. The point is that when you're
   trying to trace mutexes it's more important to know *who* called us than just that
   we were called.  This uses from pre-processor trickery so that the fprintf is actually
   placed in the function making the getDriver call.  Thus, the __FUNCTION__ and __LINE__
   macros will actually reference our caller, rather than getDriver.  The reason the
   fprintf call is passes as a parameter is because this macro has to still return a
   jack_driver_t* and we want to log both before *and* after the getDriver call for
   easier detection of blocked calls.
 */
#if TRACE_getReleaseDevice
#define getDriver(x) _getDriver(x,fprintf(OUTFILE, "%s::%s(%d) getting driver %d\n", __FILE__, __FUNCTION__, __LINE__,x)); TRACE("got driver %d\n",x);
jack_driver_t *
_getDriver(int deviceID, int ignored)
{
  fflush(OUTFILE);
#else
jack_driver_t *
getDriver(int deviceID)
{
#endif
  jack_driver_t *drv = &outDev[deviceID];

  pthread_mutex_lock(&drv->mutex);

  /* should we try to restart the jack server? */
  if(drv->jackd_died && drv->client == 0)
  {
    struct timeval now;
    gettimeofday(&now, 0);

    /* wait 250ms before trying again */
    if(TimeValDifference(&drv->last_reconnect_attempt, &now) >= 250)
    {
      JACK_OpenDevice(drv);
      drv->last_reconnect_attempt = now;
    }
  }

  return drv;
}

#if TRACE_getReleaseDevice
#define tryGetDriver(x) _tryGetDriver(x,fprintf(OUTFILE, "%s::%s(%d) trying to get driver %d\n", __FILE__, __FUNCTION__, __LINE__,x)); TRACE("got driver %d\n",x);
jack_driver_t *
_tryGetDriver(int deviceID, int ignored)
{
  fflush(OUTFILE);
#else
jack_driver_t *
tryGetDriver(int deviceID)
{
#endif
  jack_driver_t *drv = &outDev[deviceID];

  int err;
  if((err = pthread_mutex_trylock(&drv->mutex)) == 0)
    return drv;

  if(err == EBUSY)
  {
    TRACE("driver %d is busy\n",deviceID);
    return 0;
  }

  ERR("lock returned an error\n");
  return 0;
}


/* release a device's mutex */
/* */
/* This macro is similar to the one for getDriver above, only simpler since we only
   really need to know when the lock was release for the sake of debugging.
*/
#if TRACE_getReleaseDevice
#define releaseDriver(x) TRACE("releasing driver %d\n",x->deviceID); _releaseDriver(x);
void
_releaseDriver(jack_driver_t * drv)
#else
void
releaseDriver(jack_driver_t * drv)
#endif
{
  /*
     #if TRACE_getReleaseDevice
     TRACE("deviceID == %d\n", drv->deviceID);
     #endif
   */
  pthread_mutex_unlock(&drv->mutex);
}


/* Return a string corresponding to the input state */
const char *
DEBUGSTATE(enum status_enum state)
{
  if(state == PLAYING)
    return "PLAYING";
  else if(state == PAUSED)
    return "PAUSED";
  else if(state == STOPPED)
    return "STOPPED";
  else if(state == CLOSED)
    return "CLOSED";
  else if(state == RESET)
    return "RESET";
  else
    return "unknown state";
}

#define SAMPLE_MAX_24BIT  8388608.0f
#define SAMPLE_MAX_16BIT  32768.0f
#define SAMPLE_MAX_8BIT   255.0f

/* floating point volume routine */
/* volume should be a value between 0.0 and 1.0 */
static void
float_volume_effect(sample_t * buf, unsigned long nsamples, float volume,
                    int skip)
{
  if(volume < 0)
    volume = 0;
  if(volume > 1.0)
    volume = 1.0;

  while(nsamples--)
  {
    *buf = (*buf) * volume;
    buf += skip;
  }
}

/* place one channel into a multi-channel stream */
static inline void
mux(sample_t * dst, sample_t * src, unsigned long nsamples,
    unsigned long dst_skip)
{
  /* ALERT: signed sign-extension portability !!! */
  while(nsamples--)
  {
    *dst = *src;
    dst += dst_skip;
    src++;
  }
}

/* pull one channel out of a multi-channel stream */
static void
demux(sample_t * dst, sample_t * src, unsigned long nsamples,
      unsigned long src_skip)
{
  /* ALERT: signed sign-extension portability !!! */
  while(nsamples--)
  {
    *dst = *src;
    dst++;
    src += src_skip;
  }
}

/* copy floating point samples */
static inline void
sample_move_float_float(sample_t * dst, float * src, unsigned long nsamples)
{
  unsigned long i;
  for(i = 0; i < nsamples; i++)
    dst[i] = (sample_t) (src[i]);
}

/* convert from 32 bit samples to floating point */
static inline void
sample_move_int32_float(sample_t * dst, int32_t * src, unsigned long nsamples)
{
  unsigned long i;
  for(i = 0; i < nsamples; i++)
    dst[i] = (sample_t) (src[i] >> 8) / SAMPLE_MAX_24BIT;
}

/* convert from 24 bit samples packed into 32 bits to floating point */
static inline void
sample_move_int24_float(sample_t * dst, int32_t * src, unsigned long nsamples)
{
  unsigned long i;
  for(i = 0; i < nsamples; i++)
    dst[i] = (sample_t) (src[i]) / SAMPLE_MAX_24BIT;
}

/* convert from 16 bit to floating point */
static inline void
sample_move_short_float(sample_t * dst, short *src, unsigned long nsamples)
{
  /* ALERT: signed sign-extension portability !!! */
  unsigned long i;
  for(i = 0; i < nsamples; i++)
    dst[i] = (sample_t) (src[i]) / SAMPLE_MAX_16BIT;
}

/* convert from floating point to 16 bit */
static inline void
sample_move_float_short(short *dst, sample_t * src, unsigned long nsamples)
{
  /* ALERT: signed sign-extension portability !!! */
  unsigned long i;
  for(i = 0; i < nsamples; i++)
    dst[i] = (short) ((src[i]) * SAMPLE_MAX_16BIT);
}

/* convert from 8 bit to floating point */
static inline void
sample_move_char_float(sample_t * dst, unsigned char *src, unsigned long nsamples)
{
  /* ALERT: signed sign-extension portability !!! */
  unsigned long i;
  for(i = 0; i < nsamples; i++)
    dst[i] = (sample_t) (src[i]) / SAMPLE_MAX_8BIT;
}

/* convert from floating point to 8 bit */
static inline void
sample_move_float_char(unsigned char *dst, sample_t * src, unsigned long nsamples)
{
  /* ALERT: signed sign-extension portability !!! */
  unsigned long i;
  for(i = 0; i < nsamples; i++)
    dst[i] = (char) ((src[i]) * SAMPLE_MAX_8BIT);
}

/* fill dst buffer with nsamples worth of silence */
static void inline
sample_silence_float(sample_t * dst, unsigned long nsamples)
{
  /* ALERT: signed sign-extension portability !!! */
  while(nsamples--)
  {
    *dst = 0;
    dst++;
  }
}

static bool inline
ensure_buffer_size(char **buffer, unsigned long *cur_size,
                   unsigned long needed_size)
{
  DEBUG("current size = %lu, needed size = %lu\n", *cur_size, needed_size);
  if(*cur_size >= needed_size)
    return true;
  DEBUG("reallocing\n");
  char *tmp = (char *) realloc(*buffer, needed_size);
  if(tmp)
  {
    *cur_size = needed_size;
    *buffer = tmp;
    return true;
  }
  DEBUG("reallocing failed\n");
  return false;
}

/******************************************************************
 *    JACK_callback
 *
 * every time the jack server wants something from us it calls this
 * function, so we either deliver it some sound to play or deliver it nothing
 * to play
 */
static int
JACK_callback(nframes_t nframes, void *arg)
{
  jack_driver_t *drv = (jack_driver_t *) arg;

  TIMER("start\n");
  gettimeofday(&drv->previousTime, 0);  /* record the current time */

  CALLBACK_TRACE("nframes %ld, sizeof(sample_t) == %d\n", (long) nframes,
                 sizeof(sample_t));

  if(!drv->client)
    ERR("client is closed, this is weird...\n");

  sample_t *out_buffer[MAX_OUTPUT_PORTS];
  /* retrieve the buffers for the output ports */
  for(unsigned i = 0; i < drv->num_output_channels; i++)
    out_buffer[i] = (sample_t *) jack_port_get_buffer(drv->output_port[i], nframes);

  /* handle playing state */
  if(drv->state == PLAYING)
  {
    /* handle playback data, if any */
    if(drv->num_output_channels > 0)
    {
      unsigned long jackFramesAvailable = nframes;      /* frames we have left to write to jack */
      unsigned long numFramesToWrite;   /* num frames we are writing */
      size_t inputBytesAvailable = jack_ringbuffer_read_space(drv->pPlayPtr);
      unsigned long inputFramesAvailable;       /* frames we have available */

      inputFramesAvailable = inputBytesAvailable / drv->bytes_per_jack_output_frame;
      size_t jackBytesAvailable = jackFramesAvailable * drv->bytes_per_jack_output_frame;

      long read = 0;

      CALLBACK_TRACE("playing... jackFramesAvailable = %ld inputFramesAvailable = %ld\n",
         jackFramesAvailable, inputFramesAvailable);

#if JACK_CLOSE_HACK
      if(drv->in_use == false)
      {
        /* output silence if nothing is being outputted */
        for(unsigned i = 0; i < drv->num_output_channels; i++)
          sample_silence_float(out_buffer[i], nframes);

        return -1;
      }
#endif

      /* make sure our buffer is large enough for the data we are writing */
      /* ie. callback_buffer2_size < (bytes we already wrote + bytes we are going to write in this loop) */
      if(!ensure_buffer_size
         (&drv->callback_buffer2, &drv->callback_buffer2_size,
          jackBytesAvailable))
      {
        ERR("allocated %lu bytes, need %lu bytes\n",
            drv->callback_buffer2_size, (unsigned long)jackBytesAvailable);
        return -1;
      }

        /* read as much data from the buffer as is available */
        if(jackFramesAvailable && inputBytesAvailable > 0)
        {
          /* write as many bytes as we have space remaining, or as much as we have data to write */
          numFramesToWrite = min(jackFramesAvailable, inputFramesAvailable);
          jack_ringbuffer_read(drv->pPlayPtr, drv->callback_buffer2,
                               jackBytesAvailable);
          /* add on what we wrote */
          read = numFramesToWrite * drv->bytes_per_output_frame;
          jackFramesAvailable -= numFramesToWrite;      /* take away what was written */
        }

      drv->written_client_bytes += read;
      drv->played_client_bytes += drv->clientBytesInJack;       /* move forward by the previous bytes we wrote since those must have finished by now */
      drv->clientBytesInJack = read;    /* record the input bytes we wrote to jack */

      /* see if we still have jackBytesLeft here, if we do that means that we
         ran out of wave data to play and had a buffer underrun, fill in
         the rest of the space with zero bytes so at least there is silence */
      if(jackFramesAvailable)
      {
        WARN("buffer underrun of %ld frames\n", jackFramesAvailable);
        for(unsigned i = 0; i < drv->num_output_channels; i++)
          sample_silence_float(out_buffer[i] +
                               (nframes - jackFramesAvailable),
                               jackFramesAvailable);
      }

          /* apply volume */
          for(unsigned i = 0; i < drv->num_output_channels; i++)
          {
                  float_volume_effect((sample_t *) drv->callback_buffer2 + i, (nframes - jackFramesAvailable),
                                      ((float) drv->volume[i] / 100.0),
                                      drv->num_output_channels);
          }

          /* demux the stream: we skip over the number of samples we have output channels as the channel data */
          /* is encoded like chan1,chan2,chan3,chan1,chan2,chan3... */
          for(unsigned i = 0; i < drv->num_output_channels; i++)
          {
              demux(out_buffer[i],
                    (sample_t *) drv->callback_buffer2 + i,
                    (nframes - jackFramesAvailable), drv->num_output_channels);
          }
    }
  }
  else if(drv->state == PAUSED  ||
          drv->state == STOPPED ||
          drv->state == CLOSED  || drv->state == RESET)
  {
    CALLBACK_TRACE("%s, outputting silence\n", DEBUGSTATE(drv->state));

    /* output silence if nothing is being outputted */
    for(unsigned i = 0; i < drv->num_output_channels; i++)
      sample_silence_float(out_buffer[i], nframes);

    /* if we were told to reset then zero out some variables */
    /* and transition to STOPPED */
    if(drv->state == RESET)
    {
      drv->written_client_bytes = 0;
      drv->played_client_bytes = 0;     /* number of the clients bytes that jack has played */

      drv->client_bytes = 0;    /* bytes that the client wrote to use */

      drv->clientBytesInJack = 0;       /* number of input bytes in jack(not necessary the number of bytes written to jack) */

      drv->position_byte_offset = 0;

      if(drv->pPlayPtr)
        jack_ringbuffer_reset(drv->pPlayPtr);

      drv->state = STOPPED;     /* transition to STOPPED */
    }
  }

  CALLBACK_TRACE("done\n");
  TIMER("finish\n");

  return 0;
}


/******************************************************************
 *             JACK_bufsize
 *
 *             Called whenever the jack server changes the the max number
 *             of frames passed to JACK_callback
 */
static int
JACK_bufsize(nframes_t nframes, void *arg)
{
  jack_driver_t *drv = (jack_driver_t *) arg;
  TRACE("the maximum buffer size is now %lu frames\n", (long) nframes);

  drv->jack_buffer_size = nframes;

  return 0;
}

/******************************************************************
 *		JACK_srate
 */
int
JACK_srate(nframes_t nframes, void *arg)
{
  jack_driver_t *drv = (jack_driver_t *) arg;

  drv->jack_sample_rate = (long) nframes;

  TRACE("the sample rate is now %lu/sec\n", (long) nframes);
  return 0;
}


/******************************************************************
 *		JACK_shutdown
 *
 * if this is called then jack shut down... handle this appropriately */
void
JACK_shutdown(void *arg)
{
  jack_driver_t *drv = (jack_driver_t *) arg;

  TRACE("\n");

  getDriver(drv->deviceID);

  drv->client = 0;              /* reset client */
  drv->jackd_died = true;

  TRACE("jack shutdown, setting client to 0 and jackd_died to true, closing device\n");

#if JACK_CLOSE_HACK
  JACK_CloseDevice(drv, true);
#else
  JACK_CloseDevice(drv);
#endif

  TRACE("trying to reconnect right now\n");
  /* lets see if we can't reestablish the connection */
  if(JACK_OpenDevice(drv) != ERR_SUCCESS)
  {
    ERR("unable to reconnect with jack\n");
  }

  releaseDriver(drv);
}


/******************************************************************
 *		JACK_Error
 *
 * Callback for jack errors
 */
static void
JACK_Error(const char *desc)
{
  ERR("%s\n", desc);
}


/******************************************************************
 *		JACK_OpenDevice
 *
 *  RETURNS: ERR_SUCCESS upon success
 */
static int
JACK_OpenDevice(jack_driver_t * drv)
{
  const char **ports;
  char *our_client_name = 0;
  int failed = 0;

  TRACE("creating jack client and setting up callbacks\n");

#if JACK_CLOSE_HACK
  /* see if this device is already open */
  if(drv->client)
  {
    /* if this device is already in use then it is bad for us to be in here */
    if(drv->in_use)
      return ERR_OPENING_JACK;

    TRACE("using existing client\n");
    drv->in_use = true;
    return ERR_SUCCESS;
  }
#endif

  /* set up an error handler */
  jack_set_error_function(JACK_Error);


  /* build the client name */
  our_client_name = g_strdup_printf("%s_%d_%d%02d", "audacious-jack", getpid(),
                                    drv->deviceID, drv->clientCtr++);

  /* try to become a client of the JACK server */
  TRACE("client name '%s'\n", our_client_name);
  if((drv->client = jack_client_open(our_client_name, JackNoStartServer, nullptr)) == 0)
  {
    /* try once more */
    TRACE("trying once more to jack_client_new");
    if((drv->client = jack_client_open(our_client_name, JackNoStartServer, nullptr)) == 0)
    {
      ERR("jack server not running?\n");
      g_free(our_client_name);
      return ERR_OPENING_JACK;
    }
  }

  g_free(our_client_name);

  TRACE("setting up jack callbacks\n");

  /* JACK server to call `JACK_callback()' whenever
     there is work to be done. */
  jack_set_process_callback(drv->client, JACK_callback, drv);

  /* setup a buffer size callback */
  jack_set_buffer_size_callback(drv->client, JACK_bufsize, drv);

  /* tell the JACK server to call `srate()' whenever
     the sample rate of the system changes. */
  jack_set_sample_rate_callback(drv->client, JACK_srate, drv);

  /* tell the JACK server to call `jack_shutdown()' if
     it ever shuts down, either entirely, or if it
     just decides to stop calling us. */
  jack_on_shutdown(drv->client, JACK_shutdown, drv);

  /* display the current sample rate. once the client is activated
     (see below), you should rely on your own sample rate
     callback (see above) for this value. */
  drv->jack_sample_rate = jack_get_sample_rate(drv->client);
  TRACE("client sample rate: %lu, jack sample rate: %lu\n",
        drv->client_sample_rate, drv->jack_sample_rate);

  drv->jack_buffer_size = jack_get_buffer_size(drv->client);

  /* create the output ports */
  TRACE("creating output ports\n");
  for(unsigned i = 0; i < drv->num_output_channels; i++)
  {
    StringBuf portname = str_printf ("out_%d", i);
    TRACE("port %d is named '%s'\n", i, (const char *) portname);
    /* NOTE: Yes, this is supposed to be JackPortIsOutput since this is an output */
    /* port FROM bio2jack */
    drv->output_port[i] = jack_port_register(drv->client, portname,
                                             JACK_DEFAULT_AUDIO_TYPE,
                                             JackPortIsOutput, 0);
  }

#if JACK_CLOSE_HACK
  drv->in_use = true;
#endif

  /* tell the JACK server that we are ready to roll */
  TRACE("calling jack_activate()\n");
  if(jack_activate(drv->client))
  {
    ERR("cannot activate client\n");
    return ERR_OPENING_JACK;
  }

  /* if we have output channels and the port connection mode isn't CONNECT_NONE */
  /* then we should connect up some ports */
  if((drv->num_output_channels > 0) && (port_connection_mode != CONNECT_NONE))
  {
        TRACE("jack_get_ports() passing in nullptr/nullptr\n");
        ports = jack_get_ports(drv->client, nullptr, nullptr,
                               drv->jack_output_port_flags);

      /* display a trace of the output ports we found */
      unsigned num_ports = 0;
      if(ports)
      {
        for(unsigned i = 0; ports[i]; i++)
        {
          TRACE("ports[%d] = '%s'\n", i, ports[i]);
          num_ports++;
        }
      }

      /* ensure that we found enough ports */
      if(!ports || (num_ports < drv->num_output_channels))
      {
        TRACE("ERR: jack_get_ports() failed to find ports with jack port flags of 0x%lX'\n",
              drv->jack_output_port_flags);
#if JACK_CLOSE_HACK
        JACK_CloseDevice(drv, true);
#else
        JACK_CloseDevice(drv);
#endif
        return ERR_PORT_NOT_FOUND;
      }

      /* connect a port for each output channel. Note: you can't do this before
         the client is activated (this may change in the future). */
      for(unsigned i = 0; i < drv->num_output_channels; i++)
      {
        TRACE("jack_connect() to port %d('%p')\n", i, drv->output_port[i]);
        if(jack_connect(drv->client, jack_port_name(drv->output_port[i]), ports[i]))
        {
          ERR("cannot connect to output port %d('%s')\n", i, ports[i]);
          failed = 1;
        }
      }

      /* only if we are in CONNECT_ALL mode should we keep connecting ports up beyond */
      /* the minimum number of ports required for each output channel coming into bio2jack */
      if(port_connection_mode == CONNECT_ALL)
      {
          /* It's much cheaper and easier to let JACK do the processing required to
             connect 2 channels to 4 or 4 channels to 2 or any other combinations.
             This effectively eliminates the need for sample_move_d16_d16() */
          if(drv->num_output_channels < num_ports)
          {
              for(unsigned i = drv->num_output_channels; ports[i]; i++)
              {
                  int n = i % drv->num_output_channels;
                  TRACE("jack_connect() to port %d('%p')\n", i, drv->output_port[n]);
                  if(jack_connect(drv->client, jack_port_name(drv->output_port[n]), ports[i]))
                  {
                      // non fatal
                      ERR("cannot connect to output port %d('%s')\n", n, ports[i]);
                  }
              }
          }
          else if(drv->num_output_channels > num_ports)
          {
              for(unsigned i = num_ports; i < drv->num_output_channels; i++)
              {
                  int n = i % num_ports;
                  TRACE("jack_connect() to port %d('%p')\n", i, drv->output_port[n]);
                  if(jack_connect(drv->client, jack_port_name(drv->output_port[i]), ports[n]))
                  {
                      // non fatal
                      ERR("cannot connect to output port %d('%s')\n", i, ports[n]);
                  }
              }
          }
      }

      g_free(ports);              /* free the returned array of ports */
  }                             /* if( drv->num_output_channels > 0 ) */


  /* if something failed we need to shut the client down and return 0 */
  if(failed)
  {
    TRACE("failed, closing and returning error\n");
#if JACK_CLOSE_HACK
    JACK_CloseDevice(drv, true);
#else
    JACK_CloseDevice(drv);
#endif
    return ERR_OPENING_JACK;
  }

  TRACE("success\n");

  drv->jackd_died = false;      /* clear out this flag so we don't keep attempting to restart things */
  drv->state = PLAYING;         /* clients seem to behave much better with this on from the start, especially when recording */

  return ERR_SUCCESS;           /* return success */
}


/******************************************************************
 *		JACK_CloseDevice
 *
 *	Close the connection to the server cleanly.
 *  If close_client is true we close the client for this device instead of
 *    just marking the device as in_use(JACK_CLOSE_HACK only)
 */
#if JACK_CLOSE_HACK
static void
JACK_CloseDevice(jack_driver_t * drv, bool close_client)
#else
static void
JACK_CloseDevice(jack_driver_t * drv)
#endif
{
#if JACK_CLOSE_HACK
  if(close_client)
  {
#endif

    TRACE("closing the jack client thread\n");
    if(drv->client)
    {
      TRACE("after jack_deactivate()\n");
      int errorCode = jack_client_close(drv->client);
      if(errorCode)
        ERR("jack_client_close() failed returning an error code of %d\n",
            errorCode);
    }

    /* reset client */
    drv->client = 0;

    JACK_CleanupDriver(drv);

    JACK_ResetFromDriver(drv);

#if JACK_CLOSE_HACK
  } else
  {
    TRACE("setting in_use to false\n");
    drv->in_use = false;

    if(!drv->client)
    {
      TRACE("critical error, closing a device that has no client\n");
    }
  }
#endif
}




/**************************************/
/* External interface functions below */
/**************************************/

/* Clear out any buffered data, stop playing, zero out some variables */
static void
JACK_ResetFromDriver(jack_driver_t * drv)
{
  TRACE("resetting drv->deviceID(%d)\n", drv->deviceID);

  /* NOTE: we use the RESET state so we don't need to worry about clearing out */
  /* variables that the callback modifies while the callback is running */
  /* we set the state to RESET and the callback clears the variables out for us */
  drv->state = RESET;           /* tell the callback that we are to reset, the callback will transition this to STOPPED */
}

/* Clear out any buffered data, stop playing, zero out some variables */
void
JACK_Reset(int deviceID)
{
  jack_driver_t *drv = getDriver(deviceID);
  TRACE("resetting deviceID(%d)\n", deviceID);
  JACK_ResetFromDriver(drv);
  releaseDriver(drv);
}


/*
 * open the audio device for writing to
 *
 * deviceID is set to the opened device
 * if client is non-zero and in_use is false then just set in_use to true
 *
 * return value is zero upon success, non-zero upon failure
 *
 * if ERR_RATE_MISMATCH (*rate) will be updated with the jack servers rate
 */
int
JACK_Open(int *deviceID, unsigned int bits_per_channel,
          int floating_point, unsigned long *rate,
          unsigned int output_channels)
{
  jack_driver_t *drv = 0;
  int sample_format = SAMPLE_FMT_INTEGER;
  unsigned int i;
  int retval;

  if(output_channels < 1)
  {
    ERR("no output channels, nothing to do\n");
    return ERR_OPENING_JACK;
  }

  switch (bits_per_channel)
  {
  case 8:
  case 16:
  case 32:
    break;
  case 24:
    bits_per_channel = 32;
    sample_format = SAMPLE_FMT_PACKED_24B;
    break;
  default:
    ERR("invalid bits_per_channel\n");
    return ERR_OPENING_JACK;
  }

  if (floating_point)
  {
    if (bits_per_channel != 32) {
      ERR("bits_per_channel must be 32 for floating point\n");
      return ERR_OPENING_JACK;
    }
    else
    {
      sample_format = SAMPLE_FMT_FLOAT;
    }
  }

  /* Lock the device_mutex and find one that's not allocated already.
     We'll keep this lock until we've either made use of it, or given up. */
  pthread_mutex_lock(&device_mutex);

  for(i = 0; i < MAX_OUTDEVICES; i++)
  {
    if(!outDev[i].allocated)
    {
      drv = &outDev[i];
      break;
    }
  }

  if(!drv)
  {
    ERR("no more devices available\n");
    pthread_mutex_unlock(&device_mutex);
    return ERR_OPENING_JACK;
  }

  /* We found an unallocated device, now lock it for extra saftey */
  getDriver(drv->deviceID);

  TRACE("bits_per_channel=%d rate=%ld, output_channels=%d\n",
     bits_per_channel, *rate, output_channels);

  if(output_channels > MAX_OUTPUT_PORTS)
  {
    ERR("output_channels == %u, MAX_OUTPUT_PORTS == %u\n", output_channels,
        MAX_OUTPUT_PORTS);
    releaseDriver(drv);
    pthread_mutex_unlock(&device_mutex);
    return ERR_TOO_MANY_OUTPUT_CHANNELS;
  }

  drv->jack_output_port_flags = JackPortIsPhysical | JackPortIsInput;      /* port must be input(ie we can put data into it), so mask this in */

  /* initialize some variables */
  drv->in_use = false;

  JACK_ResetFromDriver(drv);    /* flushes all queued buffers, sets status to STOPPED and resets some variables */

  /* drv->jack_sample_rate is set by JACK_OpenDevice() */
  drv->client_sample_rate = *rate;
  drv->bits_per_channel = bits_per_channel;
  drv->sample_format = sample_format;
  drv->num_output_channels = output_channels;
  drv->bytes_per_output_frame = (drv->bits_per_channel * drv->num_output_channels) / 8;
  drv->bytes_per_jack_output_frame = sizeof(sample_t) * drv->num_output_channels;

  if(drv->num_output_channels > 0)
  {
    drv->pPlayPtr = jack_ringbuffer_create(drv->num_output_channels *
                                           drv->bytes_per_jack_output_frame *
                                           DEFAULT_RB_SIZE);
  }

  DEBUG("bytes_per_output_frame == %ld\n", drv->bytes_per_output_frame);
  DEBUG("bytes_per_jack_output_frame == %ld\n",
        drv->bytes_per_jack_output_frame);

  /* go and open up the device */
  retval = JACK_OpenDevice(drv);
  if(retval != ERR_SUCCESS)
  {
    TRACE("error opening jack device\n");
    releaseDriver(drv);
    pthread_mutex_unlock(&device_mutex);
    return retval;
  }
  else
  {
    TRACE("succeeded opening jack device\n");
  }

  if((long) (*rate) != drv->jack_sample_rate)
  {
    TRACE("rate of %ld doesn't match jack sample rate of %ld, returning error\n",
          *rate, drv->jack_sample_rate);
    *rate = drv->jack_sample_rate;
#if JACK_CLOSE_HACK
    JACK_CloseDevice(drv, true);
#else
    JACK_CloseDevice(drv);
#endif
    releaseDriver(drv);
    pthread_mutex_unlock(&device_mutex);
    return ERR_RATE_MISMATCH;
  }

  drv->allocated = true;        /* record that we opened this device */

  DEBUG("sizeof(sample_t) == %d\n", sizeof(sample_t));

  *deviceID = drv->deviceID;    /* set the deviceID for the caller */
  releaseDriver(drv);
  pthread_mutex_unlock(&device_mutex);
  return ERR_SUCCESS;           /* success */
}

/* Close the jack device */
//FIXME: add error handling in here at some point...
/* NOTE: return 0 for success, non-zero for failure */
int
JACK_Close(int deviceID)
{
  jack_driver_t *drv = getDriver(deviceID);

  TRACE("deviceID(%d)\n", deviceID);

#if JACK_CLOSE_HACK
  JACK_CloseDevice(drv, true);
#else
  JACK_CloseDevice(drv);
#endif

  JACK_ResetFromDriver(drv);    /* reset this device to a normal starting state */

  pthread_mutex_lock(&device_mutex);

  /* free buffer memory */
  drv->callback_buffer2_size = 0;
  if(drv->callback_buffer2) g_free(drv->callback_buffer2);
  drv->callback_buffer2 = 0;

  drv->rw_buffer1_size = 0;
  if(drv->rw_buffer1) g_free(drv->rw_buffer1);
  drv->rw_buffer1 = 0;

  if(drv->pPlayPtr) jack_ringbuffer_free(drv->pPlayPtr);
  drv->pPlayPtr = 0;

  drv->allocated = false;       /* release this device */

  pthread_mutex_unlock(&device_mutex);

  releaseDriver(drv);

  return 0;
}

/* If we haven't already taken in the max allowed data then create a wave header */
/* to package the audio data and attach the wave header to the end of the */
/* linked list of wave headers */
/* These wave headers will be peeled off as they are played by the callback routine */
/* Return value is the number of bytes written */
/* NOTE: this function takes the length of data to be written bytes */
long
JACK_Write(int deviceID, const unsigned char *data, unsigned long bytes)
{
  jack_driver_t *drv = getDriver(deviceID);

  long frames_free, frames;

  TIMER("start\n");

  TRACE("deviceID(%d), bytes == %ld\n", deviceID, bytes);

  /* check and see that we have enough space for this audio */
  frames_free =
    jack_ringbuffer_write_space(drv->pPlayPtr) /
    drv->bytes_per_jack_output_frame;
  frames = bytes / drv->bytes_per_output_frame;
  TRACE("frames free == %ld, bytes = %lu\n", frames_free, bytes);

  TRACE("state = '%s'\n", DEBUGSTATE(drv->state));
  /* if we are currently STOPPED we should start playing now...
     do this before the check for bytes == 0 since some clients like
     to write 0 bytes the first time out */
  if(drv->state == STOPPED)
  {
    TRACE("currently STOPPED, transitioning to PLAYING\n");
    drv->state = PLAYING;
  }

  /* handle the case where the user calls this routine with 0 bytes */
  if(bytes == 0 || frames_free < 1)
  {
    TRACE("no room left\n");
    TIMER("finish (nothing to do, buffer is full)\n");
    releaseDriver(drv);
    return 0;                   /* indicate that we couldn't write any bytes */
  }

  frames = min(frames, frames_free);
  long jack_bytes = frames * drv->bytes_per_jack_output_frame;
  if(!ensure_buffer_size(&drv->rw_buffer1, &drv->rw_buffer1_size, jack_bytes))
  {
    ERR("couldn't allocate enough space for the buffer\n");
    releaseDriver(drv);
    return 0;
  }
  /* adjust bytes to be how many client bytes we're actually writing */
  bytes = frames * drv->bytes_per_output_frame;

  /* convert from client samples to jack samples
     we have to tell it how many samples there are, which is frames * channels */
  switch (drv->bits_per_channel)
  {
  case 8:
    sample_move_char_float((sample_t *) drv->rw_buffer1, (unsigned char *) data,
                           frames * drv->num_output_channels);
    break;
  case 16:
    sample_move_short_float((sample_t *) drv->rw_buffer1, (short *) data,
                            frames * drv->num_output_channels);
    break;
  case 32:
    if (drv->sample_format == SAMPLE_FMT_FLOAT)
      sample_move_float_float((sample_t *) drv->rw_buffer1, (float *) data,
                            frames * drv->num_output_channels);
    else if (drv->sample_format == SAMPLE_FMT_PACKED_24B)
      sample_move_int24_float((sample_t *) drv->rw_buffer1, (int32_t *) data,
			      frames * drv->num_output_channels);
    else
      sample_move_int32_float((sample_t *) drv->rw_buffer1, (int32_t *) data,
			      frames * drv->num_output_channels);
    break;
  }

  DEBUG("ringbuffer read space = %d, write space = %d\n",
        jack_ringbuffer_read_space(drv->pPlayPtr),
        jack_ringbuffer_write_space(drv->pPlayPtr));

  jack_ringbuffer_write(drv->pPlayPtr, drv->rw_buffer1, jack_bytes);
  DEBUG("wrote %lu bytes, %lu jack_bytes\n", bytes, jack_bytes);

  DEBUG("ringbuffer read space = %d, write space = %d\n",
        jack_ringbuffer_read_space(drv->pPlayPtr),
        jack_ringbuffer_write_space(drv->pPlayPtr));

  drv->client_bytes += bytes;   /* update client_bytes */

  TIMER("finish\n");

  DEBUG("returning bytes written of %ld\n", bytes);

  releaseDriver(drv);
  return bytes;                 /* return the number of bytes we wrote out */
}

/* return ERR_SUCCESS for success */
static int
JACK_SetVolumeForChannelFromDriver(jack_driver_t * drv,
                                   unsigned int channel, unsigned int volume)
{
  /* TODO?: maybe we should have different volume levels for input & output */
  /* ensure that we have the channel we are setting volume for */
  if(channel > (drv->num_output_channels - 1))
    return 1;

  if(volume > 100)
    volume = 100;               /* check for values in excess of max */

  drv->volume[channel] = volume;
  return ERR_SUCCESS;
}

/* return ERR_SUCCESS for success */
int
JACK_SetVolumeForChannel(int deviceID, unsigned int channel,
                         unsigned int volume)
{
  jack_driver_t *drv = getDriver(deviceID);
  int retval = JACK_SetVolumeForChannelFromDriver(drv, channel, volume);
  releaseDriver(drv);
  return retval;
}

/* Return the current volume in the inputted pointers */
/* NOTE: we check for null pointers being passed in just in case */
void
JACK_GetVolumeForChannel(int deviceID, unsigned int channel,
                         unsigned int *volume)
{
  jack_driver_t *drv = getDriver(deviceID);

  /* ensure that we have the channel we are getting volume for */
  if(channel > (drv->num_output_channels - 1))
  {
    ERR("asking for channel index %u but we only have %lu channels\n", channel, drv->num_output_channels);
    releaseDriver(drv);
    return;
  }

  if(volume)
    *volume = drv->volume[channel];

#if VERBOSE_OUTPUT
  if(volume)
  {
    TRACE("deviceID(%d), returning volume of %d for channel %d\n",
          deviceID, *volume, channel);
  }
  else
  {
    TRACE("volume is null, can't dereference it\n");
  }
#endif

  releaseDriver(drv);
}


/* Controls the state of the playback(playing, paused, ...) */
int
JACK_SetState(int deviceID, enum status_enum state)
{
  jack_driver_t *drv = getDriver(deviceID);

  switch (state)
  {
  case PAUSED:
    drv->state = PAUSED;
    break;
  case PLAYING:
    drv->state = PLAYING;
    break;
  case STOPPED:
    drv->state = STOPPED;
    break;
  default:
    TRACE("unknown state of %d\n", state);
  }

  TRACE("%s\n", DEBUGSTATE(drv->state));

  releaseDriver(drv);
  return 0;
}

/* Retrieve the current state of the device */
enum status_enum
JACK_GetState(int deviceID)
{
  jack_driver_t *drv = getDriver(deviceID);
  enum status_enum return_val;

  return_val = drv->state;
  releaseDriver(drv);

  TRACE("deviceID(%d), returning current state of %s\n", deviceID,
        DEBUGSTATE(return_val));
  return return_val;
}

/* Retrieve the number of bytes per second we are outputting */
unsigned long
JACK_GetOutputBytesPerSecondFromDriver(jack_driver_t * drv)
{
  unsigned long return_val;

  return_val = drv->bytes_per_output_frame * drv->client_sample_rate;

#if VERBOSE_OUTPUT
  TRACE("deviceID(%d), return_val = %ld\n", drv->deviceID, return_val);
#endif

  return return_val;
}

static unsigned long
JACK_GetBytesFreeSpaceFromDriver(jack_driver_t * drv)
{
  if(drv->pPlayPtr == 0 || drv->bytes_per_jack_output_frame == 0)
    return 0;

  /* leave at least one frame in the buffer at all times to prevent underruns */
  long return_val = jack_ringbuffer_write_space(drv->pPlayPtr) - drv->jack_buffer_size;
  if(return_val <= 0)
  {
    return_val = 0;
  } else
  {
    /* adjust from jack bytes to client bytes */
    return_val =
      return_val / drv->bytes_per_jack_output_frame *
      drv->bytes_per_output_frame;
  }

  return return_val;
}

/* Return the number of bytes we can write to the device */
unsigned long
JACK_GetBytesFreeSpace(int deviceID)
{
  jack_driver_t *drv = getDriver(deviceID);
  unsigned long return_val;

  return_val = JACK_GetBytesFreeSpaceFromDriver(drv);
  releaseDriver(drv);

  TRACE("deviceID(%d), retval == %ld\n", deviceID, return_val);

  return return_val;
}

/* Get the current position of the driver, either in bytes or */
/* in milliseconds */
/* NOTE: this is position relative to input bytes, output bytes may differ greatly due to
   input vs. output channel count */
static long
JACK_GetPositionFromDriver(jack_driver_t * drv)
{
  long return_val = 0;
  struct timeval now;
  long elapsedMS;
  double sec2msFactor = 1000;

#if TRACE_ENABLE
  const char *type_str = "PLAYED";
#endif

  /* if we are reset we should return a position of 0 */
  if(drv->state == RESET)
  {
    TRACE("we are currently RESET, returning 0\n");
    return 0;
  }

    return_val = drv->played_client_bytes;
    gettimeofday(&now, 0);

    elapsedMS = TimeValDifference(&drv->previousTime, &now);    /* find the elapsed milliseconds since last JACK_Callback() */

    TRACE("elapsedMS since last callback is '%ld'\n", elapsedMS);

    /* account for the bytes played since the last JACK_Callback() */
    /* NOTE: [Xms * (Bytes/Sec)] * (1 sec/1,000ms) */
    /* NOTE: don't do any compensation if no data has been sent to jack since the last callback */
    /* as this would result a bogus computed result */
    if(drv->clientBytesInJack != 0)
    {
      return_val += (long) ((double) elapsedMS *
                            ((double) JACK_GetOutputBytesPerSecondFromDriver(drv) /
                             sec2msFactor));
    } else
    {
      TRACE("clientBytesInJack == 0\n");
    }

  /* add on the offset */
  return_val += drv->position_byte_offset;

  /* convert byte position to milliseconds value */
    if(JACK_GetOutputBytesPerSecondFromDriver(drv) != 0)
    {
      return_val = (long) (((double) return_val /
                            (double) JACK_GetOutputBytesPerSecondFromDriver(drv)) *
                           (double) sec2msFactor);
    } else
    {
      return_val = 0;
    }

  TRACE("drv->deviceID(%d), type(%s), return_val = %ld\n", drv->deviceID,
        type_str, return_val);

  return return_val;
}

/* Get the current position of the driver, either in bytes or */
/* in milliseconds */
/* NOTE: this is position relative to input bytes, output bytes may differ greatly due to input vs. output channel count */
long
JACK_GetPosition(int deviceID)
{
  jack_driver_t *drv = getDriver(deviceID);
  long retval = JACK_GetPositionFromDriver(drv);
  releaseDriver(drv);
  TRACE("retval == %ld\n", retval);
  return retval;
}

// Set position always applies to written bytes
// NOTE: we must apply this instantly because if we pass this as a message
//   to the callback we risk the user sending us audio data in the mean time
//   and there is no need to send this as a message, we don't modify any
//   internal variables
void
JACK_SetPositionFromDriver(jack_driver_t * drv, long value)
{
  double sec2msFactor = 1000;
#if TRACE_ENABLE
  long input_value = value;
#endif

  /* convert the incoming value from milliseconds into bytes */
    value = (long) (((double) value *
                     (double) JACK_GetOutputBytesPerSecondFromDriver(drv)) /
                    sec2msFactor);

  /* ensure that if the user asks for the position */
  /* they will at this instant get the correct position */
  drv->position_byte_offset = value - drv->client_bytes;

  TRACE("deviceID(%d) input_value of %ld %s, new value of %ld, setting position_byte_offset to %ld\n",
        drv->deviceID, input_value, "ms",
        value, drv->position_byte_offset);
}

// Set position always applies to written bytes
// NOTE: we must apply this instantly because if we pass this as a message
//   to the callback we risk the user sending us audio data in the mean time
//   and there is no need to send this as a message, we don't modify any
//   internal variables
void
JACK_SetPosition(int deviceID, long value)
{
  jack_driver_t *drv = getDriver(deviceID);
  JACK_SetPositionFromDriver(drv, value);
  releaseDriver(drv);

  TRACE("deviceID(%d) value of %ld\n", drv->deviceID, value);
}

void
JACK_CleanupDriver(jack_driver_t * drv)
{
  TRACE("\n");
  /* things that need to be reset both in JACK_Init & JACK_CloseDevice */
  drv->client = 0;
  drv->in_use = false;
  drv->state = CLOSED;
  drv->jack_sample_rate = 0;
  drv->jackd_died = false;
  gettimeofday(&drv->previousTime, 0);  /* record the current time */
  gettimeofday(&drv->last_reconnect_attempt, 0);
}

/* Initialize the jack porting library to a clean state */
void
JACK_Init(void)
{
  jack_driver_t *drv;
  int x, y;

  if(init_done)
  {
    TRACE("not initing twice\n");
    return;
  }

  init_done = 1;

  TRACE("\n");

  pthread_mutex_lock(&device_mutex);

  /* initialize the device structures */
  for(x = 0; x < MAX_OUTDEVICES; x++)
  {
    drv = &outDev[x];

    pthread_mutex_init(&drv->mutex, nullptr);

    getDriver(x);

    memset(drv, 0, sizeof(jack_driver_t));
    drv->deviceID = x;

    for(y = 0; y < MAX_OUTPUT_PORTS; y++)       /* make all volume 25% as a default */
      drv->volume[y] = 25;

    JACK_CleanupDriver(drv);
    JACK_ResetFromDriver(drv);
    releaseDriver(drv);
  }

  pthread_mutex_unlock(&device_mutex);

  TRACE("finished\n");
}

void
JACK_SetPortConnectionMode(enum JACK_PORT_CONNECTION_MODE mode)
{
    port_connection_mode = mode;
}
