/*
 * Modplug Audacious Plugin
 *
 * Configuration GUI contributed in part by Vladimir Strakh.
 *
 * This source code is public domain.
 */

#include "plugin.h"

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <audacious/preferences.h>

#define MODPLUG_CFGID "modplug"

static const char * fmts[] =
    { "amf", "ams", "dbm", "dbf", "dsm", "far", "mdl", "stm", "ult", "mt2",
      "mod", "s3m", "dmf", "umx", "it", "669", "xm", "mtm", "psm", "ft2",
      NULL };

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

 NULL
};

static ModplugSettings modplug_settings;

static const PreferencesWidget quality_widgets[] = {
    {WIDGET_LABEL, N_("<b>Resolution</b>")},
    {WIDGET_RADIO_BTN, N_("8-bit"), .cfg_type = VALUE_INT,
     .cfg = & modplug_settings.mBits, .data = {.radio_btn = {8}}},
    {WIDGET_RADIO_BTN, N_("16-bit"), .cfg_type = VALUE_INT,
     .cfg = & modplug_settings.mBits, .data = {.radio_btn = {16}}},
    {WIDGET_LABEL, N_("<b>Channels</b>")},
    {WIDGET_RADIO_BTN, N_("Mono"), .cfg_type = VALUE_INT,
     .cfg = & modplug_settings.mChannels, .data = {.radio_btn = {1}}},
    {WIDGET_RADIO_BTN, N_("Stereo"), .cfg_type = VALUE_INT,
     .cfg = & modplug_settings.mChannels, .data = {.radio_btn = {2}}},
    {WIDGET_LABEL, N_("<b>Resampling</b>")},
    {WIDGET_RADIO_BTN, N_("Nearest (fastest)"), .cfg_type = VALUE_INT,
     .cfg = & modplug_settings.mResamplingMode, .data = {.radio_btn = {0}}},
    {WIDGET_RADIO_BTN, N_("Linear (fast)"), .cfg_type = VALUE_INT,
     .cfg = & modplug_settings.mResamplingMode, .data = {.radio_btn = {1}}},
    {WIDGET_RADIO_BTN, N_("Spline (good)"), .cfg_type = VALUE_INT,
     .cfg = & modplug_settings.mResamplingMode, .data = {.radio_btn = {2}}},
    {WIDGET_RADIO_BTN, N_("Polyphase (best)"), .cfg_type = VALUE_INT,
     .cfg = & modplug_settings.mResamplingMode, .data = {.radio_btn = {3}}},
    {WIDGET_LABEL, N_("<b>Sampling rate</b>")},
    {WIDGET_RADIO_BTN, N_("22 kHz"), .cfg_type = VALUE_INT,
     .cfg = & modplug_settings.mFrequency, .data = {.radio_btn = {22050}}},
    {WIDGET_RADIO_BTN, N_("44 kHz"), .cfg_type = VALUE_INT,
     .cfg = & modplug_settings.mFrequency, .data = {.radio_btn = {44100}}},
    {WIDGET_RADIO_BTN, N_("48 kHz"), .cfg_type = VALUE_INT,
     .cfg = & modplug_settings.mFrequency, .data = {.radio_btn = {48000}}},
    {WIDGET_RADIO_BTN, N_("96 kHz"), .cfg_type = VALUE_INT,
     .cfg = & modplug_settings.mFrequency, .data = {.radio_btn = {96000}}}
};

static PreferencesWidget reverb_fields[] = {
    {WIDGET_SPIN_BTN, N_("Level:"), .cfg_type = VALUE_INT,
     .cfg = & modplug_settings.mReverbDepth, .data = {.spin_btn = {0, 100, 1, "%"}}},
    {WIDGET_SPIN_BTN, N_("Delay:"), .cfg_type = VALUE_INT,
     .cfg = & modplug_settings.mReverbDelay, .data = {.spin_btn = {40, 200, 1, N_("ms")}}}
};

static PreferencesWidget bass_fields[] = {
    {WIDGET_SPIN_BTN, N_("Level:"), .cfg_type = VALUE_INT,
     .cfg = & modplug_settings.mBassAmount, .data = {.spin_btn = {0, 100, 1, "%"}}},
    {WIDGET_SPIN_BTN, N_("Cutoff:"), .cfg_type = VALUE_INT,
     .cfg = & modplug_settings.mBassRange, .data = {.spin_btn = {10, 100, 1, N_("Hz")}}}
};

static PreferencesWidget surround_fields[] = {
    {WIDGET_SPIN_BTN, N_("Level:"), .cfg_type = VALUE_INT,
     .cfg = & modplug_settings.mSurroundDepth, .data = {.spin_btn = {0, 100, 1, "%"}}},
    {WIDGET_SPIN_BTN, N_("Delay:"), .cfg_type = VALUE_INT,
     .cfg = & modplug_settings.mSurroundDelay, .data = {.spin_btn = {5,  40, 1, N_("ms")}}}
};

static PreferencesWidget preamp_fields[] = {
    {WIDGET_SPIN_BTN, N_("Volume:"), .cfg_type = VALUE_FLOAT,
     .cfg = & modplug_settings.mPreampLevel, .data = {.spin_btn = {-3, 3, 0.1}}},
};

static const PreferencesWidget effects_widgets[] = {
    {WIDGET_LABEL, N_("<b>Reverb</b>")},
    {WIDGET_CHK_BTN, N_("Enable"), .cfg_type = VALUE_BOOLEAN,
     .cfg = & modplug_settings.mReverb},
    {WIDGET_TABLE, .child = TRUE, .data = {.table = {reverb_fields, ARRAY_LEN (reverb_fields)}}},
    {WIDGET_LABEL, N_("<b>Bass Boost</b>")},
    {WIDGET_CHK_BTN, N_("Enable"), .cfg_type = VALUE_BOOLEAN,
     .cfg = & modplug_settings.mMegabass },
    {WIDGET_TABLE, .child = TRUE, .data = {.table = {bass_fields, ARRAY_LEN (bass_fields)}}},
    {WIDGET_LABEL, N_("<b>Surround</b>")},
    {WIDGET_CHK_BTN, N_("Enable"), .cfg_type = VALUE_BOOLEAN,
     .cfg = & modplug_settings.mSurround },
    {WIDGET_TABLE, .child = TRUE, .data = {.table = {surround_fields, ARRAY_LEN (surround_fields)}}},
    {WIDGET_LABEL, N_("<b>Preamp</b>")},
    {WIDGET_CHK_BTN, N_("Enable"), .cfg_type = VALUE_BOOLEAN,
     .cfg = & modplug_settings.mPreamp },
    {WIDGET_TABLE, .child = TRUE, .data = {.table = {preamp_fields, ARRAY_LEN (preamp_fields)}}}
};

static const PreferencesWidget misc_widgets[] = {
    {WIDGET_LABEL, N_("<b>Miscellaneous</b>")},
    {WIDGET_CHK_BTN, N_("Oversample"), .cfg_type = VALUE_BOOLEAN,
     .cfg = & modplug_settings.mOversamp},
    {WIDGET_CHK_BTN, N_("Noise reduction"), .cfg_type = VALUE_BOOLEAN,
     .cfg = & modplug_settings.mNoiseReduction},
    {WIDGET_CHK_BTN, N_("Play Amiga MODs"), .cfg_type = VALUE_BOOLEAN,
     .cfg = & modplug_settings.mGrabAmigaMOD},
    {WIDGET_LABEL, N_("<b>Repeat</b>")},
    {WIDGET_SPIN_BTN, N_("Repeat count:"), .cfg_type = VALUE_INT,
     .cfg = & modplug_settings.mLoopCount, .data = {.spin_btn = {-1, 100, 1}}},
    {WIDGET_LABEL, N_("To repeat forever, set the repeat count to -1.")}
};

static const PreferencesWidget modplug_columns[] = {
    {WIDGET_BOX, .data = {.box = {quality_widgets, ARRAY_LEN (quality_widgets)}}},
    {WIDGET_SEPARATOR},
    {WIDGET_BOX, .data = {.box = {effects_widgets, ARRAY_LEN (effects_widgets)}}},
    {WIDGET_SEPARATOR},
    {WIDGET_BOX, .data = {.box = {misc_widgets, ARRAY_LEN (misc_widgets)}}}
};

static const PreferencesWidget modplug_widgets[] = {
    {WIDGET_BOX, .data = {.box = {modplug_columns, ARRAY_LEN (modplug_columns), .horizontal = TRUE}}}
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

static bool_t modplug_init (void)
{
    modplug_settings_load ();
    InitSettings (& modplug_settings);
    return TRUE;
}

static void modplug_settings_apply (void)
{
    InitSettings (& modplug_settings);
    modplug_settings_save ();
}

static const PluginPreferences modplug_prefs = {
 .widgets = modplug_widgets,
 .n_widgets = ARRAY_LEN (modplug_widgets),
 .init = modplug_settings_load,
 .apply = modplug_settings_apply
};

AUD_INPUT_PLUGIN
(
    .name = N_("ModPlug (Module Player)"),
    .domain = PACKAGE,
    .prefs = &modplug_prefs,
    .init = modplug_init,
    .play = PlayFile,
    .probe_for_tuple = GetSongTuple,
    .is_our_file_from_vfs = CanPlayFileFromVFS,
    .extensions = fmts,
    .have_subtune = TRUE,   // to exclude .zip etc which doesn't contain any mod file --yaz
)
