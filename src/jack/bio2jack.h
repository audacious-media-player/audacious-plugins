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

#define ERR_SUCCESS                           0
#define ERR_OPENING_JACK                      1
#define ERR_RATE_MISMATCH                     2
#define ERR_TOO_MANY_OUTPUT_CHANNELS          5
#define ERR_PORT_NOT_FOUND                    7

enum status_enum { PLAYING, PAUSED, STOPPED, CLOSED, RESET };

#define PLAYED          1 /* played out of the speakers(estimated value but should be close */

/**********************/
/* External functions */
void JACK_Init(void); /* call this before any other bio2jack calls */
int  JACK_Open(unsigned int bits_per_channel, int floating_point,
               unsigned long *rate, unsigned int output_channels);
int  JACK_Close(); /* return 0 for success */
void JACK_Reset(); /* free all buffered data and reset several values in the device */
long JACK_Write(const unsigned char *data, unsigned long bytes); /* returns the number of bytes written */

/* state setting values */
/* set/get the played value based on a millisecond input value */
long JACK_GetPosition();
void JACK_SetPosition(long value);

int JACK_SetState(enum status_enum state); /* playing, paused, stopped */
enum status_enum JACK_GetState();

/* Properties of the jack driver */

int  JACK_SetVolumeForChannel(unsigned int channel, unsigned int volume);
void JACK_GetVolumeForChannel(unsigned int channel, unsigned int *volume);

unsigned long JACK_GetBytesFreeSpace();       /* bytes of free space in the output buffer */

enum JACK_PORT_CONNECTION_MODE
{
    CONNECT_ALL,    /* connect to all avaliable ports */
    CONNECT_OUTPUT, /* connect only to the ports we need for output */
    CONNECT_NONE    /* don't connect to any ports */
};

/* set the mode for port connections */
/* defaults to CONNECT_ALL */
void JACK_SetPortConnectionMode(enum JACK_PORT_CONNECTION_MODE mode);

#endif /* #ifndef JACK_OUT_H */
