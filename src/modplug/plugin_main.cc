/*
 * Modplug Audacious Plugin
 *
 * Configuration GUI contributed in part by Vladimir Strakh.
 *
 * This source code is public domain.
 */

#include "modplugbmp.h"

#include <libmodplug/stdafx.h>
#include <libmodplug/sndfile.h>

#include <libaudcore/runtime.h>
#include <libaudcore/preferences.h>

#define MODPLUG_CFGID "modplug"

EXPORT ModplugXMMS aud_plugin_instance;

const char * const ModplugXMMS::exts[] =
    { "amf", "ams", "dbm", "dbf", "dsm", "far", "mdl", "stm", "ult", "mt2",
      "mod", "s3m", "dmf", "umx", "it", "669", "xm", "mtm", "psm", "ft2",
      nullptr };

const char * const ModplugXMMS::defaults[] = {
 "Bits", "16",
 "Channels", "2",
 "ResamplingMode", aud::numeric_string<SRCMODE_POLYPHASE>::str,
 "Frequency", "44100",

 "Reverb", "FALSE",
 "ReverbDepth", "30",
 "ReverbDelay", "100",

 "Megabass", "FALSE",
 "BassAmount", "40",
 "BassRange", "30",

 "Surround", "TRUE",
 "SurroundDepth", "20",
 "SurroundDelay", "20",

 "PreAmp", "FALSE",
 "PreAmpLevel", "0",

 "Oversampling", "TRUE",
 "NoiseReduction", "TRUE",
 "GrabAmigaMOD", "TRUE",
 "LoopCount", "0",

 nullptr
};

static const PreferencesWidget quality_widgets[] = {
    WidgetLabel (N_("<b>Resolution</b>")),
    WidgetRadio (N_("8-bit"), WidgetInt (MODPLUG_CFGID, "Bits"), {8}),
    WidgetRadio (N_("16-bit"), WidgetInt (MODPLUG_CFGID, "Bits"), {16}),
    WidgetLabel (N_("<b>Channels</b>")),
    WidgetRadio (N_("Mono"), WidgetInt (MODPLUG_CFGID, "Channels"), {1}),
    WidgetRadio (N_("Stereo"), WidgetInt (MODPLUG_CFGID, "Channels"), {2}),
    WidgetLabel (N_("<b>Resampling</b>")),
    WidgetRadio (N_("Nearest (fastest)"), WidgetInt (MODPLUG_CFGID, "ResamplingMode"), {0}),
    WidgetRadio (N_("Linear (fast)"), WidgetInt (MODPLUG_CFGID, "ResamplingMode"), {1}),
    WidgetRadio (N_("Spline (good)"), WidgetInt (MODPLUG_CFGID, "ResamplingMode"), {2}),
    WidgetRadio (N_("Polyphase (best)"), WidgetInt (MODPLUG_CFGID, "ResamplingMode"), {3}),
    WidgetLabel (N_("<b>Sample rate</b>")),
    WidgetRadio (N_("22 kHz"), WidgetInt (MODPLUG_CFGID, "Frequency"), {22050}),
    WidgetRadio (N_("44 kHz"), WidgetInt (MODPLUG_CFGID, "Frequency"), {44100}),
    WidgetRadio (N_("48 kHz"), WidgetInt (MODPLUG_CFGID, "Frequency"), {48000}),
    WidgetRadio (N_("96 kHz"), WidgetInt (MODPLUG_CFGID, "Frequency"), {96000})
};

static const PreferencesWidget reverb_fields[] = {
    WidgetSpin (N_("Level:"), WidgetInt (MODPLUG_CFGID, "ReverbDepth"), {0, 100, 1, "%"}),
    WidgetSpin (N_("Delay:"), WidgetInt (MODPLUG_CFGID, "ReverbDelay"), {40, 200, 1, N_("ms")})
};

static const PreferencesWidget bass_fields[] = {
    WidgetSpin (N_("Level:"), WidgetInt (MODPLUG_CFGID, "BassAmount"), {0, 100, 1, "%"}),
    WidgetSpin (N_("Cutoff:"), WidgetInt (MODPLUG_CFGID, "BassRange"), {10, 100, 1, N_("Hz")})
};

static const PreferencesWidget surround_fields[] = {
    WidgetSpin (N_("Level:"), WidgetInt (MODPLUG_CFGID, "SurroundDepth"), {0, 100, 1, "%"}),
    WidgetSpin (N_("Delay:"), WidgetInt (MODPLUG_CFGID, "SurroundDelay"), {5,  40, 1, N_("ms")})
};

static const PreferencesWidget preamp_fields[] = {
    WidgetSpin (N_("Volume:"), WidgetFloat (MODPLUG_CFGID, "PreampLevel"), {-3, 3, 0.1}),
};

static const PreferencesWidget effects_widgets[] = {
    WidgetLabel (N_("<b>Reverb</b>")),
    WidgetCheck (N_("Enable"), WidgetBool (MODPLUG_CFGID, "Reverb")),
    WidgetTable ({{reverb_fields}}, WIDGET_CHILD),
    WidgetLabel (N_("<b>Bass Boost</b>")),
    WidgetCheck (N_("Enable"), WidgetBool (MODPLUG_CFGID, "Megabass")),
    WidgetTable ({{bass_fields}}, WIDGET_CHILD),
    WidgetLabel (N_("<b>Surround</b>")),
    WidgetCheck (N_("Enable"), WidgetBool (MODPLUG_CFGID, "Surround")),
    WidgetTable ({{surround_fields}}, WIDGET_CHILD),
    WidgetLabel (N_("<b>Preamp</b>")),
    WidgetCheck (N_("Enable"), WidgetBool (MODPLUG_CFGID, "Preamp")),
    WidgetTable ({{preamp_fields}}, WIDGET_CHILD)
};

static const PreferencesWidget misc_widgets[] = {
    WidgetLabel (N_("<b>Miscellaneous</b>")),
    WidgetCheck (N_("Oversample"), WidgetBool (MODPLUG_CFGID, "Oversamp")),
    WidgetCheck (N_("Noise reduction"), WidgetBool (MODPLUG_CFGID, "NoiseReduction")),
    WidgetCheck (N_("Play Amiga MODs"), WidgetBool (MODPLUG_CFGID, "GrabAmigaMOD")),
    WidgetLabel (N_("<b>Repeat</b>")),
    WidgetSpin (N_("Repeat count:"), WidgetInt (MODPLUG_CFGID, "LoopCount"), {-1, 100, 1}),
    WidgetLabel (N_("To repeat forever, set the repeat count to -1."))
};

static const PreferencesWidget widget_columns[] = {
    WidgetBox ({{quality_widgets}}),
    WidgetSeparator (),
    WidgetBox ({{effects_widgets}}),
    WidgetSeparator (),
    WidgetBox ({{misc_widgets}})
};

const PreferencesWidget ModplugXMMS::widgets[] = {
    WidgetBox ({{widget_columns}, true}),
    WidgetLabel (N_("These settings will take effect when Audacious is restarted."))
};

const PluginPreferences ModplugXMMS::prefs = {{widgets}};

void ModplugXMMS::load_settings ()
{
    aud_config_set_defaults (MODPLUG_CFGID, defaults);

    mModProps.mBits = aud_get_int (MODPLUG_CFGID, "Bits");
    mModProps.mChannels = aud_get_int (MODPLUG_CFGID, "Channels");
    mModProps.mResamplingMode = aud_get_int (MODPLUG_CFGID, "ResamplingMode");
    mModProps.mFrequency = aud_get_int (MODPLUG_CFGID, "Frequency");

    mModProps.mReverb = aud_get_bool (MODPLUG_CFGID, "Reverb");
    mModProps.mReverbDepth = aud_get_int (MODPLUG_CFGID, "ReverbDepth");
    mModProps.mReverbDelay = aud_get_int (MODPLUG_CFGID, "ReverbDelay");

    mModProps.mMegabass = aud_get_bool (MODPLUG_CFGID, "Megabass");
    mModProps.mBassAmount = aud_get_int (MODPLUG_CFGID, "BassAmount");
    mModProps.mBassRange = aud_get_int (MODPLUG_CFGID, "BassRange");

    mModProps.mSurround = aud_get_bool (MODPLUG_CFGID, "Surround");
    mModProps.mSurroundDepth = aud_get_int (MODPLUG_CFGID, "SurroundDepth");
    mModProps.mSurroundDelay = aud_get_int (MODPLUG_CFGID, "SurroundDelay");

    mModProps.mPreamp = aud_get_bool (MODPLUG_CFGID, "PreAmp");
    mModProps.mPreampLevel = aud_get_double (MODPLUG_CFGID, "PreAmpLevel");

    mModProps.mOversamp = aud_get_bool (MODPLUG_CFGID, "Oversampling");
    mModProps.mNoiseReduction = aud_get_bool (MODPLUG_CFGID, "NoiseReduction");
    mModProps.mGrabAmigaMOD = aud_get_bool (MODPLUG_CFGID, "GrabAmigaMOD");
    mModProps.mLoopCount = aud_get_int (MODPLUG_CFGID, "LoopCount");
}
