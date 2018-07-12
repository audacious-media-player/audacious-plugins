/*
 * CoreAudio Output Plugin for Audacious
 * Copyright 2014 William Pitcock
 *
 * Based on:
 * SDL Output Plugin for Audacious
 * Copyright 2010 John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>
#include <libaudcore/ringbuf.h>
#include <libaudcore/i18n.h>
#include <libaudcore/preferences.h>
#include <libaudcore/drct.h>

#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>

#include "audiodevice.h"

class CoreAudioPlugin : public OutputPlugin {
public:
    static const char about[];
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("CoreAudio output"),
        PACKAGE,
        about,
        & prefs,
    };

    constexpr CoreAudioPlugin () :
        OutputPlugin (info, 5)
    {}

    bool init ();
    void cleanup ();
    void handle_new_default_device(AudioObjectID newID);

    StereoVolume get_volume ();
    void set_volume (StereoVolume vol);

    bool open_audio (int format, int rate_, int chan_, String & err);
    void close_audio ();

    void period_wait ();
    int write_audio (const void * data, int len);
    void drain ();

    int get_delay ();
    void pause (bool paused);
    void flush ();

protected:
    static OSStatus callback (void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData);
    static OSStatus callback_noop (void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData)
    {
        return notOpenErr;
    }

    void check_started ();

    bool set_device_samplerate(Float64 rate = 0.0);
    AudioDevice *coreAudioDevice = nullptr;
    bool caDeviceInitialised = false;
};

EXPORT CoreAudioPlugin aud_plugin_instance;

#define VOLUME_RANGE 40 /* decibels */

#define error(...) do { \
    aud_ui_show_error (str_printf ("CoreAudio error: " __VA_ARGS__)); \
} while (0)

static void callDefaultDeviceChangeHandler(AudioObjectID newID, void *data)
{
    CoreAudioPlugin *cap = static_cast<CoreAudioPlugin*>(data);
    if (cap)
    {
        cap->handle_new_default_device(newID);
    }
}

struct AudioUnitFormatDescriptionMap {
    int aud_format;
    unsigned int mBitsPerChannel;
    unsigned int mBytesPerChannel;
    unsigned int mFormatFlags;
};
static struct AudioUnitFormatDescriptionMap AUFormatMap[] = {
    {FMT_S16_LE, 16, sizeof (int16_t), kAudioFormatFlagIsSignedInteger},
    {FMT_S16_BE, 16, sizeof (int16_t), kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsBigEndian},
    {FMT_U16_LE, 16, sizeof (int16_t), 0},
    {FMT_U16_BE, 16, sizeof (int16_t), kAudioFormatFlagIsBigEndian},
    {FMT_S24_LE, 24, sizeof (int32_t), kAudioFormatFlagIsSignedInteger},
    {FMT_S24_BE, 24, sizeof (int32_t), kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsBigEndian},
    {FMT_U24_LE, 24, sizeof (int32_t), 0},
    {FMT_U24_BE, 24, sizeof (int32_t), kAudioFormatFlagIsBigEndian},
    {FMT_S32_LE, 32, sizeof (int32_t), kAudioFormatFlagIsSignedInteger},
    {FMT_S32_BE, 32, sizeof (int32_t), kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsBigEndian},
    {FMT_U32_LE, 32, sizeof (int32_t), 0},
    {FMT_U32_BE, 32, sizeof (int32_t), kAudioFormatFlagIsBigEndian},
    {FMT_FLOAT,  32, sizeof (float),   kAudioFormatFlagIsFloat},
};

static RingBuf <unsigned char> buffer;
static int vol_left = 0, vol_right = 0;
static int chan = 0;
static int rate = 0;
static int buffer_bytes_per_channel = 0;

static bool prebuffer_flag = false;
static bool paused_flag = false;

static int block_delay = 0;
static struct timeval block_time = {0, 0};

static AudioComponent output_comp = nullptr;
// static AudioComponentInstance output_instance = nullptr;
static AudioUnit output_instance = nullptr;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static bool is_exclusive = false;

const char CoreAudioPlugin::about[] =
    N_("CoreAudio Output Plugin for Audacious\n"
       "Copyright 2014 William Pitcock\n"
       "Copyright 2017,18 René J.V. Bertin\n"
       " (original code and code adapted from\n"
       "  Apple examples, (C) Apple 2013)\n\n"
       "Based on SDL Output Plugin for Audacious\n"
       "Copyright 2010 John Lindgren");

const char * const CoreAudioPlugin::defaults[] = {
    "vol_left", "100",
    "vol_right", "100",
    "exclusive_mode", "FALSE",
    "bitperfect_mode", "FALSE",
    nullptr};

const PreferencesWidget CoreAudioPlugin::widgets[] = {
    WidgetCheck (N_("Use exclusive mode"),
        WidgetBool ("coreaudio", "exclusive_mode")),
    WidgetCheck (N_("Use bit perfect audio (adapt device sampling rate)"),
        WidgetBool ("coreaudio", "bitperfect_mode")),
};

const PluginPreferences CoreAudioPlugin::prefs = {
    {widgets},
    nullptr,
    nullptr,
    nullptr
};

bool CoreAudioPlugin::init ()
{
    aud_config_set_defaults ("coreaudio", defaults);

    vol_left = aud_get_int ("coreaudio", "vol_left");
    vol_right = aud_get_int ("coreaudio", "vol_right");

    /* open the default audio device */
    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    desc.componentManufacturer = 0;

    output_comp = AudioComponentFindNext (nullptr, & desc);
    if (! output_comp)
    {
        error ("Failed to open default audio device.\n");
        return 0;
    }

    OSStatus err = AudioComponentInstanceNew (output_comp, & output_instance);
    if (err != noErr)
    {
        error ("Failed to open default audio device (error %d).\n", err);
        return 0;
    }

    AURenderCallbackStruct input;
    input.inputProc = callback;
    input.inputProcRefCon = this;

    if (AudioUnitSetProperty (output_instance, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, & input, sizeof input))
    {
        error ("Unable to attach an IOProc to the selected audio unit.\n");
        return 0;
    }

    AudioObjectPropertyAddress prop = { kAudioHardwarePropertyDefaultOutputDevice,
                                        kAudioObjectPropertyScopeGlobal,
                                        kAudioObjectPropertyElementMaster };

    UInt32 size = sizeof(AudioObjectID);
    AudioObjectID defaultOutputDeviceID;
    err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop, 0, NULL, &size, &defaultOutputDeviceID);
    if (err == noErr)
    {
        if (coreAudioDevice) {
            // TODO: check, can this ever happen?
            coreAudioDevice->ResetNominalSampleRate();
            delete coreAudioDevice;
            coreAudioDevice = nullptr;
        }
        coreAudioDevice = AudioDevice::GetDevice(defaultOutputDeviceID, false, coreAudioDevice, true);
        if (coreAudioDevice != NULL)
        {
            coreAudioDevice->installDefaultDeviceChangeHandler(callDefaultDeviceChangeHandler, static_cast<void*>(this));
            coreAudioDevice->setDefaultDevice(true);

            set_device_samplerate();

            AUDINFO ("Using default output device #%d=\"%s\" (%p).\n",
                     defaultOutputDeviceID, coreAudioDevice->GetName(), coreAudioDevice);
        }
        else
            AUDERR ("Failed to obtain a handle on the default device (%p)\n", coreAudioDevice);
    }
    else
        AUDERR ("Failed to obtain the default device ID (%d) (no bit perfect audio).\n", err);

    return 1;
}

// combined initialisation and rate-setter:
// initialises the CoreAudio device handle if not yet done
// @p rate <= 0 : reset the device sampling rate unless the handle was NOT initialised
// @p rate > 0 : set the device sampling rate after initialisation.
bool CoreAudioPlugin::set_device_samplerate(Float64 rate)
{
    bool ret = false;
    if (coreAudioDevice)
    {
        if (!caDeviceInitialised)
        {
            // use the warning loglevel to inform the user this operation can take a while (up to a few seconds)
            AUDWARN ("Preparing the \"%s\" for bit perfect output (this can take a moment)\n", coreAudioDevice->GetName());
            coreAudioDevice->Init();
            caDeviceInitialised = true;
        }
        else if (rate <= 0)
        {
            coreAudioDevice->ResetNominalSampleRate();
        }
        ret = coreAudioDevice->SetNominalSampleRate(rate) == noErr;
    }
    return ret;
}

void CoreAudioPlugin::handle_new_default_device(AudioObjectID newID)
{
    int currentPosition = aud_drct_get_position();
    int currentTime = aud_drct_get_time();

    aud_drct_stop();

    Float64 rate = 0;
    if (coreAudioDevice)
    {
        if (coreAudioDevice->NominalSampleRate(rate) != noErr)
        {
            rate = coreAudioDevice->CurrentNominalSampleRate();
        }
    }

    AURenderCallbackStruct input = { callback_noop, nullptr };
    AudioUnitSetProperty (output_instance, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, & input, sizeof input);
    AudioUnitReset(output_instance, kAudioUnitScope_Global, 0);

    cleanup();

    AudioComponentInstanceDispose(output_instance);
    output_instance = nullptr;

    init();
    if (currentPosition > 0 && currentTime > 0)
    {
        dispatch_block_t restart_playback = ^{
            aud_drct_play();
            aud_drct_seek(currentTime);
        };
        if (coreAudioDevice && rate > 0 && aud_get_bool ("coreaudio", "bitperfect_mode"))
        {
            // set the rate before starting playback to avoid hickups
            AUDDBG ("Setting device to content's samplerate %gHz\n", rate);
            set_device_samplerate(rate);
            // give the device some time, again to avoid a hickup. A lossless
            // playback gap is less annoying than a stutter to my ears.
            // We use GCD to avoid a blocking wait (even if 400ms is hardly
            // noticeable when you're busy switching the default output device
            // via a system utility).
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.4 * NSEC_PER_SEC),
                           dispatch_get_main_queue(), restart_playback);
        }
        else
        {
            // playback can be started immediately, just invoke the block.
            restart_playback();
        }
    }
}

void CoreAudioPlugin::cleanup ()
{
    if (coreAudioDevice) {
        coreAudioDevice->ResetNominalSampleRate();
    }
    delete coreAudioDevice;
    coreAudioDevice = nullptr;
    caDeviceInitialised = false;
}

StereoVolume CoreAudioPlugin::get_volume ()
{
    return {vol_left, vol_right};
}

void CoreAudioPlugin::set_volume (StereoVolume vol)
{
    int vol_max;
    float factor;

    vol_left = vol.left;
    vol_right = vol.right;

    vol_max = aud::max (vol_left, vol_right);

    aud_set_int ("coreaudio", "vol_left", vol_left);
    aud_set_int ("coreaudio", "vol_right", vol_right);

    factor = (vol_max == 0) ? 0.0 : powf (10, (float) VOLUME_RANGE * (vol_max - 100) / 100 / 20);

    AudioUnitSetParameter (output_instance, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, factor, 0);
}

OSStatus CoreAudioPlugin::callback (void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData)
{
    pthread_mutex_lock (& mutex);

    int len = ioData->mBuffers[0].mDataByteSize;
    unsigned char * buf = (unsigned char *) ioData->mBuffers[0].mData;
    ioData->mBuffers[0].mNumberChannels = chan;

    int copy = aud::min (len, buffer.len ());

    buffer.move_out (buf, copy);
    if (copy < len)
        memset (buf + copy, 0, len - copy);

    /* At this moment, we know that there is a delay of (at least) the block of
     * data just written.  We save the block size and the current time for
     * estimating the delay later on. */
    block_delay = copy / (buffer_bytes_per_channel * chan) * 1000 / rate;
    gettimeofday (& block_time, nullptr);

    pthread_cond_broadcast (& cond);
    pthread_mutex_unlock (& mutex);

    return 0;
}

// TODO: return error message to core instead of calling aud_ui_show_error
bool CoreAudioPlugin::open_audio (int format, int rate_, int chan_, String & err)
{
    struct AudioUnitFormatDescriptionMap m;

    m.aud_format = -1;
    for (struct AudioUnitFormatDescriptionMap it : AUFormatMap)
    {
        if (it.aud_format == format)
        {
            m = it;
            break;
        }
    }

    if (m.aud_format != format)
    {
        error ("The requested audio format %d is unsupported.\n", format);
        return 0;
    }

    AUDDBG ("Opening audio for %d channels, %d Hz.\n", chan_, rate_);

    chan = chan_;
    rate = rate_;

    buffer_bytes_per_channel = m.mBytesPerChannel;

    int buffer_size = buffer_bytes_per_channel * chan * (aud_get_int (nullptr, "output_buffer_size") * rate / 1000);
    buffer.alloc (buffer_size);

    prebuffer_flag = true;
    paused_flag = false;

    OSStatus result = AudioUnitInitialize (output_instance);
    if (result != noErr)
    {
        error ("Unable to initialize audio unit instance, error=%d\n", result);
        return 0;
    }

    AudioStreamBasicDescription streamFormat;
    streamFormat.mSampleRate = rate;
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags = m.mFormatFlags;
    streamFormat.mFramesPerPacket = 1;
    streamFormat.mChannelsPerFrame = chan;
    streamFormat.mBitsPerChannel = m.mBitsPerChannel;
    streamFormat.mBytesPerPacket = chan * buffer_bytes_per_channel;
    streamFormat.mBytesPerFrame = chan * buffer_bytes_per_channel;

    AUDDBG("Stream format:\n");
    AUDDBG(" Channels: %d\n", streamFormat.mChannelsPerFrame);
    AUDDBG(" Sample rate: %f\n", streamFormat.mSampleRate);
    AUDDBG(" Bits per channel: %d\n", streamFormat.mBitsPerChannel);
    AUDDBG(" Bytes per frame: %d\n", streamFormat.mBytesPerFrame);

    result = AudioUnitSetProperty (output_instance, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &streamFormat, sizeof(streamFormat));
    if (result)
    {
        error ("Failed to set audio unit input property (stream format); error=%d.\n", result);
        buffer.destroy ();
        return 0;
    }

    bool exclusive = aud_get_bool ("coreaudio", "exclusive_mode");
    if (exclusive)
    {
        UInt32 hog_mode = 1;

        AudioObjectPropertyAddress prop;

        prop.mSelector = kAudioDevicePropertyHogMode;
        prop.mScope    = kAudioObjectPropertyScopeGlobal;
        prop.mElement  = kAudioObjectPropertyElementMaster;

        OSStatus result = AudioObjectSetPropertyData (kAudioObjectSystemObject, & prop, 0, nullptr, sizeof hog_mode, & hog_mode);
        if (result != noErr)
        {
            AUDWARN ("Failed to open device for exclusive mode, continuing anyway... [%d]\n", result);
            is_exclusive = false;
        }
        else
            is_exclusive = true;
    }

    if (coreAudioDevice && aud_get_bool ("coreaudio", "bitperfect_mode") && rate != coreAudioDevice->CurrentNominalSampleRate())
    {
        AUDDBG ("Setting device to content's samplerate %dHz\n", rate);
        set_device_samplerate(Float64(rate));
    }

    if (AudioOutputUnitStart (output_instance))
    {
        error ("Unable to start audio unit.\n");
        buffer.destroy ();
        return 0;
    }

    return 1;
}

void CoreAudioPlugin::close_audio ()
{
    AUDDBG ("Closing audio.\n");

    AudioOutputUnitStop (output_instance);

    if (coreAudioDevice) {
        // always restore the device samplerate when we're done playing
        // (it's a noop when there's nothing to reset)
        coreAudioDevice->ResetNominalSampleRate();
    }

    if (is_exclusive)
    {
        UInt32 hog_mode = 0;

        AudioObjectPropertyAddress prop;

        prop.mSelector = kAudioDevicePropertyHogMode;
        prop.mScope    = kAudioObjectPropertyScopeGlobal;
        prop.mElement  = kAudioObjectPropertyElementMaster;

        OSStatus err = AudioObjectSetPropertyData (kAudioObjectSystemObject, & prop, 0, nullptr, sizeof hog_mode, & hog_mode);
        if (err != noErr)
            AUDWARN ("Failed to release device from exclusive mode, continuing anyway...");

        is_exclusive = false;
    }

    buffer.destroy ();
}

void CoreAudioPlugin::check_started ()
{
    if (! prebuffer_flag)
        return;

    AUDDBG ("Starting playback.\n");
    prebuffer_flag = false;
    block_delay = 0;
}

void CoreAudioPlugin::period_wait ()
{
    pthread_mutex_lock (& mutex);

    while (! buffer.space ())
    {
        if (! paused_flag)
            check_started ();

        pthread_cond_wait (& cond, & mutex);
    }

    pthread_mutex_unlock (& mutex);
}

int CoreAudioPlugin::write_audio (const void * data, int len)
{
    pthread_mutex_lock (& mutex);

    len = aud::min (len, buffer.space ());
    buffer.copy_in ((const unsigned char *) data, len);

    pthread_mutex_unlock (& mutex);
    return len;
}

void CoreAudioPlugin::drain ()
{
    AUDDBG ("Draining.\n");
    pthread_mutex_lock (& mutex);

    check_started ();

    while (buffer.len ())
        pthread_cond_wait (& cond, & mutex);

    pthread_mutex_unlock (& mutex);
}

int CoreAudioPlugin::get_delay ()
{
    auto timediff = [] (const timeval & a, const timeval & b) -> int64_t
        { return 1000 * (int64_t) (b.tv_sec - a.tv_sec) + (b.tv_usec - a.tv_usec) / 1000; };

    pthread_mutex_lock (& mutex);

    int delay = aud::rescale (buffer.len (), buffer_bytes_per_channel * chan * rate, 1000);

    /* Estimate the additional delay of the last block written. */
    if (! prebuffer_flag && ! paused_flag && block_delay)
    {
        struct timeval cur;
        gettimeofday (& cur, nullptr);

        delay += aud::max (block_delay - timediff (block_time, cur), (int64_t) 0);
    }

    pthread_mutex_unlock (& mutex);
    return delay;
}

void CoreAudioPlugin::pause (bool pause)
{
    AUDDBG ("%sause.\n", pause ? "P" : "Unp");
    pthread_mutex_lock (& mutex);

    paused_flag = pause;

    if (pause)
        AudioOutputUnitStop (output_instance);
    else
    {
        if (AudioOutputUnitStart (output_instance))
        {
            error ("Unable to restart audio unit after pausing.\n");
            close_audio ();
        }
    }

    pthread_cond_broadcast (& cond); /* wake up period wait */
    pthread_mutex_unlock (& mutex);
}

void CoreAudioPlugin::flush ()
{
    AUDDBG ("Seek requested; discarding buffer.\n");
    pthread_mutex_lock (& mutex);

    buffer.discard ();

    prebuffer_flag = true;

    pthread_cond_broadcast (& cond); /* wake up period wait */
    pthread_mutex_unlock (& mutex);
}
