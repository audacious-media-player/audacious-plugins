/***************************************************************************
 *            apple_pmu.c
 *
 *  Sun Dec 03 01:07:42 2006
 *  Copyright  2006  Tony 'Chainsaw' Vroon
 *  chainsaw@gentoo.org
 ****************************************************************************/

/*
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
#include <unistd.h>

int thinklight_open(void) {
	return open("/sys/class/leds/pmu-front-led/brightness",O_RDWR);
}

void thinklight_close(int fd) {
	close(fd);	
}

int thinklight_set(int fd, int state) {
	return write(fd,state?"255\n":"0\n",state?2:4);
}

int thinklight_get(int fd) {
	char buf[256];
	int ret=read(fd,&buf,sizeof(buf));
	
	if(atoi(buf) == 255)
		return 1;
	else
		return 0;
}
