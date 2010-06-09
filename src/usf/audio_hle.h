/**
 * Mupen64 hle rsp - hle.c
 * Copyright (C) 2002 Hacktarux
 *
 * Mupen64 homepage: http://mupen64.emulation64.com
 * email address: hacktarux@yahoo.fr
 *
 * If you want to contribute to the project please contact
 * me first (maybe someone is already making what you are
 * planning to do).
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
 * You should have received a copy of the GNU General Public
 * Licence along with this program; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139,
 * USA.
 *
**/

#ifndef HLE_H
#define HLE_H

#ifndef _BIG_ENDIAN
#define S 0
#define S8 0
#else
#define S 1
#define S8 3
#endif

// types
typedef unsigned char		u8;
typedef unsigned short		u16;
typedef unsigned int		u32;
typedef unsigned long long	u64;

typedef signed char			s8;
typedef signed short		s16;
typedef signed int			s32;
typedef signed long long	s64;

/*
 * Audio flags
 */

#define A_INIT			0x01
#define A_CONTINUE		0x00
#define A_LOOP          0x02
#define A_OUT           0x02
#define A_LEFT			0x02
#define	A_RIGHT			0x00
#define A_VOL			0x04
#define A_RATE			0x00
#define A_AUX			0x08
#define A_NOAUX			0x00
#define A_MAIN			0x00
#define A_MIX			0x10

typedef struct
{
   unsigned int type;
   unsigned int flags;

   unsigned int ucode_boot;
   unsigned int ucode_boot_size;

   unsigned int ucode;
   unsigned int ucode_size;

   unsigned int ucode_data;
   unsigned int ucode_data_size;

   unsigned int dram_stack;
   unsigned int dram_stack_size;

   unsigned int output_buff;
   unsigned int output_buff_size;

   unsigned int data_ptr;
   unsigned int data_size;

   unsigned int yield_data_ptr;
   unsigned int yield_data_size;
} OSTask_t;

extern u32 inst1, inst2;
//extern u16 AudioInBuffer, AudioOutBuffer, AudioCount;
//extern u16 AudioAuxA, AudioAuxC, AudioAuxE;
extern u32 loopval; // Value set by A_SETLOOP : Possible conflict with SETVOLUME???
//extern u32 UCData, UDataLen;

extern u32 SEGMENTS[0x10];		// 0x0320
// T8 = 0x360
extern u16 AudioInBuffer;		// 0x0000(T8)
extern u16 AudioOutBuffer;		// 0x0002(T8)
extern u16 AudioCount;			// 0x0004(T8)
extern s16 Vol_Left;		// 0x0006(T8)
extern s16 Vol_Right;		// 0x0008(T8)
extern u16 AudioAuxA;			// 0x000A(T8)
extern u16 AudioAuxC;			// 0x000C(T8)
extern u16 AudioAuxE;			// 0x000E(T8)
extern u32 loopval;			// 0x0010(T8) // Value set by A_SETLOOP : Possible conflict with SETVOLUME???
extern s16 VolTrg_Left;	// 0x0010(T8)
extern s32 VolRamp_Left;	// m_LeftVolTarget
//u16 VolRate_Left;	// m_LeftVolRate
extern s16 VolTrg_Right;	// m_RightVol
extern s32 VolRamp_Right;	// m_RightVolTarget
//u16 VolRate_Right;	// m_RightVolRate
extern s16 Env_Dry;		// 0x001C(T8)
extern s16 Env_Wet;		// 0x001E(T8)


extern u8 BufferSpace[0x10000];

extern short hleMixerWorkArea[256];
extern u16 adpcmtable[0x88];
extern int firstHLE, goldeneye;


extern int audio_ucode(OSTask_t *task);
//extern unsigned char *RDRAM,*DMEM, *IMEM, *ROM;
//extern unsigned int N64MEM_Pages[0x80];
#include "usf.h"
#include "memory.h"
#include "cpu.h"




#endif
