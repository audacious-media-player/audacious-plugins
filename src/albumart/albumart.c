/*
 * albumart.c
 * Copyright 2012-2013 John Lindgren
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

#include <audacious/drct.h>
#include <libaudcore/i18n.h>
#include <audacious/plugin.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

static void album_update (void * unused, GtkWidget * widget)
{
    if (! aud_drct_get_playing ())
        return;

    GdkPixbuf * pixbuf = audgui_pixbuf_request_current ();

    if (! pixbuf)
        pixbuf = audgui_pixbuf_fallback ();

    audgui_scaled_image_set (widget, pixbuf);

    if (pixbuf)
        g_object_unref (pixbuf);
}

static void album_clear (void * unused, GtkWidget * widget)
{
    audgui_scaled_image_set (widget, NULL);
}

static void album_cleanup (GtkWidget * widget)
{
    hook_dissociate_full ("playback begin", (HookFunction) album_update, widget);
    hook_dissociate_full ("current art ready", (HookFunction) album_update, widget);
    hook_dissociate_full ("playback stop", (HookFunction) album_clear, widget);

    audgui_cleanup ();
}

static void * album_get_widget (void)
{
    audgui_init ();

    GtkWidget * widget = audgui_scaled_image_new (NULL);
    gtk_widget_set_size_request (widget, 96, 96);

    g_signal_connect (widget, "destroy", (GCallback) album_cleanup, NULL);

    hook_associate ("playback begin", (HookFunction) album_update, widget);
    hook_associate ("current art ready", (HookFunction) album_update, widget);
    hook_associate ("playback stop", (HookFunction) album_clear, widget);

    album_update (NULL, widget);

    return widget;
}

AUD_GENERAL_PLUGIN
(
    .name = N_("Album Art"),
    .domain = PACKAGE,
    .get_widget = album_get_widget
)
