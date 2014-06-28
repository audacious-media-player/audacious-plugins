/*
 * Modplug Audacious Plugin
 *
 * Configuration GUI contributed in part by Vladimir Strakh.
 *
 * This source code is public domain.
 */

#include "plugin.h"

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>

#define MODPLUG_CFGID "modplug"

static const char * fmts[] =
    { "amf", "ams", "dbm", "dbf", "dsm", "far", "mdl", "stm", "ult", "mt2",
      "mod", "s3m", "dmf", "umx", "it", "669", "xm", "mtm", "psm", "ft2",
      nullptr };

static const char * const modplug_defaults[] = {
 "Bits", "16",
 "Channels", "2",
 "ResamplingMode", "3", /* SRCMODE_POLYPHASE */
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

static ModplugSettings modplug_settings;

static const PreferencesWidget quality_widgets[] = {
    WidgetLabel (N_("<b>Resolution</b>")),
    WidgetRadio (N_("8-bit"), WidgetInt (modplug_settings.mBits), {8}),
    WidgetRadio (N_("16-bit"), WidgetInt (modplug_settings.mBits), {16}),
    WidgetLabel (N_("<b>Channels</b>")),
    WidgetRadio (N_("Mono"), WidgetInt (modplug_settings.mChannels), {1}),
    WidgetRadio (N_("Stereo"), WidgetInt (modplug_settings.mChannels), {2}),
    WidgetLabel (N_("<b>Resampling</b>")),
    WidgetRadio (N_("Nearest (fastest)"), WidgetInt (modplug_settings.mResamplingMode), {0}),
    WidgetRadio (N_("Linear (fast)"), WidgetInt (modplug_settings.mResamplingMode), {1}),
    WidgetRadio (N_("Spline (good)"), WidgetInt (modplug_settings.mResamplingMode), {2}),
    WidgetRadio (N_("Polyphase (best)"), WidgetInt (modplug_settings.mResamplingMode), {3}),
    WidgetLabel (N_("<b>Sampling rate</b>")),
    WidgetRadio (N_("22 kHz"), WidgetInt (modplug_settings.mFrequency), {22050}),
    WidgetRadio (N_("44 kHz"), WidgetInt (modplug_settings.mFrequency), {44100}),
    WidgetRadio (N_("48 kHz"), WidgetInt (modplug_settings.mFrequency), {48000}),
    WidgetRadio (N_("96 kHz"), WidgetInt (modplug_settings.mFrequency), {96000})
};

static const PreferencesWidget reverb_fields[] = {
    WidgetSpin (N_("Level:"), WidgetInt (modplug_settings.mReverbDepth), {0, 100, 1, "%"}),
    WidgetSpin (N_("Delay:"), WidgetInt (modplug_settings.mReverbDelay), {40, 200, 1, N_("ms")})
};

static const PreferencesWidget bass_fields[] = {
    WidgetSpin (N_("Level:"), WidgetInt (modplug_settings.mBassAmount), {0, 100, 1, "%"}),
    WidgetSpin (N_("Cutoff:"), WidgetInt (modplug_settings.mBassRange), {10, 100, 1, N_("Hz")})
};

static const PreferencesWidget surround_fields[] = {
    WidgetSpin (N_("Level:"), WidgetInt (modplug_settings.mSurroundDepth), {0, 100, 1, "%"}),
    WidgetSpin (N_("Delay:"), WidgetInt (modplug_settings.mSurroundDelay), {5,  40, 1, N_("ms")})
};

static const PreferencesWidget preamp_fields[] = {
    WidgetSpin (N_("Volume:"), WidgetFloat (modplug_settings.mPreampLevel), {-3, 3, 0.1}),
};

static const PreferencesWidget effects_widgets[] = {
    WidgetLabel (N_("<b>Reverb</b>")),
    WidgetCheck (N_("Enable"), WidgetBool (modplug_settings.mReverb)),
    WidgetTable ({reverb_fields, ARRAY_LEN (reverb_fields)}, WIDGET_CHILD),
    WidgetLabel (N_("<b>Bass Boost</b>")),
    WidgetCheck (N_("Enable"), WidgetBool (modplug_settings.mMegabass)),
    WidgetTable ({bass_fields, ARRAY_LEN (bass_fields)}, WIDGET_CHILD),
    WidgetLabel (N_("<b>Surround</b>")),
    WidgetCheck (N_("Enable"), WidgetBool (modplug_settings.mSurround)),
    WidgetTable ({surround_fields, ARRAY_LEN (surround_fields)}, WIDGET_CHILD),
    WidgetLabel (N_("<b>Preamp</b>")),
    WidgetCheck (N_("Enable"), WidgetBool (modplug_settings.mPreamp)),
    WidgetTable ({preamp_fields, ARRAY_LEN (preamp_fields)}, WIDGET_CHILD)
};

static const PreferencesWidget misc_widgets[] = {
    WidgetLabel (N_("<b>Miscellaneous</b>")),
    WidgetCheck (N_("Oversample"), WidgetBool (modplug_settings.mOversamp)),
    WidgetCheck (N_("Noise reduction"), WidgetBool (modplug_settings.mNoiseReduction)),
    WidgetCheck (N_("Play Amiga MODs"), WidgetBool (modplug_settings.mGrabAmigaMOD)),
    WidgetLabel (N_("<b>Repeat</b>")),
    WidgetSpin (N_("Repeat count:"), WidgetInt (modplug_settings.mLoopCount), {-1, 100, 1}),
    WidgetLabel (N_("To repeat forever, set the repeat count to -1."))
};

static const PreferencesWidget modplug_columns[] = {
    WidgetBox ({quality_widgets, ARRAY_LEN (quality_widgets)}),
    WidgetSeparator (),
    WidgetBox ({effects_widgets, ARRAY_LEN (effects_widgets)}),
    WidgetSeparator (),
    WidgetBox ({misc_widgets, ARRAY_LEN (misc_widgets)})
};

static const PreferencesWidget modplug_widgets[] = {
    WidgetBox ({modplug_columns, ARRAY_LEN (modplug_columns), true})
};

static void modplug_settings_load ()
{
    aud_config_set_defaults (MODPLUG_CFGID, modplug_defaults);

    modplug_settings.mBits = aud_get_int (MODPLUG_CFGID, "Bits");
    modplug_settings.mChannels = aud_get_int (MODPLUG_CFGID, "Channels");
    modplug_settings.mResamplingMode = aud_get_int (MODPLUG_CFGID, "ResamplingMode");
    modplug_settings.mFrequency = aud_get_int (MODPLUG_CFGID, "Frequency");

    modplug_settings.mReverb = aud_get_bool (MODPLUG_CFGID, "Reverb");
    modplug_settings.mReverbDepth = aud_get_int (MODPLUG_CFGID, "ReverbDepth");
    modplug_settings.mReverbDelay = aud_get_int (MODPLUG_CFGID, "ReverbDelay");

    modplug_settings.mMegabass = aud_get_bool (MODPLUG_CFGID, "Megabass");
    modplug_settings.mBassAmount = aud_get_int (MODPLUG_CFGID, "BassAmount");
    modplug_settings.mBassRange = aud_get_int (MODPLUG_CFGID, "BassRange");

    modplug_settings.mSurround = aud_get_bool (MODPLUG_CFGID, "Surround");
    modplug_settings.mSurroundDepth = aud_get_int (MODPLUG_CFGID, "SurroundDepth");
    modplug_settings.mSurroundDelay = aud_get_int (MODPLUG_CFGID, "SurroundDelay");

    modplug_settings.mPreamp = aud_get_bool (MODPLUG_CFGID, "PreAmp");
    modplug_settings.mPreampLevel = aud_get_double (MODPLUG_CFGID, "PreAmpLevel");

    modplug_settings.mOversamp = aud_get_bool (MODPLUG_CFGID, "Oversampling");
    modplug_settings.mNoiseReduction = aud_get_bool (MODPLUG_CFGID, "NoiseReduction");
    modplug_settings.mGrabAmigaMOD = aud_get_bool (MODPLUG_CFGID, "GrabAmigaMOD");
    modplug_settings.mLoopCount = aud_get_int (MODPLUG_CFGID, "LoopCount");
}

static void modplug_settings_save ()
{
    aud_set_int (MODPLUG_CFGID, "Bits", modplug_settings.mBits);
    aud_set_int (MODPLUG_CFGID, "Channels", modplug_settings.mChannels);
    aud_set_int (MODPLUG_CFGID, "ResamplingMode", modplug_settings.mResamplingMode);
    aud_set_int (MODPLUG_CFGID, "Frequency", modplug_settings.mFrequency);

    aud_set_bool (MODPLUG_CFGID, "Reverb", modplug_settings.mReverb);
    aud_set_int (MODPLUG_CFGID, "ReverbDepth", modplug_settings.mReverbDepth);
    aud_set_int (MODPLUG_CFGID, "ReverbDelay", modplug_settings.mReverbDelay);

    aud_set_bool (MODPLUG_CFGID, "Megabass", modplug_settings.mMegabass);
    aud_set_int (MODPLUG_CFGID, "BassAmount", modplug_settings.mBassAmount);
    aud_set_int (MODPLUG_CFGID, "BassRange", modplug_settings.mBassRange);

    aud_set_bool (MODPLUG_CFGID, "Surround", modplug_settings.mSurround);
    aud_set_int (MODPLUG_CFGID, "SurroundDepth", modplug_settings.mSurroundDepth);
    aud_set_int (MODPLUG_CFGID, "SurroundDelay", modplug_settings.mSurroundDelay);

    aud_set_bool (MODPLUG_CFGID, "PreAmp", modplug_settings.mPreamp);
    aud_set_double (MODPLUG_CFGID, "PreAmpLevel", modplug_settings.mPreampLevel);

    aud_set_bool (MODPLUG_CFGID, "Oversampling", modplug_settings.mOversamp);
    aud_set_bool (MODPLUG_CFGID, "NoiseReduction", modplug_settings.mNoiseReduction);
    aud_set_bool (MODPLUG_CFGID, "GrabAmigaMOD", modplug_settings.mGrabAmigaMOD);
    aud_set_int (MODPLUG_CFGID, "LoopCount", modplug_settings.mLoopCount);
}

static bool modplug_init (void)
{
    modplug_settings_load ();
    InitSettings (& modplug_settings);
    return true;
}

static void modplug_settings_apply (void)
{
    InitSettings (& modplug_settings);
    modplug_settings_save ();
}

static const PluginPreferences modplug_prefs = {
    modplug_widgets,
    ARRAY_LEN (modplug_widgets),
    modplug_settings_load,
    modplug_settings_apply
};

#define AUD_PLUGIN_NAME        N_("ModPlug (Module Player)")
#define AUD_PLUGIN_PREFS       &modplug_prefs
#define AUD_PLUGIN_INIT        modplug_init
#define AUD_INPUT_PLAY         PlayFile
#define AUD_INPUT_READ_TUPLE   GetSongTuple
#define AUD_INPUT_IS_OUR_FILE  CanPlayFileFromVFS
#define AUD_INPUT_EXTS         fmts
#define AUD_INPUT_SUBTUNES     true

#define AUD_DECLARE_INPUT
#include <libaudcore/plugin-declare.h>
