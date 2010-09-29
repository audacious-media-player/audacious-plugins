/*
 *  Copyright (C) 2004 Benedikt 'Hunz' Heinz <rocklight@hunz.org>
 *  Copyright (C) 2006 Tony Vroon <chainsaw@gentoo.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <audacious/plugin.h>
#include "rocklight.h"

enum type {
	NONE = 0,
	THINKLIGHT,
	SYSLED,
	B43LED,
};

static int fd, last, state;
static enum type type;

static gboolean rocklight_init (void)
{
	type = NONE;

	fd = open("/proc/acpi/ibm/light", O_RDWR);
	if (fd >= 0) {
		type = THINKLIGHT;
		return TRUE;
	}

	fd = open("/sys/class/leds/pmu-front-led/brightness", O_RDWR);
	if (fd >= 0) {
		type = SYSLED;
		return TRUE;
	}

	fd = open("/sys/class/leds/smu-front-led/brightness", O_RDWR);
	if (fd >= 0) {
		type = SYSLED;
		return TRUE;
	}

	fd = open("/sys/class/leds/b43-phy0::tx/brightness", O_RDWR);
	if (fd >= 0) {
		type = B43LED;
		return TRUE;
	}

	return FALSE;
}

static void rocklight_cleanup(void) {
	close(fd);
}

static void rocklight_playback_start(void) {
	/* Get original value */
	switch (type) {
		case THINKLIGHT:
			last = state = thinklight_get(fd);
			break;

		case SYSLED:
			last = state = sysled_get(fd);
			break;

		case B43LED:
			last = state = b43led_get(fd);
			break;

		default:
			break;
	}
}

static void rocklight_playback_stop(void) {
	if (last == state) return;

	/* Reset to original value */
	switch (type) {
		case THINKLIGHT:
			thinklight_set(fd, state);
			break;

		case SYSLED:
			sysled_set(fd, state);
			break;

		case B43LED:
			b43led_set(fd, state);
			break;

		default:
			break;
	}
}

static void rocklight_render_freq(gint16 data[2][256]) {
	int new = ((data[0][0] + data[1][0]) >> 7) >= 80? 1:0;

	if (new == last) return;
	last = new;

	switch (type) {
		case THINKLIGHT:
			thinklight_set(fd, new);
			break;

		case SYSLED:
			sysled_set(fd, new);
			break;

		case B43LED:
			b43led_set(fd, new);
			break;

		default:
			break;
	}
}

static VisPlugin rocklight_vp = {
	.description = "RockLight",

	.num_pcm_chs_wanted = 0,
	.num_freq_chs_wanted = 2,

	.init = rocklight_init,
	.cleanup = rocklight_cleanup,
	.playback_start = rocklight_playback_start,
	.playback_stop = rocklight_playback_stop,
	.render_freq = rocklight_render_freq
};

VisPlugin *rocklight_vplist[] = { &rocklight_vp, NULL };

DECLARE_PLUGIN(rocklight, NULL, NULL, NULL, NULL, NULL, NULL, rocklight_vplist,NULL);
