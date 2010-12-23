/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2008 Tomasz Moń
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 * The Audacious team does not consider modular code linking to
 * Audacious or using our public API to be a derived work.
 */

#ifndef PLUGIN_SKINS_H
#define PLUGIN_SKINS_H

#include <glib.h>

#include <audacious/interface.h>
#include <audacious/plugin.h>

#include "skins_cfg.h"
#include "ui_main.h"
#include "ui_equalizer.h"
#include "ui_playlist.h"
#include "ui_skin.h"

#define PACKAGE "audacious-plugins"
#define PACKAGE_NAME "audacious-plugins"

enum {
    SKINS_PATH_USER_SKIN_DIR,
    SKINS_PATH_SKIN_THUMB_DIR,
    SKINS_PATH_COUNT
};

extern gchar *skins_paths[];
extern Iface skins_interface;

gboolean skins_init (IfaceCbs * cbs);
gboolean skins_cleanup(void);
void skins_about(void);
void show_preferences_window(gboolean show);

#endif
