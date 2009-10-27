/*
 *  Copyright (C) 2006 Tony Vroon <chainsaw@gentoo.org>
 *  Copyright (C) 2007 Michael Hanselmann <audacious@hansmi.ch>
 *  Copyright (C) 2009 William Pitcock <nenolod@dereferenced.org>
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
 
#include <unistd.h>
#include <stdlib.h>

#include "rocklight.h"

int b43led_set(int fd, int state) {
	if (state) {
		return write(fd, "0\n", 2);
	} else {
		return write(fd, "255\n", 4);
	}
}

int b43led_get(int fd) {
	char buf[256];
	int ret = read(fd, &buf, sizeof(buf));
    if(!ret)
        return 0; //dummy

	return (strtol(buf, NULL, 10) == 0);
}
