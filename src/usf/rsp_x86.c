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

#include <stdint.h>
#include <stdio.h>
#include "rsp.h"
#include "rsp_x86.h"
#include "recompiler_cpu.h"
#include "memory.h"
#include "types.h"

#define PUTDST8(dest,value)  {(*((uint8_t *)(dest))=(uint8_t)(value)); dest += 1;}
#define PUTDST16(dest,value) {(*((uint16_t *)(dest))=(uint16_t)(value)); dest += 2;}
#define PUTDST32(dest,value) {(*((uint32_t *)(dest))=(uint32_t)(value)); dest += 4;}
#define PUTDST64(dest,value) {(*((uint64_t *)(dest))=(uint64_t)(value)); dest += 8;}


#define x64_Reg		0x10
#define X64_Reg		0x10
#define X64_Ext		0x20

extern uint8_t Index[9];


char * x86_Strings[8] = {
	"eax", "ebx", "ecx", "edx",
	"esi", "edi", "ebp", "esp"
};

char * x86_ByteStrings[8] = {
	"al", "bl", "cl", "dl",
	"?4", "?5", "?6", "?7"
};

char * x86_HalfStrings[8] = {
	"ax", "bx", "cx", "dx",
	"si", "di", "bp", "sp"
};

extern uint32_t ConditionalMove;

#define x86Byte_Name(Reg) (x86_ByteStrings[(Reg)])
#define x86Half_Name(Reg) (x86_HalfStrings[(Reg)])



void RSPAdcX86regToVariable(int32_t x86reg, void * Variable) {
    OPCODE_REG_VARIABLE(,8,0x11,x86reg,Variable)
}

void RSPAdcConstToVariable(void *Variable, uint8_t Constant) {
	RSPBreakPoint();

	/*if(((uintptr_t)Variable - (uintptr_t)TLB_Map) < 0x7FFFFFFF) {
		RSPBreakPoint();
		OPCODE_REG_MREG_IMM32(8,0x81,OP_D2,x86_R15,(uintptr_t)Variable - (uintptr_t)TLB_Map);
    } else {
    	LOAD_VARIABLE(x86_TEMP, Variable);
    	OPCODE_REG_MREG(8,0x81,OP_D2,x86_TEMP);
    }

	PUTDST8(RSPRecompPos,Constant);
    */
}

void RSPAdcX86RegToX86Reg(int32_t Destination, int32_t Source) {
    OPCODE_REG_REG(8,0x13,Destination,Source);
}

void RSPAddConstToVariable (uint32_t Const, void *Variable) {
    OPCODE_REG_VARIABLE(,8,0x81,OP_D0,Variable)
    PUTDST32(RSPRecompPos,Const);
}

void RSPAddConstToX86Reg64 (int32_t x86reg, uint32_t Const) {
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		OPCODE_REG_REG(8,0x81,OP_D0,x86reg | x64_Reg);
		PUTDST32(RSPRecompPos, Const);
	} else {
		OPCODE_REG_REG(8,0x83,OP_D0,x86reg | x64_Reg);
		PUTDST8(RSPRecompPos, Const);
	}
}

void RSPAddConstToX86Reg (int32_t x86reg, uint32_t Const) {
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		OPCODE_REG_REG(8,0x81,OP_D0,x86reg);
		PUTDST32(RSPRecompPos, Const);
	} else {
		OPCODE_REG_REG(8,0x83,OP_D0,x86reg);
		PUTDST8(RSPRecompPos, Const);
	}
}


void RSPAddQwordToX86Reg (int32_t x86reg, uint64_t Const) {
#ifdef USEX64
	LOAD_VARIABLE(x86_TEMP, Const);
	OPCODE_REG_REG(8,0x1,x86_TEMP,x86reg);
#else
	RSPAddConstToX86Reg(x86reg, Const);
#endif
}


void RSPAddVariableToX86reg(int32_t x86reg, void * Variable) {
    OPCODE_REG_VARIABLE(,8,0x3,x86reg,Variable)
}

void RSPAddX86regToVariable(int32_t x86reg, void * Variable) {
    OPCODE_REG_VARIABLE(,8,0x1,x86reg,Variable)
}

void RSPAddX86RegToX86Reg(int32_t Destination, int32_t Source) {
	OPCODE_REG_REG(8,0x3,Destination,Source);
}

void RSPAndConstToVariable (uint32_t Const, void *Variable) {
    OPCODE_REG_VARIABLE(,8,0x81,OP_D4,Variable)
    PUTDST32(RSPRecompPos,Const);
}

void RSPAndConstToX86Reg(int32_t x86reg, uint32_t Const) {
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		OPCODE_REG_REG(8,0x81,OP_D4,x86reg);
		PUTDST32(RSPRecompPos, Const);
	} else {
		OPCODE_REG_REG(8,0x83,OP_D4,x86reg);
		PUTDST8(RSPRecompPos, Const);
	}
}

void RSPAndVariableDispToX86Reg(void *Variable, int32_t x86reg, int32_t AddrReg, int32_t Multiplier) {
#ifdef USEX64
	if(((uintptr_t)Variable - (uintptr_t)TLB_Map) < 0x7FFFFFFF) {
		OPCODE_REG_BASE_INDEX_SCALE_IMM32(8,0x23,x86reg,x86_R15,AddrReg,Index[Multiplier], (uintptr_t)Variable - (uintptr_t)TLB_Map);
	} else {
		LOAD_VARIABLE(x86_TEMP, Variable);
		OPCODE_REG_BASE_INDEX_SCALE(8,0x23,x86reg,x86_TEMP,AddrReg,Index[Multiplier]);
	}
#else
	OPCODE_REG_INDEX_SCALE_IMM32(8,0x23,x86reg,AddrReg,Index[Multiplier],Variable);
#endif
}

void RSPAndVariableToX86Reg(void * Variable, int32_t x86reg) {
	OPCODE_REG_VARIABLE(,8,0x23,x86reg,Variable)
}

void RSPAndX86RegToX86Reg(int32_t Destination, int32_t Source) {
	OPCODE_REG_REG(8,0x21,Source,Destination);
}

void RSPBreakPoint (void) {
	PUTDST8(RSPRecompPos,0xCC);
}

void RSPCall_Direct(void * FunctAddress) {
	uintptr_t disp = 0;
#ifdef USEX64
	disp = (uintptr_t)FunctAddress-(uintptr_t)RSPRecompPos - 5;
	RSPSubConstFromX86Reg(x86_RSP, 0x28);
//	if(disp <= 0x7fffffff) {
//		PUTDST8(RSPRecompPos,0xE8);
//		PUTDST32(RSPRecompPos,disp);
//	} else {
		LOAD_VARIABLE(x86_TEMP, FunctAddress);
		OPCODE_REG_REG(8,0xff,OP_D2,x86_TEMP);

//	}
	RSPAddConstToX86Reg(x86_RSP, 0x28);
#else
	disp = (uintptr_t)FunctAddress-(uintptr_t)RSPRecompPos - 5;
	PUTDST8(RSPRecompPos,0xE8);
	PUTDST32(RSPRecompPos,disp);
#endif
}

void RSPCompConstToVariable(uint32_t Const, void * Variable) {
    OPCODE_REG_VARIABLE(,8,0x81,OP_D7,Variable)
    PUTDST32(RSPRecompPos,Const);
}

void RSPCompConstToX86reg(int32_t x86reg, uint32_t Const) {
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		OPCODE_REG_REG(8,0x81,OP_D7,x86reg);
		PUTDST32(RSPRecompPos,Const);
	} else {
		OPCODE_REG_REG(8,0x83,OP_D7,x86reg);
		PUTDST8(RSPRecompPos, Const);
	}
}

void RSPCompX86regToVariable(int32_t x86reg, void * Variable) {
    OPCODE_REG_VARIABLE(,8,0x3B,x86reg,Variable)
}

void RSPCompX86RegToX86Reg(int32_t Destination, int32_t Source) {
	OPCODE_REG_REG(8,0x3B,Destination,Source);
}

void RSPDecX86reg(x86reg) {
	OPCODE_REG_REG(8,0xFF,OP_D1,x86reg);
}

void RSPDivX86reg(int32_t x86reg) {
	OPCODE_REG_REG(8,0xF7,OP_D6,x86reg);
}

void RSPidivX86reg(int32_t x86reg) {
	OPCODE_REG_REG(8,0xF7,OP_D7,x86reg);
}

void RSPimulX86reg(int32_t x86reg) {
	OPCODE_REG_REG(8,0xF7,OP_D5,x86reg);
}

void RSPIncX86reg(int32_t x86reg) {
	OPCODE_REG_REG(8,0xFF,OP_D0,x86reg);
}

void RSPJaeLabel8( uint8_t Value) {
	PUTDST8(RSPRecompPos,0x73);
	PUTDST8(RSPRecompPos,Value);
}

void RSPJaeLabel32(uint32_t Value) {
	PUTDST16(RSPRecompPos,0x830F);
	PUTDST32(RSPRecompPos,Value);
}

void RSPJaLabel8( uint8_t Value) {
	PUTDST8(RSPRecompPos,0x77);
	PUTDST8(RSPRecompPos,Value);
}

void RSPJaLabel32(uint32_t Value) {
	PUTDST16(RSPRecompPos,0x870F);
	PUTDST32(RSPRecompPos,Value);
}

void RSPJbLabel8( uint8_t Value) {
	PUTDST8(RSPRecompPos,0x72);
	PUTDST8(RSPRecompPos,Value);
}

void RSPJbLabel32(uint32_t Value) {
	PUTDST16(RSPRecompPos,0x820F);
	PUTDST32(RSPRecompPos,Value);
}

void RSPJecxzLabel8( uint8_t Value) {
	PUTDST8(RSPRecompPos,0xE3);
	PUTDST8(RSPRecompPos,Value);
}

void RSPJeLabel8( uint8_t Value) {
	PUTDST8(RSPRecompPos,0x74);
	PUTDST8(RSPRecompPos,Value);
}

void RSPJeLabel32(uint32_t Value) {
	PUTDST16(RSPRecompPos,0x840F);
	PUTDST32(RSPRecompPos,Value);
}

void RSPJgeLabel32(uint32_t Value) {
	PUTDST16(RSPRecompPos,0x8D0F);
	PUTDST32(RSPRecompPos,Value);
}

void RSPJgLabel8( uint8_t Value) {
	PUTDST8(RSPRecompPos,0x7F);
	PUTDST8(RSPRecompPos,Value);
}

void RSPJgLabel32(uint32_t Value) {
	PUTDST16(RSPRecompPos,0x8F0F);
	PUTDST32(RSPRecompPos,Value);
}

void RSPJleLabel8( uint8_t Value) {
	PUTDST8(RSPRecompPos,0x7E);
	PUTDST8(RSPRecompPos,Value);
}

void RSPJleLabel32(uint32_t Value) {
	PUTDST16(RSPRecompPos,0x8E0F);
	PUTDST32(RSPRecompPos,Value);
}

void RSPJlLabel8( uint8_t Value) {
	PUTDST8(RSPRecompPos,0x7C);
	PUTDST8(RSPRecompPos,Value);
}

void RSPJlLabel32(uint32_t Value) {
	PUTDST16(RSPRecompPos,0x8C0F);
	PUTDST32(RSPRecompPos,Value);
}

void RSPJzLabel8( uint8_t Value) {
	PUTDST8(RSPRecompPos,0x74);
	PUTDST8(RSPRecompPos,Value);
}

void RSPJnzLabel8( uint8_t Value) {
	PUTDST8(RSPRecompPos,0x75);
	PUTDST8(RSPRecompPos,Value);
}


void RSPJmpDirectReg( int32_t x86reg ) {
	OPCODE_REG_REG(8,0xff,OP_D4,x86reg);
}

void RSPJmpLabel8( uint8_t Value) {
	PUTDST8(RSPRecompPos,0xEB);
	PUTDST8(RSPRecompPos,Value);
}

void RSPJmpLabel32( uint32_t Value) {
	PUTDST8(RSPRecompPos,0xE9);
	PUTDST32(RSPRecompPos,Value);
}

void RSPJneLabel8( uint8_t Value) {
	PUTDST8(RSPRecompPos,0x75);
	PUTDST8(RSPRecompPos,Value);
}

void RSPJneLabel32(uint32_t Value) {
	PUTDST16(RSPRecompPos,0x850F);
	PUTDST32(RSPRecompPos,Value);
}

void RSPJnsLabel8( uint8_t Value) {
	PUTDST8(RSPRecompPos,0x79);
	PUTDST8(RSPRecompPos,Value);
}

void RSPJnsLabel32(uint32_t Value) {
	PUTDST16(RSPRecompPos,0x890F);
	PUTDST32(RSPRecompPos,Value);
}

void RSPJsLabel32(uint32_t Value) {
	PUTDST16(RSPRecompPos,0x880F);
	PUTDST32(RSPRecompPos,Value);
}

void RSPLeaRegReg(int32_t x86RegDest, int32_t x86RegSrc, int32_t multiplier) {
	OPCODE_REG_BASE_INDEX_SCALE(8,0x8D,x86RegDest,x86_EBP,x86RegSrc,Index[multiplier]);
	PUTDST32(RSPRecompPos,0x00000000);
}

void RSPLeaSourceAndOffset(int32_t x86DestReg, int32_t x86SourceReg, int32_t offset) {
    OPCODE_REG_MREG_IMM32(8,0x8D,x86DestReg,x86SourceReg,offset);
}

void RSPMoveConstByteToVariable (uint8_t Const,void *Variable) {
    OPCODE_REG_VARIABLE(,8,0xC6,OP_D0,Variable)
    PUTDST8(RSPRecompPos,Const);
}

void RSPMoveConstHalfToVariable (uint16_t Const,void *Variable) {
    OPCODE_REG_VARIABLE(PUTDST8(RSPRecompPos,0x66),8,0xC7,OP_D0,Variable)
    PUTDST16(RSPRecompPos,Const);
}

void RSPMoveConstHalfToX86regPointer(uint16_t Const, int32_t AddrReg1, int32_t AddrReg2) {
    PUTDST8(RSPRecompPos,0x66);
    OPCODE_REG_BASE_INDEX(8,0xC7,OP_D0,AddrReg1,AddrReg2);
    PUTDST16(RSPRecompPos,Const);
}

void RSPMoveX86regByteToN64Mem(int32_t x86reg, int32_t AddrReg) {
	OPCODE_REG_ADDR_DMEM(,8,0x88,x86reg,AddrReg);
}

void RSPMoveX86regHalfToN64Mem(int32_t x86reg, int32_t AddrReg) {
    OPCODE_REG_ADDR_DMEM(PUTDST8(RSPRecompPos,0x66),8,0x89,x86reg,AddrReg);
}

void RSPMoveX86regToN64Mem(int32_t x86reg, int32_t AddrReg) {
    OPCODE_REG_ADDR_DMEM(,8,0x89,x86reg,AddrReg);
}


void RSPMoveConstToVariable (uint32_t Const,void *Variable) {
    OPCODE_REG_VARIABLE(,8,0xC7,OP_D0,Variable)
    PUTDST32(RSPRecompPos,Const);
}

void RSPMoveConstToX86Pointer(uint32_t Const, int32_t X86Pointer) {
    OPCODE_REG_MREG(8,0xC7,OP_D0,X86Pointer);
    PUTDST32(RSPRecompPos,Const);
}


void RSPMoveOffsetToX86reg(uint32_t Const, int32_t x86reg) {
	OPCODE_REG_REG(8,0xC7,OP_D0,x86reg);
    PUTDST32(RSPRecompPos,Const);
}


void RSPMoveConstToX86reg(uint32_t Const, int32_t x86reg) {
	OPCODE_REG_REG(8,0xC7,OP_D0,x86reg);
    PUTDST32(RSPRecompPos,Const);
}

void RSPMoveConstQwordToX86reg(uintptr_t Const, int32_t x86reg) {
#ifndef USEX64
    OPCODE_REG_REG(8,0xC7,OP_D0,x86reg);
    PUTDST32(RSPRecompPos,Const);
#else
    PUTDST8(RSPRecompPos, 0x48 | ((x86reg&0x20)>>5));
    PUTDST8(RSPRecompPos, 0xB8 | ((x86reg-1)&0x7));
    PUTDST64(RSPRecompPos,Const);
#endif
}

void RSPMoveConstByteToX86regPointer(uint8_t Const, int32_t AddrReg1, int32_t AddrReg2) {
	OPCODE_REG_BASE_INDEX(8,0xC6,OP_D0,AddrReg1,AddrReg2)
    PUTDST8(RSPRecompPos,Const);
}

void RSPMoveConstToX86regPointer(uint32_t Const, int32_t AddrReg1, int32_t AddrReg2) {
	OPCODE_REG_BASE_INDEX(8,0xC7,OP_D0,AddrReg1,AddrReg2);
    PUTDST32(RSPRecompPos,Const);
}

void RSPMoveN64MemToX86reg(int32_t x86reg, int32_t AddrReg) {
#ifdef USEX64
	if(((uintptr_t)DMEM - (uintptr_t)TLB_Map) < 0x7FFFFFFF) {
		OPCODE_REG_BASE_INDEX_IMM32(8,0x8B,x86reg,x86_R15,AddrReg,(uintptr_t)DMEM - (uintptr_t)TLB_Map);
    } else {
    	LOAD_VARIABLE(x86_TEMP, DMEM);
		OPCODE_REG_BASE_INDEX(8,0x8B,x86reg,x86_TEMP,AddrReg);
    }
#else
	OPCODE_REG_MREG_IMM32(8,0x8B,x86reg,AddrReg,DMEM);
#endif
}

void RSPMoveSxByteX86regPointerToX86reg(int32_t AddrReg1, int32_t AddrReg2, int32_t x86reg) {
	OPCODE_REG_BASE_INDEX(16,0xBE0F,x86reg,AddrReg1,AddrReg2);
}

void RSPMoveSxHalfX86regPointerToX86reg(int32_t AddrReg1, int32_t AddrReg2, int32_t x86reg) {
    OPCODE_REG_BASE_INDEX(16,0xBF0F,x86reg,AddrReg1,AddrReg2);
}


void RSPMoveN64MemToX86regByte(int32_t x86reg, int32_t AddrReg) {
	OPCODE_REG_ADDR_DMEM(,8,0x8A,x86reg,AddrReg);
}


void RSPMoveSxN64MemToX86regByte(int32_t x86reg, int32_t AddrReg) {
    OPCODE_REG_ADDR_DMEM(,16,0xBE0F,x86reg,AddrReg);
}

void RSPMoveSxVariableToX86regHalf(void *Variable, int32_t x86reg) {
    OPCODE_REG_VARIABLE(,16,0xBF0F,x86reg,Variable);
}


void RSPMoveSxN64MemToX86regHalf(int32_t x86reg, int32_t AddrReg) {
    OPCODE_REG_ADDR_DMEM(,16,0xBF0F,x86reg,AddrReg);
}

void RSPMoveZxN64MemToX86regHalf(int32_t x86reg, int32_t AddrReg) {
	OPCODE_REG_ADDR_DMEM(,16,0xB70F,x86reg,AddrReg);
}

void RSPMoveVariableToX86reg(void *Variable, int32_t x86reg) {
	OPCODE_REG_VARIABLE(,8,0x8B,x86reg,Variable);
}

void RSPMoveX86RegDispToX86Reg(int32_t x86reg, int32_t AddrReg, int32_t IndexReg, int32_t Multiplier) {
	OPCODE_REG_BASE_INDEX_SCALE(8,0x8B,x86reg,AddrReg,IndexReg,Index[Multiplier]);
}

void RSPMoveN64MemDispToX86reg(int32_t x86reg, int32_t AddrReg, uint8_t Disp) {
#ifdef USEX64
	if(((uintptr_t)DMEM - (uintptr_t)TLB_Map) < 0x7FFFFFFF) {
		OPCODE_REG_BASE_INDEX_SCALE_IMM32(8,0x8B,x86reg,x86_R15,AddrReg,Index[0],(((uintptr_t)DMEM - (uintptr_t)TLB_Map))+Disp);
	} else {
		LOAD_VARIABLE(x86_TEMP, DMEM);
		OPCODE_REG_BASE_INDEX_SCALE_IMM8(8,0x8B,x86reg,x86_TEMP,AddrReg,Index[0],Disp);
	}
#else
    OPCODE_REG_MREG_IMM32(8,0x8B,x86reg,AddrReg,DMEM+Disp);
#endif
}

void RSPMoveVariableDispToX86Reg(void *Variable, int32_t x86reg, int32_t AddrReg, int32_t Multiplier) {
#ifdef USEX64
	if(((uintptr_t)Variable - (uintptr_t)TLB_Map) < 0x7FFFFFFF) {
		OPCODE_REG_BASE_INDEX_SCALE_IMM32(8,0x8B,x86reg,x86_R15,AddrReg,Index[Multiplier],((uintptr_t)Variable - (uintptr_t)TLB_Map));
	} else {
		LOAD_VARIABLE(x86_TEMP, Variable);
		OPCODE_REG_BASE_INDEX_SCALE(8,0x8B,x86reg,x86_TEMP,AddrReg,Index[Multiplier]);
	}
#else
	OPCODE_REG_INDEX_SCALE_IMM32(8,0x8B,x86reg,AddrReg,Index[Multiplier],Variable);
#endif
}

void RSPMoveN64MemToX86regHalf(int32_t x86reg, int32_t AddrReg)
{
	RSPMoveVariableDispToX86Reg(DMEM,x86reg,AddrReg,1);
}


void RSPMoveVariableToX86regByte(void *Variable, int32_t x86reg) {
    OPCODE_REG_VARIABLE(,8,0x8A,x86reg,Variable);
}

void RSPMoveVariableToX86regHalf(void *Variable, int32_t x86reg) {
    OPCODE_REG_VARIABLE(PUTDST8(RSPRecompPos,0x66),8,0x8B,x86reg,Variable);
}

void RSPMoveX86regByteToVariable(int32_t x86reg, void * Variable) {
    OPCODE_REG_VARIABLE(,8,0x88,x86reg,Variable);
}

void RSPMoveX86regByteToX86regPointer(int32_t x86reg, int32_t AddrReg) {
	OPCODE_REG_MREG(8,0x88,x86reg,AddrReg);
}

void RSPMoveX86regHalfToVariable(int32_t x86reg, void * Variable) {
    OPCODE_REG_VARIABLE(PUTDST8(RSPRecompPos,0x66),8,0x89,x86reg,Variable);
}

void RSPMoveX86regHalfToX86regPointer(int32_t x86reg, int32_t AddrReg) {
	PUTDST8(RSPRecompPos,0x66);
	OPCODE_REG_MREG(8,0x89,x86reg,AddrReg);
}

void RSPMoveX86PointerToX86reg(int32_t x86reg, int32_t X86Pointer) {
	OPCODE_REG_MREG(8,0x8B,x86reg,X86Pointer);
}

void RSPMoveX86regPointerToX86reg(int32_t Destination, int32_t AddrReg) {
	OPCODE_REG_MREG(8,0x8B,Destination,AddrReg);
}

void RSPMoveX86regPointerToX86regDisp8(int32_t AddrReg1, int32_t AddrReg2, int32_t x86reg, uint8_t offset) {
	OPCODE_REG_BASE_INDEX_IMM8(8,0x8B,x86reg,AddrReg1,AddrReg2,offset);
}

void RSPMoveX86regToMemory(int32_t x86reg, int32_t AddrReg, uint32_t Disp) {
	OPCODE_REG_MREG_IMM32(8,0x89,x86reg,AddrReg,Disp);
}

void RSPMoveX86regToVariable(int32_t x86reg, void * Variable) {
    OPCODE_REG_VARIABLE(,8,0x89,x86reg,Variable);
}

void RSPMoveX86RegToX86Reg(int32_t Source, int32_t Destination) {
	OPCODE_REG_REG(8,0x89,Source|X64_Reg, Destination|X64_Reg);
}

void RSPMoveX86regToX86Pointer(int32_t x86reg, int32_t X86Pointer) {
	OPCODE_REG_MREG(8,0x89,x86reg, X86Pointer);
}

void RSPMoveX86regToX86regPointer(int32_t x86reg, int32_t AddrReg) {
    OPCODE_REG_MREG(8,0x89,x86reg,AddrReg);
}

void RSPMoveZxByteX86regPointerToX86reg(int32_t AddrReg1, int32_t AddrReg2, int32_t x86reg) {
	OPCODE_REG_BASE_INDEX(16,0xB60F,x86reg,AddrReg1,AddrReg2);
}

void RSPMoveZxHalfX86regPointerToX86reg(int32_t AddrReg1, int32_t AddrReg2, int32_t x86reg) {
	OPCODE_REG_BASE_INDEX(16,0xB70F,x86reg,AddrReg1,AddrReg2);
}

void RSPMoveZxVariableToX86regByte(void *Variable, int32_t x86reg) {
    OPCODE_REG_VARIABLE(,16,0xB60F,x86reg,Variable);
}

void RSPMoveZxVariableToX86regHalf(void *Variable, int32_t x86reg) {
	OPCODE_REG_VARIABLE(,16,0xB70F,x86reg,Variable);
}

void RSPMulX86reg(int32_t x86reg) {
	OPCODE_REG_REG(8,0xF7,OP_D4,x86reg);
}

void RSPNotX86Reg(int32_t  x86reg) {
	OPCODE_REG_REG(8,0xF7,OP_D2,x86reg);
}

void RSPOrConstToVariable(uint32_t Const, void * Variable) {
    OPCODE_REG_VARIABLE(,8,0x81,OP_D1,Variable);
    PUTDST32(RSPRecompPos,Const);
}

void RSPOrConstToX86Reg(uint32_t Const, int32_t  x86reg) {
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		OPCODE_REG_REG(8,0x81,OP_D1,x86reg);
		PUTDST32(RSPRecompPos, Const);
	} else {
		OPCODE_REG_REG(8,0x83,OP_D1,x86reg);
		PUTDST8(RSPRecompPos, Const);
	}
}

void RSPOrVariableToX86Reg(void * Variable, int32_t x86reg) {
    OPCODE_REG_VARIABLE(,8,0xb,x86reg,Variable);
}

void RSPOrX86RegToVariable(void * Variable, int32_t x86reg) {
    OPCODE_REG_VARIABLE(,8,0x9,x86reg,Variable);
}

void RSPOrX86RegToX86Reg(int32_t Destination, int32_t Source) {
    OPCODE_REG_REG(8,0x0B,Destination,Source);
}

void RSPPushfd() {
	PUTDST8(RSPRecompPos,0x9c);
}

void RSPPopfd() {
	PUTDST8(RSPRecompPos,0x9d);
}

void RSPPopad(void) {
#ifdef USEX64
	PUTDST16(RSPRecompPos,0x5f41);
	PUTDST16(RSPRecompPos,0x5e41);
	PUTDST16(RSPRecompPos,0x5d41);
	PUTDST16(RSPRecompPos,0x5c41);
	PUTDST16(RSPRecompPos,0x5b41);
	PUTDST16(RSPRecompPos,0x5a41);
	PUTDST16(RSPRecompPos,0x5941);
	PUTDST16(RSPRecompPos,0x5841);
	PUTDST8(RSPRecompPos,0x5f);
	PUTDST8(RSPRecompPos,0x5e);
	PUTDST8(RSPRecompPos,0x5b);
	PUTDST8(RSPRecompPos,0x5a);
	PUTDST8(RSPRecompPos,0x59);
	PUTDST8(RSPRecompPos,0x58);
#else
	PUTDST8(RSPRecompPos,0x61);
#endif
}

void RSPPushad(void) {
#ifdef USEX64
	PUTDST8(RSPRecompPos,0x50);
	PUTDST8(RSPRecompPos,0x51);
	PUTDST8(RSPRecompPos,0x52);
	PUTDST8(RSPRecompPos,0x53);
	PUTDST8(RSPRecompPos,0x56);
	PUTDST8(RSPRecompPos,0x57);
	PUTDST16(RSPRecompPos,0x5041);
	PUTDST16(RSPRecompPos,0x5141);
	PUTDST16(RSPRecompPos,0x5241);
	PUTDST16(RSPRecompPos,0x5341);
	PUTDST16(RSPRecompPos,0x5441);
	PUTDST16(RSPRecompPos,0x5541);
	PUTDST16(RSPRecompPos,0x5641);
	PUTDST16(RSPRecompPos,0x5741);
#else
PUTDST8(RSPRecompPos,0x60);
#endif
}

void RSPPush(int32_t x86reg) {

#ifdef USEX64
	PUTDST8(RSPRecompPos, 0x40 | ((x86reg & 0x20) >> 5));
#endif
	switch(x86reg&0xf) {
	case x86_EAX: PUTDST8(RSPRecompPos, 0x50); break;
	case x86_EBX: PUTDST8(RSPRecompPos, 0x53); break;
	case x86_ECX: PUTDST8(RSPRecompPos, 0x51); break;
	case x86_EDX: PUTDST8(RSPRecompPos, 0x52); break;
	case x86_ESI: PUTDST8(RSPRecompPos, 0x56); break;
	case x86_EDI: PUTDST8(RSPRecompPos, 0x57); break;
	case x86_ESP: PUTDST8(RSPRecompPos, 0x54); break;
	case x86_EBP: PUTDST8(RSPRecompPos, 0x55); break;
	}
}

void RSPPop(int32_t x86reg) {
#ifdef USEX64
	PUTDST8(RSPRecompPos, 0x40 | ((x86reg & 0x20) >> 5));
#endif
	switch(x86reg&0xf) {

	case x86_EAX: PUTDST8(RSPRecompPos, 0x58); break;
	case x86_EBX: PUTDST8(RSPRecompPos, 0x5B); break;
	case x86_ECX: PUTDST8(RSPRecompPos, 0x59); break;
	case x86_EDX: PUTDST8(RSPRecompPos, 0x5A); break;
	case x86_ESI: PUTDST8(RSPRecompPos, 0x5E); break;
	case x86_EDI: PUTDST8(RSPRecompPos, 0x5F); break;
	case x86_ESP: PUTDST8(RSPRecompPos, 0x5C); break;
	case x86_EBP: PUTDST8(RSPRecompPos, 0x5D); break;
	}
}

void RSPPushImm32(uint32_t Value) {
	PUTDST8(RSPRecompPos,0x68);
	PUTDST32(RSPRecompPos,Value);
}

void RSPRet(void) {
	PUTDST8(RSPRecompPos,0xC3);
}

void RSPSeta(int32_t x86reg) {
	OPCODE_REG_REG(16,0x970F,OP_D0,x86reg);
}

void RSPSetaVariable(void * Variable) {
    OPCODE_REG_VARIABLE(,16,0x970F,OP_D0,Variable);
}

void RSPSetae(int32_t x86reg) {
	OPCODE_REG_REG(16,0x930F,OP_D0,x86reg);
}

void RSPSetb(int32_t x86reg) {
	OPCODE_REG_REG(16,0x920F,OP_D0,x86reg);
}

void RSPSetbVariable(void * Variable) {
    OPCODE_REG_VARIABLE(,16,0x920F,OP_D0,Variable);
}

void RSPSetg(int32_t x86reg) {
	OPCODE_REG_REG(16,0x9F0F,OP_D0,x86reg);
}

void RSPSetgVariable(void * Variable) {
    OPCODE_REG_VARIABLE(,16,0x9F0F,OP_D0,Variable);
}

void RSPSetl(int32_t x86reg) {
	OPCODE_REG_REG(16,0x9C0F,OP_D0,x86reg);
}

void RSPSetlVariable(void * Variable) {
    OPCODE_REG_VARIABLE(,16,0x9C0F,OP_D0,Variable);
}

void RSPShiftLeftSignVariableImmed(void *Variable, uint8_t Immediate) {
    OPCODE_REG_VARIABLE(,8,0xC1,OP_D4,Variable);
    PUTDST8(RSPRecompPos,Immediate);
}

void RSPShiftRightSignVariableImmed(void *Variable, uint8_t Immediate) {
    OPCODE_REG_VARIABLE(,8,0xC1,OP_D7,Variable);
    PUTDST8(RSPRecompPos,Immediate);
}

void RSPShiftRightUnsignVariableImmed(void *Variable, uint8_t Immediate) {
    OPCODE_REG_VARIABLE(,8,0xC1,OP_D5,Variable);
    PUTDST8(RSPRecompPos,Immediate);
}

void RSPSetzVariable(void *Variable) {
	OPCODE_REG_VARIABLE(,16,0x940F,OP_D0,Variable);
}

void RSPSetnzVariable(void *Variable) {
	OPCODE_REG_VARIABLE(,16,0x950F,OP_D0,Variable);
}

void RSPSetleVariable(void *Variable) {
	RSPBreakPoint();
	OPCODE_REG_VARIABLE(,16,0x9E0F,OP_D0,Variable);
}

void RSPSetgeVariable(void *Variable) {
	RSPBreakPoint();
	OPCODE_REG_VARIABLE(,16,0x9D0F,OP_D0,Variable);
}

void RSPSetz(int32_t x86reg) {
	OPCODE_REG_REG(16,0x940F,OP_D0,x86reg);
}

void RSPSetnz(int32_t x86reg) {
	OPCODE_REG_REG(16,0x950F,OP_D0,x86reg);
}

void RSPShiftLeftDouble(int32_t Destination, int32_t Source) {
	OPCODE_REG_REG(16,0xA50F,Source,Destination);
}

void RSPShiftLeftDoubleImmed(int32_t Destination, int32_t Source, uint8_t Immediate) {
	OPCODE_REG_REG(16,0xA40F,Source,Destination);
	PUTDST8(RSPRecompPos,Immediate);
}

void RSPShiftLeftSign(int32_t x86reg) {
	OPCODE_REG_REG(8,0xD3,OP_D4,x86reg);
}

void RSPShiftLeftSignImmed(int32_t x86reg, uint8_t Immediate) {
	OPCODE_REG_REG(8,0xC1,OP_D4,x86reg);
	PUTDST8(RSPRecompPos,Immediate);
}

void RSPShiftRightSign(int32_t x86reg) {
	OPCODE_REG_REG(8,0xD3,OP_D7,x86reg);
}

void RSPShiftRightSignImmed(int32_t x86reg, uint8_t Immediate) {
	OPCODE_REG_REG(8,0xC1,OP_D7,x86reg);
	PUTDST8(RSPRecompPos,Immediate);
}

void RSPShiftRightUnsign(int32_t x86reg) {
	OPCODE_REG_REG(8,0xD3,OP_D5,x86reg);
}

void RSPShiftRightDouble(int32_t Destination, int32_t Source) {
	OPCODE_REG_REG(16,0xAD0F,Source,Destination);
}

void RSPShiftRightDoubleImmed(int32_t Destination, int32_t Source, uint8_t Immediate) {
	OPCODE_REG_REG(16,0xAC0F,Source,Destination);
	PUTDST8(RSPRecompPos,Immediate);
}

void RSPShiftRightUnsignImmed(int32_t x86reg, uint8_t Immediate) {
	OPCODE_REG_REG(8,0xC1,OP_D5,x86reg);
	PUTDST8(RSPRecompPos,Immediate);
}

void RSPSbbConstFromX86Reg (int32_t x86reg, uint32_t Const) {
RSPBreakPoint();

	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		OPCODE_REG_REG(8,0x81,OP_D3,x86reg);
		PUTDST32(RSPRecompPos, Const);
	} else {
		OPCODE_REG_REG(8,0x83,OP_D3,x86reg);
		PUTDST8(RSPRecompPos, Const);
	}
}

void RSPSbbVariableFromX86reg(int32_t x86reg, void * Variable) {
    RSPBreakPoint();
    OPCODE_REG_VARIABLE(,8,0x1B,x86reg,Variable);
}

void RSPSbbX86RegToX86Reg(int32_t Destination, int32_t Source) {
    OPCODE_REG_REG(8,0x1B,Destination,Source);
}

void RSPSubConstFromVariable (uint32_t Const, void *Variable) {
	OPCODE_REG_VARIABLE(,8,0x81,OP_D5,Variable);
	PUTDST32(RSPRecompPos,Const);
}

void RSPSubConstFromX86Reg (int32_t x86reg, uint32_t Const) {
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		OPCODE_REG_REG(8,0x81,OP_D5,x86reg);
		PUTDST32(RSPRecompPos, Const);
	} else {
		OPCODE_REG_REG(8,0x83,OP_D5,x86reg);
		PUTDST8(RSPRecompPos, Const);
	}
}

void RSPSubVariableFromX86reg(int32_t x86reg, void * Variable) {
    OPCODE_REG_VARIABLE(,8,0x2B,x86reg,Variable);
}

void RSPSubX86RegToX86Reg(int32_t Destination, int32_t Source) {
	OPCODE_REG_REG(8,0x2B,Destination,Source);
}

void RSPTestConstToX86Reg(uint32_t Const, int32_t x86reg) {
    OPCODE_REG_REG(8,0xF7,OP_D0,x86reg);
	PUTDST32(RSPRecompPos,Const);
}

void RSPTestVariable(uint32_t Const, void * Variable) {
	OPCODE_REG_VARIABLE(,8,0xf7,OP_D0,Variable);
	PUTDST32(RSPRecompPos,Const);
}

void RSPTestX86RegToX86Reg(int32_t Destination, int32_t Source) {
	OPCODE_REG_REG(8,0x85,Destination,Source);
}

void RSPTestVariableToX86Reg(uint32_t x86reg, void * Variable) {
    OPCODE_REG_VARIABLE(,8,0x85,x86reg,Variable);
}

void RSPXorConstToX86Reg(int32_t x86reg, uint32_t Const) {
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		OPCODE_REG_REG(8,0x81,OP_D6,x86reg);
		PUTDST32(RSPRecompPos, Const);
	} else {
		OPCODE_REG_REG(8,0x83,OP_D6,x86reg);
		PUTDST8(RSPRecompPos, Const);
	}
}


void RSPXorX86RegToX86Reg(int32_t Source, int32_t Destination) {
	OPCODE_REG_REG(8,0x31,Destination,Source);
}

void RSPXorVariableToX86reg(void *Variable, int32_t x86reg) {
	RSPBreakPoint();
	OPCODE_REG_VARIABLE(,8,0x33,x86reg,Variable);
}


void RSPXorConstToVariable(void *Variable, uint32_t Const) {
    OPCODE_REG_VARIABLE(,8,0x81,OP_D6,Variable);
    PUTDST32(RSPRecompPos,Const);
}

// ********************************************************************************************

void RSPAndX86RegToVariable(void * Variable, int32_t x86Reg) {
	OPCODE_REG_VARIABLE(,8,0x21,x86Reg,Variable);
}

void RSPXorX86RegToVariable(void *Variable, int32_t x86reg) {
	OPCODE_REG_VARIABLE(,8,0x31,x86reg,Variable);
}

void RSPTestConstToVariable(uint32_t Const, void * Variable) {
    OPCODE_REG_VARIABLE(,8,0xf7,OP_D0,Variable);
	PUTDST32(RSPRecompPos,Const);
}

void RSPCondMoveGreaterEqual(int32_t Destination, int32_t Source) {
	if (ConditionalMove == 0) {
		uint8_t * Jump;
		// CPU_Message("   [*]cmovge %s, %s",x86_Name(Destination),x86_Name(Source));

		RSPJlLabel8(0);
		Jump = RSPRecompPos - 1;
		RSPMoveX86RegToX86Reg(Source, Destination);
		// CPU_Message("     label:");
		RSPx86_SetBranch8b(Jump, RSPRecompPos);
	} else {
		uint8_t x86Command;
		// CPU_Message("      cmovge %s, %s",x86_Name(Destination),x86_Name(Source));

		PUTDST16(RSPRecompPos,0x4D0F);

		switch (Source&0xf) {
		case x86_EAX: x86Command = 0x00; break;
		case x86_EBX: x86Command = 0x03; break;
		case x86_ECX: x86Command = 0x01; break;
		case x86_EDX: x86Command = 0x02; break;
		case x86_ESI: x86Command = 0x06; break;
		case x86_EDI: x86Command = 0x07; break;
		case x86_ESP: x86Command = 0x04; break;
		case x86_EBP: x86Command = 0x05; break;
		}

		switch (Destination&0xf) {
		case x86_EAX: x86Command += 0xC0; break;
		case x86_EBX: x86Command += 0xD8; break;
		case x86_ECX: x86Command += 0xC8; break;
		case x86_EDX: x86Command += 0xD0; break;
		case x86_ESI: x86Command += 0xF0; break;
		case x86_EDI: x86Command += 0xF8; break;
		case x86_ESP: x86Command += 0xE0; break;
		case x86_EBP: x86Command += 0xE8; break;
		}

		PUTDST8(RSPRecompPos, x86Command);
	}
}

void RSPMoveX86regPointerToX86regByte(int32_t Destination, int32_t AddrReg) {
	uint8_t x86Command = 0;

	// CPU_Message("      mov %s, byte ptr [%s]",x86Byte_Name(Destination), x86_Name(AddrReg));

	switch (AddrReg&0xf) {
	case x86_EAX: x86Command = 0x00; break;
	case x86_EBX: x86Command = 0x03; break;
	case x86_ECX: x86Command = 0x01; break;
	case x86_EDX: x86Command = 0x02; break;
	case x86_ESI: x86Command = 0x06; break;
	case x86_EDI: x86Command = 0x07; break;
	default: DisplayError("MoveX86regPointerToX86regByte\nUnknown x86 Register");
	}

	switch (Destination&0xf) {
	case x86_EAX: x86Command += 0x00; break;
	case x86_EBX: x86Command += 0x18; break;
	case x86_ECX: x86Command += 0x08; break;
	case x86_EDX: x86Command += 0x10; break;
	default: DisplayError("MoveX86regPointerToX86regByte\nUnknown x86 Register");
	}

	PUTDST8(RSPRecompPos, 0x8A);
	PUTDST8(RSPRecompPos, x86Command);
}

void RSPMoveX86regToN64MemDisp(int32_t x86reg, int32_t AddrReg, uint8_t Disp) {
#ifdef USEX64
	if(((uintptr_t)DMEM - (uintptr_t)TLB_Map) < 0x7FFFFFFF) {
		OPCODE_REG_BASE_INDEX_IMM32(8,0x89,x86reg,x86_R15,AddrReg,((uintptr_t)DMEM - (uintptr_t)TLB_Map) +  Disp);
    } else {
    	LOAD_VARIABLE(x86_TEMP, DMEM);
    	OPCODE_REG_MREG_IMM8(8,0xf7,x86reg,x86_TEMP,Disp);
    }
#else
	OPCODE_REG_MREG_IMM32(8,0x89,x86reg,AddrReg,DMEM + Disp);
#endif
}

void RSPOrVariableToX86regHalf(void * Variable, int32_t x86Reg) {
    OPCODE_REG_VARIABLE(PUTDST8(RSPRecompPos,0x66),8,0xb,x86Reg,Variable);
}

void RSPAndVariableToX86regHalf(void * Variable, int32_t x86Reg) {
    OPCODE_REG_VARIABLE(PUTDST8(RSPRecompPos,0x66),8,0x23,x86Reg,Variable);
}

void RSPAndX86RegHalfToX86RegHalf(int32_t Destination, int32_t Source) {
	PUTDST8(RSPRecompPos, 0x66);
	OPCODE_REG_REG(8,0x21,Source,Destination);
}

void RSPCondMoveNotEqual(int32_t Destination, int32_t Source) {
	if (ConditionalMove == 0) {
		uint8_t * Jump;
		// CPU_Message("   [*]cmovne %s, %s",x86_Name(Destination),x86_Name(Source));

		RSPJeLabel8(0);
		Jump = RSPRecompPos - 1;
		RSPMoveX86RegToX86Reg(Source, Destination);
		// CPU_Message("     label:");
		RSPx86_SetBranch8b(Jump, RSPRecompPos);
	} else {
		uint8_t x86Command;
		// CPU_Message("      cmovne %s, %s",x86_Name(Destination),x86_Name(Source));

		PUTDST16(RSPRecompPos,0x450F);

		switch (Source&0xf) {
		case x86_EAX: x86Command = 0x00; break;
		case x86_EBX: x86Command = 0x03; break;
		case x86_ECX: x86Command = 0x01; break;
		case x86_EDX: x86Command = 0x02; break;
		case x86_ESI: x86Command = 0x06; break;
		case x86_EDI: x86Command = 0x07; break;
		case x86_ESP: x86Command = 0x04; break;
		case x86_EBP: x86Command = 0x05; break;
		}

		switch (Destination&0xf) {
		case x86_EAX: x86Command += 0xC0; break;
		case x86_EBX: x86Command += 0xD8; break;
		case x86_ECX: x86Command += 0xC8; break;
		case x86_EDX: x86Command += 0xD0; break;
		case x86_ESI: x86Command += 0xF0; break;
		case x86_EDI: x86Command += 0xF8; break;
		case x86_ESP: x86Command += 0xE0; break;
		case x86_EBP: x86Command += 0xE8; break;
		}

		PUTDST8(RSPRecompPos, x86Command);
	}
}


void RSPNegateX86reg(int32_t x86reg) {
	// CPU_Message("      neg %s", x86_Name(x86reg));
	switch (x86reg&0xf) {
	case x86_EAX: PUTDST16(RSPRecompPos,0xd8f7); break;
	case x86_EBX: PUTDST16(RSPRecompPos,0xdbf7); break;
	case x86_ECX: PUTDST16(RSPRecompPos,0xd9f7); break;
	case x86_EDX: PUTDST16(RSPRecompPos,0xdaf7); break;
	case x86_ESI: PUTDST16(RSPRecompPos,0xdef7); break;
	case x86_EDI: PUTDST16(RSPRecompPos,0xdff7); break;
	case x86_ESP: PUTDST16(RSPRecompPos,0xdcf7); break;
	case x86_EBP: PUTDST16(RSPRecompPos,0xddf7); break;
	default:
		DisplayError("NegateX86reg\nUnknown x86 Register");
	}
}


void RSPImulX86RegToX86Reg(int32_t Destination, int32_t Source) {
	uint8_t x86Command;

	// CPU_Message("      imul %s, %s",x86_Name(Destination), x86_Name(Source));

	switch (Source&0xf) {
	case x86_EAX: x86Command = 0x00; break;
	case x86_EBX: x86Command = 0x03; break;
	case x86_ECX: x86Command = 0x01; break;
	case x86_EDX: x86Command = 0x02; break;
	case x86_ESI: x86Command = 0x06; break;
	case x86_EDI: x86Command = 0x07; break;
	case x86_ESP: x86Command = 0x04; break;
	case x86_EBP: x86Command = 0x05; break;
	}

	switch (Destination&0xf) {
	case x86_EAX: x86Command += 0xC0; break;
	case x86_EBX: x86Command += 0xD8; break;
	case x86_ECX: x86Command += 0xC8; break;
	case x86_EDX: x86Command += 0xD0; break;
	case x86_ESI: x86Command += 0xF0; break;
	case x86_EDI: x86Command += 0xF8; break;
	case x86_ESP: x86Command += 0xE0; break;
	case x86_EBP: x86Command += 0xE8; break;
	}

	PUTDST16(RSPRecompPos, 0xAF0F);
	PUTDST8(RSPRecompPos, x86Command);
}


void RSPMoveX86RegToX86regPointerDisp ( int32_t Source, int32_t AddrReg, uint8_t Disp ) {
    OPCODE_REG_MREG_IMM8(8,0x89,Source,AddrReg,Disp);
}

void RSPCondMoveGreater(int32_t Destination, int32_t Source) {
	if (ConditionalMove == 0) {
		uint8_t * Jump;
		// CPU_Message("    [*]cmovg %s, %s",x86_Name(Destination),x86_Name(Source));

		RSPJleLabel8(0);
		Jump = RSPRecompPos - 1;
		RSPMoveX86RegToX86Reg(Source, Destination);
		// CPU_Message("     label:");
		RSPx86_SetBranch8b(Jump, RSPRecompPos);
	} else {
		uint8_t x86Command;
		// CPU_Message("      cmovg %s, %s",x86_Name(Destination),x86_Name(Source));

		PUTDST16(RSPRecompPos,0x4F0F);

		switch (Source&0xf) {
		case x86_EAX: x86Command = 0x00; break;
		case x86_EBX: x86Command = 0x03; break;
		case x86_ECX: x86Command = 0x01; break;
		case x86_EDX: x86Command = 0x02; break;
		case x86_ESI: x86Command = 0x06; break;
		case x86_EDI: x86Command = 0x07; break;
		case x86_ESP: x86Command = 0x04; break;
		case x86_EBP: x86Command = 0x05; break;
		}

		switch (Destination&0xf) {
		case x86_EAX: x86Command += 0xC0; break;
		case x86_EBX: x86Command += 0xD8; break;
		case x86_ECX: x86Command += 0xC8; break;
		case x86_EDX: x86Command += 0xD0; break;
		case x86_ESI: x86Command += 0xF0; break;
		case x86_EDI: x86Command += 0xF8; break;
		case x86_ESP: x86Command += 0xE0; break;
		case x86_EBP: x86Command += 0xE8; break;
		}

		PUTDST8(RSPRecompPos, x86Command);
	}
}

void RSPCondMoveLess(int32_t Destination, int32_t Source) {
	if (ConditionalMove == 0) {
		uint8_t * Jump;
		// CPU_Message("   [*]cmovl %s, %s",x86_Name(Destination),x86_Name(Source));

		RSPJgeLabel8(0);
		Jump = RSPRecompPos - 1;
		RSPMoveX86RegToX86Reg(Source, Destination);
		// CPU_Message("     label:");
		RSPx86_SetBranch8b(Jump, RSPRecompPos);
	} else {
		uint8_t x86Command;
		// CPU_Message("      cmovl %s, %s",x86_Name(Destination),x86_Name(Source));

		PUTDST16(RSPRecompPos,0x4C0F);

		switch (Source&0xf) {
		case x86_EAX: x86Command = 0x00; break;
		case x86_EBX: x86Command = 0x03; break;
		case x86_ECX: x86Command = 0x01; break;
		case x86_EDX: x86Command = 0x02; break;
		case x86_ESI: x86Command = 0x06; break;
		case x86_EDI: x86Command = 0x07; break;
		case x86_ESP: x86Command = 0x04; break;
		case x86_EBP: x86Command = 0x05; break;
		}

		switch (Destination&0xf) {
		case x86_EAX: x86Command += 0xC0; break;
		case x86_EBX: x86Command += 0xD8; break;
		case x86_ECX: x86Command += 0xC8; break;
		case x86_EDX: x86Command += 0xD0; break;
		case x86_ESI: x86Command += 0xF0; break;
		case x86_EDI: x86Command += 0xF8; break;
		case x86_ESP: x86Command += 0xE0; break;
		case x86_EBP: x86Command += 0xE8; break;
		}

		PUTDST8(RSPRecompPos, x86Command);
	}
}

void RSPMoveSxX86RegPtrDispToX86RegHalf(int32_t AddrReg, uint8_t Disp, int32_t Destination) {
	OPCODE_REG_MREG_IMM8(16,0xBF0F,Destination,AddrReg,Disp);

}


void RSPMoveZxX86RegPtrDispToX86RegHalf(int32_t AddrReg, uint8_t Disp, int32_t Destination) {
	OPCODE_REG_MREG_IMM8(16,0xB70F,Destination,AddrReg,Disp);
}


void RSPMoveX86regHalfToX86regPointerDisp(int32_t Source, int32_t AddrReg, uint8_t Disp) {
	uint8_t x86Amb;

	// CPU_Message("      mov word ptr [%s+%X], %s",x86_Name(AddrReg), Disp, x86Half_Name(Source));

	switch (AddrReg&0xf) {
	case x86_EAX: x86Amb = 0x00; break;
	case x86_EBX: x86Amb = 0x03; break;
	case x86_ECX: x86Amb = 0x01; break;
	case x86_EDX: x86Amb = 0x02; break;
	case x86_ESI: x86Amb = 0x06; break;
	case x86_EDI: x86Amb = 0x07; break;
	case x86_ESP: x86Amb = 0x04; break;
	case x86_EBP: x86Amb = 0x05; break;
	default: DisplayError("MoveX86regBytePointer\nUnknown x86 Register");
	}

	switch (Source&0xf) {
	case x86_EAX: x86Amb += 0x00; break;
	case x86_ECX: x86Amb += 0x08; break;
	case x86_EDX: x86Amb += 0x10; break;
	case x86_EBX: x86Amb += 0x18; break;
	case x86_ESI: x86Amb += 0x30; break;
	case x86_EDI: x86Amb += 0x38; break;
	case x86_ESP: x86Amb += 0x20; break;
	case x86_EBP: x86Amb += 0x28; break;
	default: DisplayError("MoveX86regBytePointer\nUnknown x86 Register");
	}

	PUTDST16(RSPRecompPos, 0x8966);
	PUTDST8(RSPRecompPos, x86Amb | 0x40);
	PUTDST8(RSPRecompPos, Disp);
}

void RSPCondMoveEqual(int32_t Destination, int32_t Source) {
	if (ConditionalMove == 0) {
		uint8_t * Jump;
		// CPU_Message("   [*]cmove %s, %s",x86_Name(Destination),x86_Name(Source));

		RSPJneLabel8(0);
		Jump = RSPRecompPos - 1;
		RSPMoveX86RegToX86Reg(Source, Destination);
		// CPU_Message("     label:");
		RSPx86_SetBranch8b(Jump, RSPRecompPos);
	} else {
		uint8_t x86Command;
		// CPU_Message("      cmove %s, %s",x86_Name(Destination),x86_Name(Source));

		PUTDST16(RSPRecompPos,0x440F);

		switch (Source&0xf) {
		case x86_EAX: x86Command = 0x00; break;
		case x86_EBX: x86Command = 0x03; break;
		case x86_ECX: x86Command = 0x01; break;
		case x86_EDX: x86Command = 0x02; break;
		case x86_ESI: x86Command = 0x06; break;
		case x86_EDI: x86Command = 0x07; break;
		case x86_ESP: x86Command = 0x04; break;
		case x86_EBP: x86Command = 0x05; break;
		}

		switch (Destination&0xf) {
		case x86_EAX: x86Command += 0xC0; break;
		case x86_EBX: x86Command += 0xD8; break;
		case x86_ECX: x86Command += 0xC8; break;
		case x86_EDX: x86Command += 0xD0; break;
		case x86_ESI: x86Command += 0xF0; break;
		case x86_EDI: x86Command += 0xF8; break;
		case x86_ESP: x86Command += 0xE0; break;
		case x86_EBP: x86Command += 0xE8; break;
		}

		PUTDST8(RSPRecompPos, x86Command);
	}
}


void RSPCwd(void) {
	// CPU_Message("      cwd");
	PUTDST16(RSPRecompPos, 0x9966);
}

void RSPCwde(void) {
	// CPU_Message("      cwde");
	PUTDST8(RSPRecompPos, 0x98);
}

void RSPSubX86regFromVariable(int32_t x86reg, void * Variable) {
    OPCODE_REG_VARIABLE(,8,0x29,x86reg,Variable);
}

void RSPJumpX86Reg( int32_t x86reg ) {
	// CPU_Message("      jmp %s",x86_Name(x86reg));
/*
	switch (x86reg) {
	case x86_EAX: PUTDST16(RSPRecompPos,0xe0ff); break;
	case x86_EBX: PUTDST16(RSPRecompPos,0xe3ff); break;
	case x86_ECX: PUTDST16(RSPRecompPos,0xe1ff); break;
	case x86_EDX: PUTDST16(RSPRecompPos,0xe2ff); break;
	case x86_ESI: PUTDST16(RSPRecompPos,0xe6ff); break;
	case x86_EDI: PUTDST16(RSPRecompPos,0xe7ff); break;
	default: DisplayError("JumpX86Reg: Unknown reg (%08x)", x86reg); Int3();
	}*/
	OPCODE_REG_REG(8,0xFF,OP_D4,x86reg);
}




void RSPJgeLabel8(uint8_t Value) {
	PUTDST8(RSPRecompPos,0x7D);
	PUTDST8(RSPRecompPos,Value);
}

