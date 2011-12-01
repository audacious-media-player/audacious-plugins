/*
 * UNIX Transport Plugin for Audacious
 * Copyright 2009-2011 John Lindgren
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

#include <stdarg.h>

#include <gtk/gtk.h>

#include <audacious/i18n.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui-gtk.h>

#include "config.h"

void unix_about (void)
{
    static GtkWidget * window = NULL;

    audgui_simple_message (& window, GTK_MESSAGE_INFO, _("About File I/O "
     "Plugin"), "File I/O Plugin for Audacious\n"
     "Copyright 2009-2011 John Lindgren\n\n"
     "THIS PLUGIN IS REQUIRED.  DO NOT DISABLE IT.\n\n"
     "Redistribution and use in source and binary forms, with or without "
     "modification, are permitted provided that the following conditions are "
     "met:\n\n"
     "1. Redistributions of source code must retain the above copyright "
     "notice, this list of conditions, and the following disclaimer.\n\n"
     "2. Redistributions in binary form must reproduce the above copyright "
     "notice, this list of conditions, and the following disclaimer in the "
     "documentation provided with the distribution.\n\n"
     "This software is provided \"as is\" and without any warranty, express or "
     "implied. In no event shall the authors be liable for any damages arising "
     "from the use of this software.");
}

void unix_error (const gchar * format, ...)
{
    va_list args;
    va_start (args, format);

    event_queue_full (0, "interface show error", g_strdup_vprintf (format, args), g_free);

    va_end (args);
}
