/*  Copyright (C) 2006 Theo Berkau

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef SPU_H
#define SPU_H

#include "types.h"

#define SNDCORE_DEFAULT         -1
#define SNDCORE_DUMMY           0

typedef struct
{
   int id;
   const char *Name;
   int (*Init)(int buffersize);
   void (*DeInit)();
   void (*UpdateAudio)(s16 *buffer, u32 num_samples);
   u32 (*GetAudioSpace)();
   void (*MuteAudio)();
   void (*UnMuteAudio)();
   void (*SetVolume)(int volume);
} SoundInterface_struct;
extern SoundInterface_struct SNDDummy;


int SPU_ChangeSoundCore(int coreid, int buffersize);
int SPU_Init(int coreid, int buffersize);
void SPU_Pause(int pause);
void SPU_SetVolume(int volume);
void SPU_Reset(void);
void SPU_DeInit(void);
void SPU_KeyOn(int channel);
void SPU_WriteByte(u32 addr, u8 val);
void SPU_WriteWord(u32 addr, u16 val);
void SPU_WriteLong(u32 addr, u32 val);
u32 SPU_ReadLong(u32 addr);
void SPU_Emulate(void);
void SPU_EmulateSamples(u32 numsamples);

#endif
