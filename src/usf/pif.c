/*
 * Project 64 - A Nintendo 64 emulator.
 *
 * (c) Copyright 2001 zilmar (zilmar@emulation64.com) and
 * Jabo (jabo@emulation64.com).
 *
 * pj64 homepage: www.pj64.net
 *
 * Permission to use, copy, modify and distribute Project64 in both binary and
 * source form, for non-commercial purposes, is hereby granted without fee,
 * providing that this license information and copyright notice appear with
 * all copies and any derived work.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event shall the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Project64 is freeware for PERSONAL USE only. Commercial users should
 * seek permission of the copyright holders first. Commercial use includes
 * charging money for Project64 or software derived from Project64.
 *
 * The copyright holders request that bug fixes and improvements to the code
 * should be forwarded to them so if they want them.
 *
 */
#include "main.h"
#include "cpu.h"

// Skeletal support so USFs that read the controller won't fail (bad practice, though)

void ProcessControllerCommand ( int32_t Control, uint8_t * Command);

uint8_t *PIF_Ram = 0;

void PifRamRead (void) {
	int32_t Channel, CurPos;

	Channel = 0;
	CurPos  = 0;

	do {
		switch(PIF_Ram[CurPos]) {
		case 0x00:
			Channel += 1;
			if (Channel > 6) { CurPos = 0x40; }
			break;
		case 0xFE: CurPos = 0x40; break;
		case 0xFF: break;
		case 0xB4: case 0x56: case 0xB8: break; /* ??? */
		default:
			if ((PIF_Ram[CurPos] & 0xC0) == 0) {
				CurPos += PIF_Ram[CurPos] + (PIF_Ram[CurPos + 1] & 0x3F) + 1;
				Channel += 1;
			} else {
				CurPos = 0x40;
			}
			break;
		}
		CurPos += 1;
	} while( CurPos < 0x40 );
}

void PifRamWrite (void) {
	int Channel, CurPos;

	Channel = 0;

	for (CurPos = 0; CurPos < 0x40; CurPos++){
		switch(PIF_Ram[CurPos]) {
		case 0x00:
			Channel += 1;
			if (Channel > 6) { CurPos = 0x40; }
			break;
		case 0xFE: CurPos = 0x40; break;
		case 0xFF: break;
		case 0xB4: case 0x56: case 0xB8: break; /* ??? */
		default:
			if ((PIF_Ram[CurPos] & 0xC0) == 0) {
				if (Channel < 4) {
					ProcessControllerCommand(Channel,&PIF_Ram[CurPos]);
				}
				CurPos += PIF_Ram[CurPos] + (PIF_Ram[CurPos + 1] & 0x3F) + 1;
				Channel += 1;
			} else
				CurPos = 0x40;

			break;
		}
	}
	PIF_Ram[0x3F] = 0;
}

// always return failure
void ProcessControllerCommand ( int32_t Control, uint8_t * Command) {
	Command[1] |= 0x80;
}

