/*
 * AdPlug/XMMS - AdPlug XMMS Plugin
 * Copyright (C) 2002, 2003 Simon Peter <dn.tlp@gmx.net>
 *
 * AdPlug/XMMS is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This plugin is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.

 * You should have received a copy of the GNU Lesser General Public License
 * along with this plugin; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef ADPLUG_XMMS_H
#define ADPLUG_XMMS_H

#include <libaudcore/plugin.h>

bool adplug_init (void);
void adplug_quit (void);
void adplug_about (void);
void adplug_config (void);
bool adplug_play (const char * filename, VFSFile * file);
void adplug_info_box (const char * filename);
Tuple adplug_get_tuple (const char * filename, VFSFile * file);
bool adplug_is_our_fd (const char * filename, VFSFile * file);

#endif
