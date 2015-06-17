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

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>

#define N_ITEMS 2
#define N_MENUS 3

class CDMenuItems : public GeneralPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("Audio CD Menu Items"),
        PACKAGE
    };

    constexpr CDMenuItems () : GeneralPlugin (info, true) {}

    bool init ();
    void cleanup ();
};

EXPORT CDMenuItems aud_plugin_instance;

static constexpr const char * titles[N_ITEMS] = {N_("Play CD"), N_("Add CD")};

static constexpr AudMenuID menus[N_MENUS] = {
    AudMenuID::Main,
    AudMenuID::PlaylistAdd,
    AudMenuID::Playlist
};

static void cd_play () {aud_drct_pl_open ("cdda://"); }
static void cd_add () {aud_drct_pl_add ("cdda://", -1); }
static void (* funcs[N_ITEMS]) () = {cd_play, cd_add};

bool CDMenuItems::init ()
{
    for (int m = 0; m < N_MENUS; m ++)
        for (int i = 0; i < N_ITEMS; i ++)
            aud_plugin_menu_add (menus[m], funcs[i], _(titles[i]), "media-optical");

    return true;
}

void CDMenuItems::cleanup ()
{
    for (int m = 0; m < N_MENUS; m ++)
        for (int i = 0; i < N_ITEMS; i ++)
            aud_plugin_menu_remove (menus[m], funcs[i]);
}
