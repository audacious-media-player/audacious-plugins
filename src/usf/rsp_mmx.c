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

#include "rsp.h"
#include "rsp_x86.h"
#include "recompiler_cpu.h"
#include "memory.h"

#define PUTDST8(dest,value)  (*((uint8_t *)(dest))=(uint8_t)(value)); dest += 1;
#define PUTDST16(dest,value) (*((uint16_t *)(dest))=(uint16_t)(value)); dest += 2;
#define PUTDST32(dest,value) (*((uint32_t *)(dest))=(uint32_t)(value)); dest += 4;
#define PUTDST64(dest,value) {(*((uint64_t *)(dest))=(uint64_t)(value)); dest += 8;}

void RSPMmxEmptyMultimediaState(void) {
	PUTDST16(RSPRecompPos,0x770f);
}

void RSPMmxMoveRegToReg(int32_t Dest, int32_t Source) {
	OPCODE_REG_REG(16,0x7f0f,Source,Dest);
}

void RSPMmxMoveQwordVariableToReg(int32_t Dest, void *Variable) {
    OPCODE_REG_VARIABLE(,16,0x6f0f,Dest,Variable);
}

void RSPMmxMoveQwordRegToVariable(int32_t Dest, void *Variable) {
	OPCODE_REG_VARIABLE(,16,0x7f0f,Dest,Variable);
}

void RSPMmxPorRegToReg(int32_t Dest, int32_t Source) {
	OPCODE_REG_REG(16,0x7f0f,Source,Dest);
}

void RSPMmxPorVariableToReg(void * Variable, int32_t Dest) {
	OPCODE_REG_VARIABLE(,16,0xeb0f,Dest,Variable);
}

void RSPMmxPandRegToReg(int32_t Dest, int32_t Source) {
	OPCODE_REG_REG(16,0xdb0f,Dest,Source);
}

void RSPMmxPandVariableToReg(void * Variable, int32_t Dest) {
	OPCODE_REG_VARIABLE(,16,0xdb0f,Dest,Variable);
}

void RSPMmxPandnRegToReg(int32_t Dest, int32_t Source) {
	OPCODE_REG_REG(16,0xdf0f,Dest,Source);
}

void RSPMmxXorRegToReg(int32_t Dest, int32_t Source) {
	OPCODE_REG_REG(16,0xef0f,Dest,Source);
}

void RSPMmxShuffleMemoryToReg(int32_t Dest, void * Variable, uint8_t Immed) {
    OPCODE_REG_VARIABLE(,16,0x700f,Dest,(uint8_t*)Variable);
    PUTDST8(RSPRecompPos, Immed);
}

void RSPMmxPmullwRegToReg(int32_t Dest, int32_t Source) {
	OPCODE_REG_REG(16,0xd50f,Dest,Source);
}

void RSPMmxPmullwVariableToReg(int32_t Dest, void * Variable) {
    OPCODE_REG_VARIABLE(,16,0xd50f,Dest,Variable);
}

void RSPMmxPmulhuwRegToReg(int32_t Dest, int32_t Source) {
	OPCODE_REG_REG(16,0xe40f,Dest,Source);
}

void RSPMmxPmulhwRegToReg(int32_t Dest, int32_t Source) {
	OPCODE_REG_REG(16,0xe50f,Dest,Source);
}

void RSPMmxPmulhwRegToVariable(int32_t Dest, void * Variable) {
	OPCODE_REG_VARIABLE(,16,0xe50f,Dest,Variable);
}

void RSPMmxPsrlwImmed(int32_t Dest, uint8_t Immed) {
	OPCODE_REG_REG(16,0xe50f,OP_D2,Dest);
	PUTDST8(RSPRecompPos, Immed);
}

void RSPMmxPsrawImmed(int32_t Dest, uint8_t Immed) {
	OPCODE_REG_REG(16,0x710f,OP_D4,Dest);
	PUTDST8(RSPRecompPos, Immed);
}

void RSPMmxPsllwImmed(int32_t Dest, uint8_t Immed) {
	OPCODE_REG_REG(16,0x710f,OP_D6,Dest);
	PUTDST8(RSPRecompPos, Immed);
}

void RSPMmxPaddswRegToReg(int32_t Dest, int32_t Source) {
	OPCODE_REG_REG(16,0xed0f,Dest,Source);
}

void RSPMmxPaddswVariableToReg(int32_t Dest, void * Variable) {
	OPCODE_REG_VARIABLE(,16,0xed0f,Dest,Variable);
}

void RSPMmxPaddwRegToReg(int32_t Dest, int32_t Source) {
	OPCODE_REG_REG(16,0xfd0f,Dest,Source);
}

void RSPMmxPackSignedDwords(int32_t Dest, int32_t Source) {
	OPCODE_REG_REG(16,0x6b0f,Dest,Source);
}

void RSPMmxUnpackLowWord(int32_t Dest, int32_t Source) {
	OPCODE_REG_REG(16,0x610f,Dest,Source);
}

void RSPMmxUnpackHighWord(int32_t Dest, int32_t Source) {
	OPCODE_REG_REG(16,0x690f,Dest,Source);
}

void RSPMmxCompareGreaterWordRegToReg(int32_t Dest, int32_t Source) {
	OPCODE_REG_REG(16,0x650f,Dest,Source);
}
