/*      xmms - jack output plugin
 *    Copyright 2002 Chris Morgan<cmorgan@alum.wpi.edu>
 *
 *      audacious port (2005-2006) by Giacomo Lozito from develia.org
 *
 *    This code maps xmms calls into the jack translation library
 */

#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>

#include "bio2jack.h" /* includes for the bio2jack library */
#include "jack.h"

/* set to 1 for verbose output */
#define VERBOSE_OUTPUT          0

jackconfig jack_cfg;

#define OUTFILE stderr

#define TRACE(...)                                      \
    if(jack_cfg.isTraceEnabled) {                       \
        fprintf(OUTFILE, "%s:", __FUNCTION__),          \
        fprintf(OUTFILE, __VA_ARGS__),                    \
        fflush(OUTFILE);                                \
    }

#define ERR(...)                                        \
    if(jack_cfg.isTraceEnabled) {                       \
        fprintf(OUTFILE, "ERR: %s:", __FUNCTION__),     \
        fprintf(OUTFILE, __VA_ARGS__),                    \
        fflush(OUTFILE);                                \
    }


static int driver = 0; /* handle to the jack output device */

typedef struct format_info {
  int format;
  long    frequency;
  int     channels;
  long    bps;
} format_info_t;

static format_info_t input;
static format_info_t effect;
static format_info_t output;

static bool_t output_opened; /* true if we have a connection to jack */
static bool_t paused;


/* Giacomo's note: removed the destructor from the original xmms-jack, cause
   destructors + thread join + NPTL currently leads to problems; solved this
   by adding a cleanup function callback for output plugins in Audacious, this
   is used to close the JACK connection and to perform a correct shutdown */
static void jack_cleanup(void)
{
  int errval;
  TRACE("cleanup\n");

  if((errval = JACK_Close(driver)))
    ERR("error closing device, errval of %d\n", errval);

  return;
}


/* Set the volume */
static void jack_set_volume(int l, int r)
{
  if(output.channels == 1)
  {
    TRACE("l(%d)\n", l);
  } else if(output.channels > 1)
  {
    TRACE("l(%d), r(%d)\n", l, r);
  }

  if(output.channels > 0) {
      JACK_SetVolumeForChannel(driver, 0, l);
      jack_cfg.volume_left = l;
  }
  if(output.channels > 1) {
      JACK_SetVolumeForChannel(driver, 1, r);
      jack_cfg.volume_right = r;
  }
}


/* Get the current volume setting */
static void jack_get_volume(int *l, int *r)
{
  unsigned int _l, _r;

  if(output.channels > 0)
  {
      JACK_GetVolumeForChannel(driver, 0, &_l);
      (*l) = _l;
  }
  if(output.channels > 1)
  {
      JACK_GetVolumeForChannel(driver, 1, &_r);
      (*r) = _r;
  }

#if VERBOSE_OUTPUT
  if(output.channels == 1)
      TRACE("l(%d)\n", *l);
  else if(output.channels > 1)
      TRACE("l(%d), r(%d)\n", *l, *r);
#endif
}


/* Return the current number of milliseconds of audio data that has */
/* been played out of the audio device, not including the buffer */
static int jack_get_output_time(void)
{
  int return_val;

  /* don't try to get any values if the device is still closed */
  if(JACK_GetState(driver) == CLOSED)
    return_val = 0;
  else
    return_val = JACK_GetPosition(driver, MILLISECONDS, PLAYED); /* get played position in terms of milliseconds */

  TRACE("returning %d milliseconds\n", return_val);
  return return_val;
}


/* returns TRUE if we are currently playing */
/* NOTE: this was confusing at first BUT, if the device is open and there */
/* is no more audio to be played, then the device is NOT PLAYING */
#if 0
static int jack_playing(void)
{
  int return_val;

  /* If we are playing see if we ACTUALLY have something to play */
  if(JACK_GetState(driver) == PLAYING)
  {
    /* If we have zero bytes stored, we are done playing */
    if(JACK_GetBytesStored(driver) == 0)
      return_val = FALSE;
    else
      return_val = TRUE;
  }
  else
    return_val = FALSE;

  TRACE("returning %d\n", return_val);
  return return_val;
}
#endif

void jack_set_port_connection_mode()
{
  /* setup the port connection mode that determines how bio2jack will connect ports */
  char * mode_str = aud_get_str ("jack", "port_connection_mode");
  enum JACK_PORT_CONNECTION_MODE mode;

  if(strcmp(mode_str, "CONNECT_ALL") == 0)
      mode = CONNECT_ALL;
  else if(strcmp(mode_str, "CONNECT_OUTPUT") == 0)
      mode = CONNECT_OUTPUT;
  else if(strcmp(mode_str, "CONNECT_NONE") == 0)
      mode = CONNECT_NONE;
  else
  {
      TRACE("Defaulting to CONNECT_ALL");
      mode = CONNECT_ALL;
  }

  JACK_SetPortConnectionMode(mode);
  str_unref (mode_str);
}

static const char * const jack_defaults[] = {
 "isTraceEnabled", "FALSE",
 "port_connection_mode", "CONNECT_ALL",
 "volume_left", "25",
 "volume_right", "25",
 NULL};

static const ComboBoxElements mode_list[] = {
 {"CONNECT_ALL", N_("Connect to all available jack ports")},
 {"CONNECT_OUTPUT", N_("Connect only the output ports")},
 {"CONNECT_NONE", N_("Don't connect to any port")},
};

static const PreferencesWidget jack_widgets[] = {
    WidgetCombo (N_("Connection mode:"),
        {VALUE_STRING, 0, "jack", "port_connection_mode"},
        {mode_list, ARRAY_LEN (mode_list)}),
    WidgetCheck (N_("Enable debug printing"),
        {VALUE_BOOLEAN, 0, "jack", "isTraceEnabled"})
};

static const PluginPreferences jack_prefs = {
 .widgets = jack_widgets,
 .n_widgets = ARRAY_LEN (jack_widgets)
};

/* Initialize necessary things */
static bool_t jack_init (void)
{
  aud_config_set_defaults ("jack", jack_defaults);

  jack_cfg.isTraceEnabled = aud_get_bool ("jack", "isTraceEnabled");
  jack_cfg.volume_left = aud_get_int ("jack", "volume_left");
  jack_cfg.volume_right = aud_get_int ("jack", "volume_right");

  TRACE("initializing\n");
  JACK_Init(); /* initialize the driver */

  /* set the bio2jack name so users will see xmms-jack in their */
  /* jack client list */
  JACK_SetClientName("audacious-jack");

  /* set the port connection mode */
  jack_set_port_connection_mode();

  output_opened = FALSE;

  /* Always return OK, as we don't know about physical devices here */
  return TRUE;
}


/* Return the amount of data that can be written to the device */
static int audacious_jack_free(void)
{
  unsigned long return_val = JACK_GetBytesFreeSpace(driver);
  unsigned long tmp;

  /* adjust for frequency differences, otherwise xmms could send us */
  /* as much data as we have free, then we go to convert this to */
  /* the output frequency and won't have enough space, so adjust */
  /* by the ratio of the two */
  if(effect.frequency != output.frequency)
  {
    tmp = return_val;
    return_val = (return_val * effect.frequency) / output.frequency;
    TRACE("adjusting from %lu to %lu free bytes to compensate for frequency differences\n", tmp, return_val);
  }

  if(return_val > INT_MAX)
  {
      TRACE("Warning: return_val > INT_MAX\n");
      return_val = INT_MAX;
  }

  TRACE("free space of %lu bytes\n", return_val);

  return return_val;
}


/* Close the device */
static void jack_close(void)
{
  aud_set_int ("jack", "volume_left", jack_cfg.volume_left);
  aud_set_int ("jack", "volume_right", jack_cfg.volume_right);

  JACK_Reset(driver); /* flush buffers, reset position and set state to STOPPED */
  TRACE("resetting driver, not closing now, destructor will close for us\n");
}


/* Open the device up */
static int jack_open(int fmt, int sample_rate, int num_channels)
{
  int bits_per_sample;
  int floating_point = FALSE;
  int retval;
  unsigned long rate;

  TRACE("fmt == %d, sample_rate == %d, num_channels == %d\n",
    fmt, sample_rate, num_channels);

  if((fmt == FMT_U8) || (fmt == FMT_S8))
  {
    bits_per_sample = 8;
  } else if(fmt == FMT_S16_NE)
  {
    bits_per_sample = 16;
  } else if (fmt == FMT_S24_NE)
  {
    /* interpreted by bio2jack as 24 bit values packed to 32 bit samples */
    bits_per_sample = 24;
  } else if (fmt == FMT_S32_NE)
  {
    bits_per_sample = 32;
  } else if (fmt == FMT_FLOAT)
  {
    bits_per_sample = 32;
    floating_point = TRUE;
  } else {
    TRACE("sample format not supported\n");
    return 0;
  }

  /* record some useful information */
  input.format    = fmt;
  input.frequency = sample_rate;
  input.bps       = bits_per_sample * sample_rate * num_channels;
  input.channels  = num_channels;

  /* setup the effect as matching the input format */
  effect.format    = input.format;
  effect.frequency = input.frequency;
  effect.channels  = input.channels;
  effect.bps       = input.bps;

  /* if we are already opened then don't open again */
  if(output_opened)
  {
    /* if something has changed we should close and re-open the connect to jack */
    if((output.channels != input.channels) ||
       (output.frequency != input.frequency) ||
       (output.format != input.format))
    {
      TRACE("output.channels is %d, jack_open called with %d channels\n", output.channels, input.channels);
      TRACE("output.frequency is %ld, jack_open called with %ld\n", output.frequency, input.frequency);
      TRACE("output.format is %d, jack_open called with %d\n", output.format, input.format);
      jack_close();
      JACK_Close(driver);
    } else
    {
        TRACE("output_opened is TRUE and no options changed, not reopening\n");
        paused = FALSE;
        return 1;
    }
  }

  /* try to open the jack device with the requested rate at first */
  output.frequency = input.frequency;
  output.bps       = input.bps;
  output.channels  = input.channels;
  output.format    = input.format;

  rate = output.frequency;
  retval = JACK_Open(&driver, bits_per_sample, floating_point, &rate, output.channels);
  output.frequency = rate; /* avoid compile warning as output.frequency differs in type
                              from what JACK_Open() wants for the type of the rate parameter */
  if(retval == ERR_RATE_MISMATCH)
  {
    TRACE("set the resampling rate properly");
    return 0;
  } else if(retval != ERR_SUCCESS)
  {
    TRACE("failed to open jack with JACK_Open(), error %d\n", retval);
    return 0;
  }

  jack_set_volume(jack_cfg.volume_left, jack_cfg.volume_right); /* sets the volume to stored value */
  output_opened = TRUE;
  paused = FALSE;

  return 1;
}


/* write some audio out to the device */
static void jack_write(void * ptr, int length)
{
  long written;

  TRACE("starting length of %d\n", length);

  /* loop until we have written all the data out to the jack device */
  /* this is due to xmms' audio driver api */
  char *buf = (char*)ptr;
  while(length > 0)
  {
    TRACE("writing %d bytes\n", length);
    written = JACK_Write(driver, (unsigned char*)buf, length);
    length-=written;
    buf+=written;
  }
  TRACE("finished\n");
}


/* Flush any output currently buffered */
/* and set the number of bytes written based on ms_offset_time, */
/* the number of milliseconds of offset passed in */
/* This is done so the driver itself keeps track of */
/* current playing position of the mp3 */
static void jack_flush(int ms_offset_time)
{
  TRACE("setting values for ms_offset_time of %d\n", ms_offset_time);

  JACK_Reset(driver); /* flush buffers and set state to STOPPED */

  /* update the internal driver values to correspond to the input time given */
  JACK_SetPosition(driver, MILLISECONDS, ms_offset_time);

  if (paused)
    JACK_SetState(driver, PAUSED);
  else
    JACK_SetState(driver, PLAYING);
}


/* Pause the jack device */
static void jack_pause (bool_t p)
{
  TRACE("p == %d\n", p);

  paused = p;

  /* pause the device if p is non-zero, unpause the device if p is zero and */
  /* we are currently paused */
  if(p)
    JACK_SetState(driver, PAUSED);
  else if(JACK_GetState(driver) == PAUSED)
    JACK_SetState(driver, PLAYING);
}

static const char jack_about[] =
 N_("Based on xmms-jack, by Chris Morgan:\n"
    "http://xmms-jack.sourceforge.net/\n\n"
    "Ported to Audacious by Giacomo Lozito");

#define AUD_PLUGIN_NAME        N_("JACK Output")
#define AUD_PLUGIN_ABOUT       jack_about
#define AUD_PLUGIN_INIT        jack_init
#define AUD_PLUGIN_CLEANUP     jack_cleanup
#define AUD_PLUGIN_PREFS       & jack_prefs
#define AUD_OUTPUT_GET_VOLUME  jack_get_volume
#define AUD_OUTPUT_SET_VOLUME  jack_set_volume
#define AUD_OUTPUT_OPEN        jack_open
#define AUD_OUTPUT_WRITE       jack_write
#define AUD_OUTPUT_DRAIN       NULL
#define AUD_OUTPUT_CLOSE       jack_close
#define AUD_OUTPUT_FLUSH       jack_flush
#define AUD_OUTPUT_PAUSE       jack_pause
#define AUD_OUTPUT_GET_FREE    audacious_jack_free
#define AUD_OUTPUT_WAIT_FREE   NULL
#define AUD_OUTPUT_GET_TIME    jack_get_output_time

#define AUD_DECLARE_OUTPUT
#include <libaudcore/plugin-declare.h>
