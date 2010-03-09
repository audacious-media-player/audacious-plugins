/*
 * RSP Compiler plug in for Project 64 (A Nintendo 64 emulator).
 *
 * (c) Copyright 2001 jabo (jabo@emulation64.com) and
 * zilmar (zilmar@emulation64.com)
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

#include "types.h"
#include "rsp.h"
#include "rsp_x86.h"
#include "recompiler_cpu.h"
#include "memory.h"
#include "types.h"

extern uint8_t * DMEM;

#define PUTDST8(dest,value)  (*((uint8_t *)(dest))=(uint8_t)(value)); dest += 1;
#define PUTDST16(dest,value) (*((uint16_t *)(dest))=(uint16_t)(value)); dest += 2;
#define PUTDST32(dest,value) (*((uint32_t *)(dest))=(uint32_t)(value)); dest += 4;
#define PUTDST64(dest,value) {(*((uint64_t *)(dest))=(uint64_t)(value)); dest += 8;}

void RSPSseMoveAlignedVariableToReg(void * Variable, int32_t sseReg) {
    OPCODE_REG_VARIABLE(,16,0x100F,sseReg,Variable);
}

void RSPSseMoveAlignedN64MemToReg(int32_t sseReg, int32_t AddrReg) {
    OPCODE_REG_ADDR_DMEM(,16,0x100F,sseReg,AddrReg);
}

void RSPSseMoveAlignedRegToVariable(int32_t sseReg, void *Variable) {
    OPCODE_REG_VARIABLE(,16,0x110f,sseReg,Variable); //290F
}

void RSPSseMoveAlignedRegToN64Mem(int32_t sseReg, int32_t AddrReg) {
	OPCODE_REG_ADDR_DMEM(,16,0x110f,sseReg,AddrReg); //290F
}

void RSPSseMoveUnalignedVariableToReg(void *Variable, int32_t sseReg) {
	OPCODE_REG_VARIABLE(,16,0x100F,sseReg,Variable);
}

void RSPSseMoveUnalignedN64MemToReg(int32_t sseReg, int32_t AddrReg) {
	OPCODE_REG_ADDR_DMEM(,16,0x100f,sseReg,AddrReg);
}

void RSPSseMoveUnalignedRegToVariable(int32_t sseReg, void *Variable) {
	OPCODE_REG_VARIABLE(,16,0x110F,sseReg,Variable);
}

void RSPSseMoveUnalignedRegToN64Mem(int32_t sseReg, int32_t AddrReg) {
	OPCODE_REG_ADDR_DMEM(,16,0x110f,sseReg,AddrReg);
}

void RSPSseMoveRegToReg(int32_t Dest, int32_t Source) {
	OPCODE_REG_REG(16,0x280F,Dest,Source)
}

void RSPSseXorRegToReg(int32_t Dest, int32_t Source) {
	OPCODE_REG_REG(16,0x570F,Dest,Source)
}

void RSPSseShuffleReg(int32_t Dest, int32_t Source, uint8_t Immed) {
	OPCODE_REG_REG(16,0xC60F,Dest,Source)
	PUTDST8(RSPRecompPos, Immed);
}
