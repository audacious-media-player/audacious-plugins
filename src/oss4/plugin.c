/*
 * plugin.c
 * Copyright 2010-2012 Michał Lipski <tallica@o2.pl>
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

#include "oss.h"
#include <audacious/preferences.h>

static void combo_init(ComboBoxElements ** elements, int * n_elements)
{
    oss_sysinfo sysinfo;
    int mixerfd;

    CHECK_NOISY(mixerfd = open, DEFAULT_MIXER, O_RDWR);
    CHECK(ioctl, mixerfd, SNDCTL_SYSINFO, &sysinfo);
    CHECK_NOISY(oss_probe_for_adev, &sysinfo);

    * elements = (ComboBoxElements *) malloc((sysinfo.numaudios + 1) * sizeof(ComboBoxElements));
    * n_elements = 1;
    ComboBoxElements * tmp = * elements;

    tmp->label = N_("1. Default device");
    tmp->value = DEFAULT_DSP;

    for (int i = 0; i < sysinfo.numaudios; i++)
    {
        oss_audioinfo ainfo;
        ainfo.dev = i;

        CHECK(ioctl, mixerfd, SNDCTL_AUDIOINFO, &ainfo);

        if (ainfo.caps & PCM_CAP_OUTPUT)
        {
            tmp++;
            (* n_elements) ++;
            SPRINTF(str, "%d. %s", * n_elements, ainfo.name);
            tmp->label = strdup(str);
            tmp->value = strdup(ainfo.devnode);
        }
        else
            continue;
    }

FAILED:
    close(mixerfd);
}

static void combo_cleanup(ComboBoxElements * elements, int n_elements)
{
    /* first elements were allocated statically */
    for (int i = 1; i < n_elements; i++)
    {
        free(elements[i].value);
        free((char *) elements[i].label);
    }

    free(elements);
}

static PreferencesWidget oss_widgets[] = {
 {WIDGET_COMBO_BOX, N_("Audio device:"),
  .cfg_type = VALUE_STRING, .csect = "oss4", .cname = "device"},
 {WIDGET_CHK_BTN, N_("Use alternate device:"),
  .cfg_type = VALUE_BOOLEAN, .csect = "oss4", .cname = "use_alt_device"},
 {WIDGET_ENTRY,
  .cfg_type = VALUE_STRING, .csect = "oss4", .cname = "alt_device", .child = TRUE},
 {WIDGET_CHK_BTN, N_("Save volume between sessions."),
  .cfg_type = VALUE_BOOLEAN, .csect = "oss4", .cname = "save_volume"},
 {WIDGET_CHK_BTN, N_("Enable format conversions made by the OSS software."),
  .cfg_type = VALUE_BOOLEAN, .csect = "oss4", .cname = "cookedmode"},
 {WIDGET_CHK_BTN, N_("Enable exclusive mode to prevent virtual mixing."),
  .cfg_type = VALUE_BOOLEAN, .csect = "oss4", .cname = "exclusive"},
};

static void prefs_init()
{
    combo_init((ComboBoxElements **) &oss_widgets[0].data.combo.elements,
     &oss_widgets[0].data.combo.n_elements);
}

static void prefs_cleanup()
{
    combo_cleanup((ComboBoxElements *) oss_widgets[0].data.combo.elements,
     oss_widgets[0].data.combo.n_elements);
}

static const PluginPreferences oss_prefs = {
 .widgets = oss_widgets,
 .n_widgets = N_ELEMENTS(oss_widgets),
 .init = prefs_init,
 .cleanup = prefs_cleanup};

static const char oss_about[] =
 "OSS4 Output Plugin for Audacious\n"
 "Copyright 2010-2012 Michał Lipski <tallica@o2.pl>\n\n"
 "I would like to thank people on #audacious, especially Tony Vroon and "
 "John Lindgren and of course the authors of the previous OSS plugin.\n\n"
 "Redistribution and use in source and binary forms, with or without "
 "modification, are permitted provided that the following conditions are met:\n\n"
 "1. Redistributions of source code must retain the above copyright notice, "
 "this list of conditions, and the following disclaimer.\n\n"
 "2. Redistributions in binary form must reproduce the above copyright notice, "
 "this list of conditions, and the following disclaimer in the documentation "
 "provided with the distribution.\n\n"
 "This software is provided \"as is\" and without any warranty, express or "
 "implied. In no event shall the authors be liable for any damages arising from "
 "the use of this software.";

AUD_OUTPUT_PLUGIN
(
    .name = N_("OSS4 Output"),
    .domain = PACKAGE,
    .probe_priority = 5,
    .init = oss_init,
    .cleanup = oss_cleanup,
    .open_audio = oss_open_audio,
    .close_audio = oss_close_audio,
    .write_audio = oss_write_audio,
    .drain = oss_drain,
    .buffer_free = oss_buffer_free,
    .output_time = oss_output_time,
    .flush = oss_flush,
    .pause = oss_pause,
    .set_volume = oss_set_volume,
    .get_volume = oss_get_volume,
    .about_text = oss_about,
    .prefs = &oss_prefs
)
