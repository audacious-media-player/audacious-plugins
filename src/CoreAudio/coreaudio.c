/*  XMMS - Cross-platform multimedia player
 *  Copyright (C) 1998-2000  Peter Alm, Mikael Alm, Olle Hallnas, Thomas Nilsson and 4Front Technologies
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#include <glib.h>

#include "coreaudio.h"

#include <audacious/plugin.h>
#include <audacious/i18n.h>

AUD_OUTPUT_PLUGIN
(
	.name = "CoreAudio Output",
	.init = osx_init,
	.about = osx_about,
	.configure = osx_configure,
	.get_volume = osx_get_volume,
	.set_volume = osx_set_volume,
	.open_audio = osx_open,
	.write_audio = osx_write,
	.close_audio = osx_close,
	.flush = osx_flush,
	.pause = osx_pause,
	.buffer_free = osx_free,
	.buffer_playing = osx_playing,
	.output_time = osx_get_output_time,
	.written_time = osx_get_written_time,
	.probe_priority = 1,
)
