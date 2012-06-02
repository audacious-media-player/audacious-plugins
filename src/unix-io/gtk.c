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
#include <audacious/misc.h>
#include <libaudgui/libaudgui-gtk.h>

#include "config.h"

void unix_error (const gchar * format, ...)
{
    va_list args;
    va_start (args, format);

    char * error = g_strdup_vprintf (format, args);
    aud_interface_show_error (error);
    g_free (error);

    va_end (args);
}
