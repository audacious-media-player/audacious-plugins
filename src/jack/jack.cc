/*      xmms - jack output plugin
 *    Copyright 2002 Chris Morgan<cmorgan@alum.wpi.edu>
 *
 *      audacious port (2005-2006) by Giacomo Lozito from develia.org
 *
 *    This code maps xmms calls into the jack translation library
 */

#include <pthread.h>
#include <string.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>

#include "bio2jack.h" /* includes for the bio2jack library */

/* set to 1 for verbose output */
#define VERBOSE_OUTPUT          0

#define TRACE AUDDBG
#define ERR AUDDBG

class JACKOutput : public OutputPlugin
{
public:
    static const char about[];
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("JACK Output"),
        PACKAGE,
        about,
        & prefs
    };

    constexpr JACKOutput () : OutputPlugin (info, 0) {}

    bool init ();

    StereoVolume get_volume ();
    void set_volume (StereoVolume v);

    bool open_audio (int fmt, int sample_rate, int num_channels);
    void close_audio ();

    int buffer_free ();
    void period_wait ();
    void write_audio (const void * ptr, int size);
    void drain () {} // TODO

    int output_time ();

    void pause (bool pause);
    void flush (int time);
};

EXPORT JACKOutput aud_plugin_instance;

static bool paused, flushed;

static pthread_mutex_t free_space_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t free_space_cond = PTHREAD_COND_INITIALIZER;


/* Set the volume */
void JACKOutput::set_volume (StereoVolume v)
{
    aud_set_int ("jack", "volume_left", v.left);
    aud_set_int ("jack", "volume_right", v.right);

    JACK_SetVolumeForChannel (0, v.left);
    JACK_SetVolumeForChannel (1, v.right);
}


/* Get the current volume setting */
StereoVolume JACKOutput::get_volume ()
{
    return {aud_get_int ("jack", "volume_left"), aud_get_int ("jack", "volume_right")};
}


/* Return the current number of milliseconds of audio data that has */
/* been played out of the audio device, not including the buffer */
int JACKOutput::output_time()
{
  int return_val;

  /* don't try to get any values if the device is still closed */
  if(JACK_GetState() == CLOSED)
    return_val = 0;
  else
    return_val = JACK_GetPosition(); /* get played position in terms of milliseconds */

  TRACE("returning %d milliseconds\n", return_val);
  return return_val;
}


void jack_set_port_connection_mode()
{
  /* setup the port connection mode that determines how bio2jack will connect ports */
  String mode_str = aud_get_str ("jack", "port_connection_mode");
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
}

const char * const JACKOutput::defaults[] = {
    "port_connection_mode", "CONNECT_ALL",
    "volume_left", "100",
    "volume_right", "100",
    nullptr
};

static const ComboItem mode_list[] = {
    ComboItem (N_("Connect to all available jack ports"), "CONNECT_ALL"),
    ComboItem (N_("Connect only the output ports"), "CONNECT_OUTPUT"),
    ComboItem (N_("Don't connect to any port"), "CONNECT_NONE")
};

const PreferencesWidget JACKOutput::widgets[] = {
    WidgetCombo (N_("Connection mode:"),
        WidgetString ("jack", "port_connection_mode"),
        {{mode_list}})
};

const PluginPreferences JACKOutput::prefs = {{JACKOutput::widgets}};

/* Initialize necessary things */
bool JACKOutput::init ()
{
  aud_config_set_defaults ("jack", defaults);

  TRACE("initializing\n");
  JACK_Init(); /* initialize the driver */

  /* set the port connection mode */
  jack_set_port_connection_mode();

  /* Always return OK, as we don't know about physical devices here */
  return true;
}


/* Return the amount of data that can be written to the device */
int JACKOutput::buffer_free()
{
  int return_val = JACK_GetBytesFreeSpace();
  TRACE("free space of %lu bytes\n", return_val);
  return return_val;
}


/* Called when more space is available in the buffer */
static void jack_free_space_notify()
{
  pthread_mutex_lock(&free_space_mutex);
  pthread_cond_broadcast(&free_space_cond);
  pthread_mutex_unlock(&free_space_mutex);
}


/* Sleeps until more space is available in the buffer */
void JACKOutput::period_wait()
{
  pthread_mutex_lock(&free_space_mutex);

  while(!flushed && !JACK_GetBytesFreeSpace())
    pthread_cond_wait(&free_space_cond, &free_space_mutex);

  pthread_mutex_unlock(&free_space_mutex);
}


/* Close the device */
void JACKOutput::close_audio()
{
  TRACE("close\n");
  JACK_Close();
}


/* Open the device up */
bool JACKOutput::open_audio(int fmt, int sample_rate, int num_channels)
{
  TRACE("fmt == %d, sample_rate == %d, num_channels == %d\n",
    fmt, sample_rate, num_channels);

  if(fmt != FMT_FLOAT)
  {
    aud_ui_show_error(_("JACK error: You must change the output sample format "
     "to floating-point."));
    return false;
  }

  /* try to open the jack device */
  if(!JACK_Open(sample_rate, num_channels, jack_free_space_notify))
    return false;

  /* set the volume to stored value */
  set_volume(get_volume());

  paused = false;
  flushed = true;

  return true;
}


/* write some audio out to the device */
void JACKOutput::write_audio(const void *ptr, int length)
{
  long written;

  TRACE("starting length of %d\n", length);

  /* loop until we have written all the data out to the jack device */
  auto buf = (const char *)ptr;
  while(length > 0)
  {
    TRACE("writing %d bytes\n", length);
    written = JACK_Write(buf, length);
    length-=written;
    buf+=written;
  }
  TRACE("finished\n");

  pthread_mutex_lock(&free_space_mutex);
  flushed = false;
  pthread_mutex_unlock(&free_space_mutex);
}


/* Flush any output currently buffered */
/* and set the number of bytes written based on ms_offset_time, */
/* the number of milliseconds of offset passed in */
/* This is done so the driver itself keeps track of */
/* current playing position of the mp3 */
void JACKOutput::flush(int ms_offset_time)
{
  TRACE("setting values for ms_offset_time of %d\n", ms_offset_time);

  JACK_Reset(); /* flush buffers and set state to STOPPED */

  /* update the internal driver values to correspond to the input time given */
  JACK_SetPosition(ms_offset_time);

  if (paused)
    JACK_SetState(PAUSED);
  else
    JACK_SetState(PLAYING);

  /* wake up period_wait() */
  pthread_mutex_lock(&free_space_mutex);
  flushed = true;
  pthread_cond_broadcast(&free_space_cond);
  pthread_mutex_unlock(&free_space_mutex);
}


/* Pause the jack device */
void JACKOutput::pause (bool p)
{
  TRACE("p == %d\n", p);

  paused = p;

  /* pause the device if p is non-zero, unpause the device if p is zero and */
  /* we are currently paused */
  if(p)
    JACK_SetState(PAUSED);
  else if(JACK_GetState() == PAUSED)
    JACK_SetState(PLAYING);
}

const char JACKOutput::about[] =
 N_("Based on xmms-jack, by Chris Morgan:\n"
    "http://xmms-jack.sourceforge.net/\n\n"
    "Ported to Audacious by Giacomo Lozito");
