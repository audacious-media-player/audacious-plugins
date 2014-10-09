/*
 * Copyright 2003-2004 Chris Morgan <cmorgan@alum.wpi.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _H_JACK_OUT_H
#define _H_JACK_OUT_H

#include <jack/jack.h>

enum status_enum { PLAYING, PAUSED, STOPPED, CLOSED, RESET };

void JACK_Init();
bool JACK_Open(int rate, int output_channels,
               void (*free_space_notify)());
void JACK_Close();
void JACK_Reset(); /* free all buffered data and reset several values in the device */
long JACK_Write(const char * data, unsigned long bytes); /* returns the number of bytes written */

/* set/get the played value based on a millisecond input value */
long JACK_GetPosition();
void JACK_SetPosition(long value);

int JACK_SetState(enum status_enum state); /* playing, paused, stopped */
enum status_enum JACK_GetState();

size_t JACK_GetBytesFreeSpace();       /* bytes of free space in the output buffer */

#endif /* #ifndef JACK_OUT_H */
