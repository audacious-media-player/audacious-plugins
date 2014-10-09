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

#include <assert.h>
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

#include <type_traits>

#include <libaudcore/audio.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/runtime.h>

#include "bio2jack.h"

using aud::min;
using aud::max;

static_assert(std::is_same<jack_default_audio_sample_t, float>::value,
 "JACK must be compiled to use float samples");

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
#define CALLBACK_TRACE AUDDBG
#else
#define CALLBACK_TRACE(...)
#endif

#define WARN AUDWARN
#define ERR AUDERR

typedef struct jack_driver_s
{
  int clientCtr;                        /* to prevent overlapping client ids */
  int jack_sample_rate;                 /* jack samples(frames) per second */
  bool sample_rate_changed;
  int client_sample_rate;               /* client samples(frames) per second */
  int num_output_channels;              /* number of output channels(1 is mono, 2 stereo etc..) */
  int bytes_per_output_frame;           /* sizeof(float) * num_output_channels */

  long clientBytesInJack;       /* number of INPUT bytes(from the client of bio2jack) we wrote to jack(not necessary the number of bytes we wrote to jack) */

  Index<char> callback_buffer2;  /* buffer for processing data in JACK_Callback */

  struct timeval previousTime;  /* time of last JACK_Callback() write to jack, allows for MS accurate bytes played  */

  unsigned long written_client_bytes;   /* input bytes we wrote to jack, not necessarily actual bytes we wrote to jack due to channel and other conversion */
  unsigned long played_client_bytes;    /* input bytes that jack has played */

  unsigned long client_bytes;   /* total bytes written by the client of bio2jack via JACK_Write() */

  jack_port_t *output_port[AUD_MAX_CHANNELS];   /* output ports */

  jack_client_t *client;        /* pointer to jack client */

  jack_ringbuffer_t *pPlayPtr;  /* the playback ringbuffer */

  enum status_enum state;       /* one of PLAYING, PAUSED, STOPPED, CLOSED, RESET etc */

  long position_byte_offset;    /* an offset that we will apply to returned position queries to achieve */
                                /* the position that the user of the driver desires set */

  pthread_mutex_t mutex;        /* mutex to lock this specific device */

  void (*free_space_notify)();  /* called when there is free space in the buffer */

  /* variables used for trying to restart the connection to jack */
  bool jackd_died;              /* true if jackd has died and we should try to restart it */
  struct timeval last_reconnect_attempt;
} jack_driver_t;


static bool init_done = false;  /* just to prevent clients from calling JACK_Init twice, that would be very bad */

typedef jack_nframes_t nframes_t;

/* output device */
static jack_driver_t outDev;

/* Prototypes */
static bool JACK_OpenDevice(jack_driver_t * drv, String & error);
static void JACK_CloseDevice(jack_driver_t * drv);
static size_t JACK_GetBytesFreeSpaceFromDriver(jack_driver_t * drv);
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
jack_driver_t *
getDriver(jack_driver_t *drv)
{
  pthread_mutex_lock(&drv->mutex);

  /* should we try to restart the jack server? */
  if(drv->jackd_died && drv->client == 0)
  {
    struct timeval now;
    gettimeofday(&now, 0);

    /* wait 250ms before trying again */
    if(TimeValDifference(&drv->last_reconnect_attempt, &now) >= 250)
    {
      String ignored_error;
      JACK_OpenDevice(drv, ignored_error);
      drv->last_reconnect_attempt = now;
    }
  }

  return drv;
}

/* release a device's mutex */
void
releaseDriver(jack_driver_t *drv)
{
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

  CALLBACK_TRACE("nframes %ld, sizeof(float) == %d\n", (long) nframes,
                 sizeof(float));

  float *out_buffer[AUD_MAX_CHANNELS];
  /* retrieve the buffers for the output ports */
  for(int i = 0; i < drv->num_output_channels; i++)
    out_buffer[i] = (float *) jack_port_get_buffer(drv->output_port[i], nframes);

  /* handle playing state */
  if(drv->state == PLAYING && drv->client_sample_rate == drv->jack_sample_rate)
  {
    /* handle playback data, if any */
    if(drv->num_output_channels > 0)
    {
      unsigned long jackFramesAvailable = nframes;      /* frames we have left to write to jack */
      unsigned long numFramesToWrite;   /* num frames we are writing */
      size_t inputBytesAvailable = jack_ringbuffer_read_space(drv->pPlayPtr);
      unsigned long inputFramesAvailable;       /* frames we have available */

      inputFramesAvailable = inputBytesAvailable / drv->bytes_per_output_frame;
      size_t jackBytesAvailable = jackFramesAvailable * drv->bytes_per_output_frame;

      long read = 0;

      CALLBACK_TRACE("playing... jackFramesAvailable = %ld inputFramesAvailable = %ld\n",
         jackFramesAvailable, inputFramesAvailable);

      /* make sure our buffer is large enough for the data we are writing */
      /* ie. callback_buffer2_size < (bytes we already wrote + bytes we are going to write in this loop) */
      drv->callback_buffer2.resize(jackBytesAvailable);

        /* read as much data from the buffer as is available */
        if(jackFramesAvailable && inputBytesAvailable > 0)
        {
          /* write as many bytes as we have space remaining, or as much as we have data to write */
          numFramesToWrite = min(jackFramesAvailable, inputFramesAvailable);
          jack_ringbuffer_read(drv->pPlayPtr, drv->callback_buffer2.begin(),
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
        for(int i = 0; i < drv->num_output_channels; i++)
          memset(out_buffer[i] + (nframes - jackFramesAvailable), 0,
                 sizeof(float) * jackFramesAvailable);
      }

      StereoVolume v = {aud_get_int ("jack", "volume_left"), aud_get_int ("jack", "volume_right")};
      audio_amplify ((float *) drv->callback_buffer2.begin(),
       drv->num_output_channels, nframes - jackFramesAvailable, v);

      audio_deinterlace (drv->callback_buffer2.begin(), FMT_FLOAT,
       drv->num_output_channels, (void * const *) out_buffer,
       nframes - jackFramesAvailable);
    }
  }
  else
  {
    if(drv->sample_rate_changed && drv->client_sample_rate != drv->jack_sample_rate)
      aud_ui_show_error(str_printf(_("The JACK server expects a sample rate of "
       "%d Hz, but Audacious is playing at %d Hz.  Please use the Sample Rate "
       "Converter effect to correct the mismatch."), drv->jack_sample_rate,
       drv->client_sample_rate));

    drv->sample_rate_changed = false;

    CALLBACK_TRACE("%s, outputting silence\n", DEBUGSTATE(drv->state));

    /* output silence if nothing is being outputted */
    for(int i = 0; i < drv->num_output_channels; i++)
      memset(out_buffer[i], 0, sizeof(float) * nframes);

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

  if (drv->free_space_notify)
    drv->free_space_notify();

  CALLBACK_TRACE("done\n");
  TIMER("finish\n");

  return 0;
}


/******************************************************************
 *		JACK_srate
 */
int
JACK_srate(nframes_t nframes, void *arg)
{
  jack_driver_t *drv = (jack_driver_t *) arg;

  drv->jack_sample_rate = nframes;
  drv->sample_rate_changed = true;

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

  getDriver(drv);

  drv->client = 0;              /* reset client */
  drv->jackd_died = true;

  TRACE("jack shutdown, setting client to 0 and jackd_died to true, closing device\n");

  JACK_CloseDevice(drv);

  TRACE("trying to reconnect right now\n");
  /* lets see if we can't reestablish the connection */
  String error;
  if(!JACK_OpenDevice(drv, error))
    aud_ui_show_error(error);

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
static bool
JACK_OpenDevice(jack_driver_t * drv, String & error)
{
  const char **ports;
  bool failed = false;

  TRACE("creating jack client and setting up callbacks\n");
  assert(!drv->client);

  /* set up an error handler */
  jack_set_error_function(JACK_Error);

  /* build the client name */
  StringBuf our_client_name = str_printf("%s_%d_%02d", "audacious-jack",
   getpid(), drv->clientCtr++);

  /* try to become a client of the JACK server */
  TRACE("client name '%s'\n", (const char *) our_client_name);
  if((drv->client = jack_client_open(our_client_name, JackNoStartServer, nullptr)) == 0)
  {
    error = String(_("jack_client_open failed; is the JACK server running?"));
    return false;
  }

  TRACE("setting up jack callbacks\n");

  /* JACK server to call `JACK_callback()' whenever
     there is work to be done. */
  jack_set_process_callback(drv->client, JACK_callback, drv);

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
  drv->sample_rate_changed = true;
  TRACE("client sample rate: %lu, jack sample rate: %lu\n",
        drv->client_sample_rate, drv->jack_sample_rate);

  /* create the output ports */
  TRACE("creating output ports\n");
  for(int i = 0; i < drv->num_output_channels; i++)
  {
    StringBuf portname = str_printf ("out_%d", i);
    TRACE("port %d is named '%s'\n", i, (const char *) portname);
    /* NOTE: Yes, this is supposed to be JackPortIsOutput since this is an output */
    /* port FROM bio2jack */
    drv->output_port[i] = jack_port_register(drv->client, portname,
                                             JACK_DEFAULT_AUDIO_TYPE,
                                             JackPortIsOutput, 0);
  }

  /* tell the JACK server that we are ready to roll */
  TRACE("calling jack_activate()\n");
  if(jack_activate(drv->client))
  {
    error = String(_("jack_activate failed."));
    return false;
  }

  /* should we connect up some ports? */
  if(aud_get_bool("jack", "auto_connect"))
  {
        TRACE("jack_get_ports() passing in nullptr/nullptr\n");
        ports = jack_get_ports(drv->client, nullptr, nullptr,
                               JackPortIsPhysical | JackPortIsInput);

      /* display a trace of the output ports we found */
      int num_ports = 0;
      if(ports)
      {
        for(int i = 0; ports[i]; i++)
        {
          TRACE("ports[%d] = '%s'\n", i, ports[i]);
          num_ports++;
        }
      }

      /* ensure that we found enough ports */
      if(!ports || (num_ports < drv->num_output_channels))
      {
        error = String(_("jack_get_ports did not find enough output ports."));
        JACK_CloseDevice(drv);
        return false;
      }

      /* connect a port for each output channel. Note: you can't do this before
         the client is activated (this may change in the future). */
      for(int i = 0; i < drv->num_output_channels; i++)
      {
        TRACE("jack_connect() to port %d('%p')\n", i, drv->output_port[i]);
        if(jack_connect(drv->client, jack_port_name(drv->output_port[i]), ports[i]))
        {
          error = String(str_printf(_("jack_connect failed for port %d "
           "(%s).\n"), i, ports[i]));
          failed = true;
        }
      }

      jack_free(ports);              /* free the returned array of ports */
  }                             /* if( drv->num_output_channels > 0 ) */


  /* if something failed we need to shut the client down and return 0 */
  if(failed)
  {
    TRACE("failed, closing and returning error\n");
    JACK_CloseDevice(drv);
    return false;
  }

  TRACE("success\n");

  drv->jackd_died = false;      /* clear out this flag so we don't keep attempting to restart things */
  drv->state = PLAYING;         /* clients seem to behave much better with this on from the start, especially when recording */

  return true;
}


/******************************************************************
 *		JACK_CloseDevice
 *
 *	Close the connection to the server cleanly.
 */
static void
JACK_CloseDevice(jack_driver_t * drv)
{
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
}


/**************************************/
/* External interface functions below */
/**************************************/

/* Clear out any buffered data, stop playing, zero out some variables */
static void
JACK_ResetFromDriver(jack_driver_t * drv)
{
  TRACE("resetting drv\n");

  /* NOTE: we use the RESET state so we don't need to worry about clearing out */
  /* variables that the callback modifies while the callback is running */
  /* we set the state to RESET and the callback clears the variables out for us */
  drv->state = RESET;           /* tell the callback that we are to reset, the callback will transition this to STOPPED */
}

/* Clear out any buffered data, stop playing, zero out some variables */
void
JACK_Reset()
{
  jack_driver_t *drv = getDriver(&outDev);
  TRACE("resetting\n");
  JACK_ResetFromDriver(drv);
  releaseDriver(drv);
}


/*
 * open the audio device for writing to
 *
 * return value is zero upon success, non-zero upon failure
 */
bool
JACK_Open(int rate, int output_channels,
          void (*free_space_notify)())
{
  jack_driver_t *drv = 0;

  assert(output_channels > 0 && output_channels <= AUD_MAX_CHANNELS);

  drv = getDriver(&outDev);

  TRACE("rate=%ld, output_channels=%d\n", rate, output_channels);

  JACK_ResetFromDriver(drv);    /* flushes all queued buffers, sets status to STOPPED and resets some variables */

  /* drv->jack_sample_rate is set by JACK_OpenDevice() */
  drv->client_sample_rate = rate;
  drv->num_output_channels = output_channels;
  drv->bytes_per_output_frame = sizeof(float) * drv->num_output_channels;

    drv->pPlayPtr = jack_ringbuffer_create(drv->num_output_channels *
                                           drv->bytes_per_output_frame *
                                           DEFAULT_RB_SIZE);

  DEBUG("bytes_per_output_frame == %ld\n", drv->bytes_per_output_frame);

  drv->free_space_notify = free_space_notify;

  /* go and open up the device */
  String error;
  if(!JACK_OpenDevice(drv, error))
  {
    aud_ui_show_error(error);
    releaseDriver(drv);
    return false;
  }

  TRACE("succeeded opening jack device\n");
  DEBUG("sizeof(float) == %d\n", sizeof(float));

  releaseDriver(drv);
  return true;
}

/* Close the jack device */
void JACK_Close()
{
  jack_driver_t *drv = getDriver(&outDev);

  TRACE("close\n");

  JACK_CloseDevice(drv);

  JACK_ResetFromDriver(drv);    /* reset this device to a normal starting state */

  /* free buffer memory */
  drv->callback_buffer2.clear();

  if(drv->pPlayPtr) jack_ringbuffer_free(drv->pPlayPtr);
  drv->pPlayPtr = 0;

  drv->free_space_notify = nullptr;

  releaseDriver(drv);
}

/* If we haven't already taken in the max allowed data then create a wave header */
/* to package the audio data and attach the wave header to the end of the */
/* linked list of wave headers */
/* These wave headers will be peeled off as they are played by the callback routine */
/* Return value is the number of bytes written */
/* NOTE: this function takes the length of data to be written bytes */
long
JACK_Write(const char *data, unsigned long bytes)
{
  jack_driver_t *drv = getDriver(&outDev);

  long frames_free, frames;

  TIMER("start\n");

  TRACE("bytes == %ld\n", bytes);

  /* check and see that we have enough space for this audio */
  frames_free =
    jack_ringbuffer_write_space(drv->pPlayPtr) /
    drv->bytes_per_output_frame;
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

  /* adjust bytes to be how many client bytes we're actually writing */
  bytes = frames * drv->bytes_per_output_frame;

  DEBUG("ringbuffer read space = %d, write space = %d\n",
        jack_ringbuffer_read_space(drv->pPlayPtr),
        jack_ringbuffer_write_space(drv->pPlayPtr));

  jack_ringbuffer_write(drv->pPlayPtr, data, bytes);
  DEBUG("wrote %lu bytes\n", bytes);

  DEBUG("ringbuffer read space = %d, write space = %d\n",
        jack_ringbuffer_read_space(drv->pPlayPtr),
        jack_ringbuffer_write_space(drv->pPlayPtr));

  drv->client_bytes += bytes;   /* update client_bytes */

  TIMER("finish\n");

  DEBUG("returning bytes written of %ld\n", bytes);

  releaseDriver(drv);
  return bytes;                 /* return the number of bytes we wrote out */
}


/* Controls the state of the playback(playing, paused, ...) */
int
JACK_SetState(enum status_enum state)
{
  jack_driver_t *drv = getDriver(&outDev);

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
JACK_GetState()
{
  jack_driver_t *drv = getDriver(&outDev);
  enum status_enum return_val;

  return_val = drv->state;
  releaseDriver(drv);

  TRACE("returning current state of %s\n",
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
  TRACE("return_val = %ld\n", return_val);
#endif

  return return_val;
}

static size_t
JACK_GetBytesFreeSpaceFromDriver(jack_driver_t * drv)
{
  if(drv->pPlayPtr == 0 || drv->bytes_per_output_frame == 0)
    return 0;

  size_t return_val = jack_ringbuffer_write_space(drv->pPlayPtr);

  /* do not return partial frames since JACK_Write will not write them */
  return return_val - return_val % drv->bytes_per_output_frame;
}

/* Return the number of bytes we can write to the device */
size_t JACK_GetBytesFreeSpace()
{
  jack_driver_t *drv = getDriver(&outDev);
  size_t return_val = JACK_GetBytesFreeSpaceFromDriver(drv);
  releaseDriver(drv);

  TRACE("retval == %zd\n", return_val);

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

  TRACE("drv(%d), type(%s), return_val = %ld\n", drv,
        type_str, return_val);

  return return_val;
}

/* Get the current position of the driver, either in bytes or */
/* in milliseconds */
/* NOTE: this is position relative to input bytes, output bytes may differ greatly due to input vs. output channel count */
long
JACK_GetPosition()
{
  jack_driver_t *drv = getDriver(&outDev);
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

  TRACE("input_value of %ld %s, new value of %ld, setting position_byte_offset to %ld\n",
        input_value, "ms",
        value, drv->position_byte_offset);
}

// Set position always applies to written bytes
// NOTE: we must apply this instantly because if we pass this as a message
//   to the callback we risk the user sending us audio data in the mean time
//   and there is no need to send this as a message, we don't modify any
//   internal variables
void
JACK_SetPosition(long value)
{
  jack_driver_t *drv = getDriver(&outDev);
  JACK_SetPositionFromDriver(drv, value);
  releaseDriver(drv);

  TRACE("value of %ld\n", value);
}

void
JACK_CleanupDriver(jack_driver_t * drv)
{
  TRACE("\n");
  /* things that need to be reset both in JACK_Init & JACK_CloseDevice */
  drv->client = 0;
  drv->state = CLOSED;
  drv->jack_sample_rate = 0;
  drv->clientBytesInJack = 0;
  gettimeofday(&drv->previousTime, 0);  /* record the current time */
  gettimeofday(&drv->last_reconnect_attempt, 0);
}

/* Initialize the jack porting library to a clean state */
void
JACK_Init()
{
  jack_driver_t *drv = &outDev;

  if(init_done)
  {
    TRACE("not initing twice\n");
    return;
  }

  init_done = true;

  TRACE("\n");

  /* initialize the device structure */
    pthread_mutex_init(&drv->mutex, nullptr);

    getDriver(drv);

    memset(drv, 0, sizeof(jack_driver_t));

    JACK_CleanupDriver(drv);
    JACK_ResetFromDriver(drv);
    releaseDriver(drv);

  TRACE("finished\n");
}
