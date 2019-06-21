/***************************************************************************
                            spu.h  -  description
                             -------------------
    begin                : Wed May 15 2002
    copyright            : (C) 2002 by Pete Bernert
    email                : BlackDove@addcom.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version. See also the license.txt file for *
 *   additional informations.                                              *
 *                                                                         *
 ***************************************************************************/

//*************************************************************************//
// History of changes:
//
// 2002/05/15 - Pete
// - generic cleanup for the Peops release
//
//*************************************************************************//

void SPUirq(void);

int psf_seek(uint32_t t);
void setlength(int32_t stop, int32_t fade);

int SPUasync(uint32_t cycles, void (*update)(const void *, int));
void SPU_flushboot(void);
int SPUinit(void);
int SPUopen(void);
int SPUclose(void);
int SPUshutdown(void);
void SPUinjectRAMImage(uint16_t *pIncoming);
void SPUreadDMAMem(uint32_t usPSXMem, int iSize);
void SPUwriteDMAMem(uint32_t usPSXMem, int iSize);
uint16_t SPUreadRegister(uint32_t reg);
