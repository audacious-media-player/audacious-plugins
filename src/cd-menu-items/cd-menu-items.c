/*
 * cd-menu-items.c
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

#include <gtk/gtk.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>

#define N_ITEMS 2
#define N_MENUS 3

static const gchar * titles[N_ITEMS] = {N_("Play CD"), N_("Add CD")};
static const gint menus[N_MENUS] = {AUD_MENU_MAIN, AUD_MENU_PLAYLIST_ADD, AUD_MENU_PLAYLIST};

static void cd_play (void) {aud_drct_pl_open ("cdda://"); }
static void cd_add (void) {aud_drct_pl_add ("cdda://", -1); }
static void (* funcs[N_ITEMS]) (void) = {cd_play, cd_add};

static gboolean cd_init (void)
{
    for (gint m = 0; m < N_MENUS; m ++)
        for (gint i = 0; i < N_ITEMS; i ++)
            aud_plugin_menu_add (menus[m], funcs[i], _(titles[i]), "media-optical");

    return TRUE;
}

void cd_cleanup (void)
{
    for (gint m = 0; m < N_MENUS; m ++)
        for (gint i = 0; i < N_ITEMS; i ++)
            aud_plugin_menu_remove (menus[m], funcs[i]);
}

#define AUD_PLUGIN_NAME        N_("Audio CD Menu Items")
#define AUD_GENERAL_AUTO_ENABLE  TRUE
#define AUD_PLUGIN_INIT        cd_init
#define AUD_PLUGIN_CLEANUP     cd_cleanup

#define AUD_DECLARE_GENERAL
#include <libaudcore/plugin-declare.h>
