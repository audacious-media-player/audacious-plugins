/*
 * OSS4 Output Plugin for Audacious
 * Copyright 2010-2011 Michał Lipski <tallica@o2.pl>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
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
    "This program is free software: you can redistribute it and/or modify "
    "it under the terms of the GNU General Public License as published by "
    "the Free Software Foundation, either version 3 of the License, or "
    "(at your option) any later version.\n\n"
    "This program is distributed in the hope that it will be useful, "
    "but WITHOUT ANY WARRANTY; without even the implied warranty of "
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
    "GNU General Public License for more details.\n\n"
    "You should have received a copy of the GNU General Public License "
    "along with this program.  If not, see <http://www.gnu.org/licenses/>.\n");
}
