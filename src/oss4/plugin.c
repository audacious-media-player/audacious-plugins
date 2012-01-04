/*
 * plugin.c
 * Copyright 2010-2011 Michał Lipski <tallica@o2.pl>
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
#include <gtk/gtk.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

AUD_OUTPUT_PLUGIN
(
    .name = "OSS4",
    .probe_priority = 5,
    .init = oss_init,
    .cleanup = oss_cleanup,
    .open_audio = oss_open_audio,
    .close_audio = oss_close_audio,
    .write_audio = oss_write_audio,
    .drain = oss_drain,
    .buffer_free = oss_buffer_free,
    .set_written_time = oss_set_written_time,
    .written_time = oss_written_time,
    .output_time = oss_output_time,
    .flush = oss_flush,
    .pause = oss_pause,
    .set_volume = oss_set_volume,
    .get_volume = oss_get_volume,
    .about = oss_about,
    .configure = oss_configure,
)

void oss_about(void)
{
    static GtkWidget *dialog;

    audgui_simple_message(&dialog, GTK_MESSAGE_INFO, _("About OSS4 Plugin"),
    "OSS4 Output Plugin for Audacious\n"
    "Copyright 2010-2011 Michał Lipski <tallica@o2.pl>\n\n"
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
    "the use of this software.");
}
