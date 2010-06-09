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
#include "x86.h"
#include "types.h"

uint8_t Index[9] = {0,0,0x40,0,0x80,0,0,0,0xC0};


void AdcX86regToVariable(int32_t x86reg, void * Variable) {
	OPCODE_REG_VARIABLE(,8,0x11,x86reg,Variable);
}

void AdcConstToVariable(void *Variable, uint8_t Constant) {
	OPCODE_REG_VARIABLE(,8,0x81,OP_D2,Variable);
	PUTDST8(RecompPos,Constant);
}

void AdcConstToX86Reg (int32_t x86reg, uint32_t Const) {
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		OPCODE_REG_REG(8,0x81,OP_D2,x86reg);
		PUTDST32(RecompPos, Const);
	} else {
		OPCODE_REG_REG(8,0x83,OP_D2,x86reg);
		PUTDST8(RecompPos, Const);
	}
}

void AdcVariableToX86reg(int32_t x86reg, void * Variable) {
	OPCODE_REG_VARIABLE(,8,0x13	,x86reg,Variable);
}

void AdcX86RegToX86Reg(int32_t Destination, int32_t Source) {
	OPCODE_REG_REG(8,0x13,Destination,Source);
}

void AddConstToVariable (uint32_t Const, void *Variable) {
    OPCODE_REG_VARIABLE(,8,0x81,OP_D0,Variable);
    PUTDST32(RecompPos,Const);
}

void AddConstToX86Reg64 (int32_t x86reg, uint32_t Const) {
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		OPCODE_REG_REG(8,0x81,OP_D0,x86reg | x64_Reg);
		PUTDST32(RecompPos, Const);
	} else {
		OPCODE_REG_REG(8,0x83,OP_D0,x86reg | x64_Reg);
		PUTDST8(RecompPos, Const);
	}
}

void AddConstToX86Reg (int32_t x86reg, uint32_t Const) {
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		OPCODE_REG_REG(8,0x81,OP_D0,x86reg);
		PUTDST32(RecompPos, Const);
	} else {
		OPCODE_REG_REG(8,0x83,OP_D0,x86reg);
		PUTDST8(RecompPos, Const);
	}
}

void AddVariableToX86reg(int32_t x86reg, void * Variable) {
    OPCODE_REG_VARIABLE(,8,0x3,x86reg,Variable);
}

void AddX86regToVariable(int32_t x86reg, void * Variable) {
    OPCODE_REG_VARIABLE(,8,0x1,x86reg,Variable);
}

void AddX86RegToX86Reg(int32_t Destination, int32_t Source) {
	OPCODE_REG_REG(8,0x3,Destination,Source);
}

void AndConstToVariable (uint32_t Const, void *Variable) {
    OPCODE_REG_VARIABLE(,8,0x81,OP_D4,Variable);
    PUTDST32(RecompPos,Const);
}

void AndConstToX86Reg(int32_t x86reg, uint32_t Const) {
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		OPCODE_REG_REG(8,0x81,OP_D4,x86reg);
		PUTDST32(RecompPos, Const);
	} else {
		OPCODE_REG_REG(8,0x83,OP_D4,x86reg);
		PUTDST8(RecompPos, Const);
	}
}

void AndVariableDispToX86Reg(void *Variable, int32_t x86reg, int32_t AddrReg, int32_t Multiplier) {
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

void AndVariableToX86Reg(void * Variable, int32_t x86reg) {
    OPCODE_REG_VARIABLE(,8,0x23,x86reg,Variable);
}

void AndX86RegToX86Reg(int32_t Destination, int32_t Source) {
	OPCODE_REG_REG(8,0x21,Source,Destination);
}

void BreakPoint (void) {
	PUTDST8(RecompPos,0xCC);
}

void Call_Direct(void * FunctAddress) {
	uintptr_t disp;
#ifdef USEX64
	disp = (uintptr_t)FunctAddress-(uintptr_t)RecompPos - 5;
	SubConstFromX86Reg(x86_RSP, 0x28);
	if(disp <= 0x7fffffff) {
		PUTDST8(RecompPos,0xE8);
		PUTDST32(RecompPos,disp);
	} else {
		LOAD_VARIABLE(x86_TEMP, FunctAddress);
		OPCODE_REG_REG(8,0xff,OP_D2,x86_TEMP);

	}
	AddConstToX86Reg(x86_RSP, 0x28);
#else
	//BreakPoint();
	disp = (uintptr_t)FunctAddress-(uintptr_t)RecompPos - 5;
	PUTDST8(RecompPos,0xE8);
	PUTDST32(RecompPos,disp);
#endif
}

void CompConstToVariable(uint32_t Const, void * Variable) {
    OPCODE_REG_VARIABLE(,8,0x81,OP_D7,Variable);
    PUTDST32(RecompPos,Const);
}

void CompConstToX86reg(int32_t x86reg, uint32_t Const) {
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		OPCODE_REG_REG(8,0x81,OP_D7,x86reg);
		PUTDST32(RecompPos,Const);
	} else {
		OPCODE_REG_REG(8,0x83,OP_D7,x86reg);
		PUTDST8(RecompPos, Const);
	}
}

void CompX86regToVariable(int32_t x86reg, void * Variable) {
    OPCODE_REG_VARIABLE(,8,0x3B,x86reg,Variable);
}

void CompX86RegToX86Reg(int32_t Destination, int32_t Source) {
	OPCODE_REG_REG(8,0x3B,Destination,Source);
}

void DecX86reg(x86reg) {
	OPCODE_REG_REG(8,0xFF,OP_D1,x86reg);
}

void DivX86reg(int32_t x86reg) {
	OPCODE_REG_REG(8,0xF7,OP_D6,x86reg);
}

void idivX86reg(int32_t x86reg) {
	OPCODE_REG_REG(8,0xF7,OP_D7,x86reg);
}

void imulX86reg(int32_t x86reg) {
	OPCODE_REG_REG(8,0xF7,OP_D5,x86reg);
}

void IncX86reg(int32_t x86reg) {
	OPCODE_REG_REG(8,0xFF,OP_D0,x86reg);
}

void JaeLabel8( uint8_t Value) {
	PUTDST8(RecompPos,0x73);
	PUTDST8(RecompPos,Value);
}

void JaeLabel32(uint32_t Value) {
	PUTDST16(RecompPos,0x830F);
	PUTDST32(RecompPos,Value);
}

void JaLabel8( uint8_t Value) {
	PUTDST8(RecompPos,0x77);
	PUTDST8(RecompPos,Value);
}

void JaLabel32(uint32_t Value) {
	PUTDST16(RecompPos,0x870F);
	PUTDST32(RecompPos,Value);
}

void JbLabel8( uint8_t Value) {
	PUTDST8(RecompPos,0x72);
	PUTDST8(RecompPos,Value);
}

void JbLabel32(uint32_t Value) {
	PUTDST16(RecompPos,0x820F);
	PUTDST32(RecompPos,Value);
}

void JecxzLabel8( uint8_t Value) {
	PUTDST8(RecompPos,0xE3);
	PUTDST8(RecompPos,Value);
}

void JeLabel8( uint8_t Value) {
	PUTDST8(RecompPos,0x74);
	PUTDST8(RecompPos,Value);
}

void JeLabel32(uint32_t Value) {
	PUTDST16(RecompPos,0x840F);
	PUTDST32(RecompPos,Value);
}

void JgeLabel32(uint32_t Value) {
	PUTDST16(RecompPos,0x8D0F);
	PUTDST32(RecompPos,Value);
}

void JgLabel8( uint8_t Value) {
	PUTDST8(RecompPos,0x7F);
	PUTDST8(RecompPos,Value);
}

void JgLabel32(uint32_t Value) {
	PUTDST16(RecompPos,0x8F0F);
	PUTDST32(RecompPos,Value);
}

void JleLabel8( uint8_t Value) {
	PUTDST8(RecompPos,0x7E);
	PUTDST8(RecompPos,Value);
}

void JleLabel32(uint32_t Value) {
	PUTDST16(RecompPos,0x8E0F);
	PUTDST32(RecompPos,Value);
}

void JlLabel8( uint8_t Value) {
	PUTDST8(RecompPos,0x7C);
	PUTDST8(RecompPos,Value);
}

void JlLabel32(uint32_t Value) {
	PUTDST16(RecompPos,0x8C0F);
	PUTDST32(RecompPos,Value);
}

void JzLabel8( uint8_t Value) {
	PUTDST8(RecompPos,0x74);
	PUTDST8(RecompPos,Value);
}

void JnzLabel8( uint8_t Value) {
	PUTDST8(RecompPos,0x75);
	PUTDST8(RecompPos,Value);
}


void JmpDirectReg( int32_t x86reg ) {
	OPCODE_REG_REG(8,0xff,OP_D4,x86reg);
}

void JmpLabel8( uint8_t Value) {
	PUTDST8(RecompPos,0xEB);
	PUTDST8(RecompPos,Value);
}

void JmpLabel32( uint32_t Value) {
	PUTDST8(RecompPos,0xE9);
	PUTDST32(RecompPos,Value);
}

void JneLabel8( uint8_t Value) {
	PUTDST8(RecompPos,0x75);
	PUTDST8(RecompPos,Value);
}

void JneLabel32(uint32_t Value) {
	PUTDST16(RecompPos,0x850F);
	PUTDST32(RecompPos,Value);
}

void JnsLabel8( uint8_t Value) {
	PUTDST8(RecompPos,0x79);
	PUTDST8(RecompPos,Value);
}

void JnsLabel32(uint32_t Value) {
	PUTDST16(RecompPos,0x890F);
	PUTDST32(RecompPos,Value);
}

void JsLabel32(uint32_t Value) {
	PUTDST16(RecompPos,0x880F);
	PUTDST32(RecompPos,Value);
}

void LeaRegReg(int32_t x86RegDest, int32_t x86RegSrc, int32_t multiplier) {
	OPCODE_REG_BASE_INDEX_SCALE(8,0x8D,x86RegDest,x86_EBP,x86RegSrc,Index[multiplier]);
	PUTDST32(RecompPos,0x00000000);
}

void LeaSourceAndOffset(int32_t x86DestReg, int32_t x86SourceReg, int32_t offset) {
    OPCODE_REG_MREG_IMM32(8,0x8D,x86DestReg,x86SourceReg,offset);
}

void MoveConstByteToVariable (uint8_t Const,void *Variable) {
    OPCODE_REG_VARIABLE(,8,0xC6,OP_D0,Variable);
    PUTDST8(RecompPos,Const);
}

void MoveConstHalfToVariable (uint16_t Const,void *Variable) {
    OPCODE_REG_VARIABLE(PUTDST8(RecompPos,0x66),8,0xC7,OP_D0,Variable);
    PUTDST16(RecompPos,Const);
}

void MoveConstHalfToX86regPointer(uint16_t Const, int32_t AddrReg1, int32_t AddrReg2) {
    PUTDST8(RecompPos,0x66);
    OPCODE_REG_BASE_INDEX(8,0xC7,OP_D0,AddrReg1,AddrReg2);
    PUTDST16(RecompPos,Const);
}

void MoveConstToVariable (uint32_t Const,void *Variable) {
    OPCODE_REG_VARIABLE(,8,0xC7,OP_D0,Variable);
    PUTDST32(RecompPos,Const);
}

void MoveConstToX86Pointer(uint32_t Const, int32_t X86Pointer) {
    OPCODE_REG_MREG(8,0xC7,OP_D0,X86Pointer);
    PUTDST32(RecompPos,Const);
}


void MoveConstToX86reg(uint32_t Const, int32_t x86reg) {
	OPCODE_REG_REG(8,0xC7,OP_D0,x86reg);
    PUTDST32(RecompPos,Const);
}

void MoveConstQwordToX86reg(uint64_t Const, int32_t x86reg) {
    PUTDST8(RecompPos, 0x48 | ((x86reg&0x20)>>5));
    PUTDST8(RecompPos, 0xB8 | ((x86reg-1)&0x7));
    PUTDST64(RecompPos,Const);
}

void MoveConstByteToX86regPointer(uint8_t Const, int32_t AddrReg1, int32_t AddrReg2) {
	OPCODE_REG_BASE_INDEX(8,0xC6,OP_D0,AddrReg1,AddrReg2)
    PUTDST8(RecompPos,Const);
}

void MoveConstToX86regPointer(uint32_t Const, int32_t AddrReg1, int32_t AddrReg2) {
	OPCODE_REG_BASE_INDEX(8,0xC7,OP_D0,AddrReg1,AddrReg2);
    PUTDST32(RecompPos,Const);
}

void MoveSxByteX86regPointerToX86reg(int32_t AddrReg1, int32_t AddrReg2, int32_t x86reg) {
	OPCODE_REG_BASE_INDEX(16,0xBE0F,x86reg,AddrReg1,AddrReg2);
}

void MoveSxHalfX86regPointerToX86reg(int32_t AddrReg1, int32_t AddrReg2, int32_t x86reg) {
    OPCODE_REG_BASE_INDEX(16,0xBF0F,x86reg,AddrReg1,AddrReg2);
}

void MoveSxVariableToX86regByte(void *Variable, int32_t x86reg) {
    OPCODE_REG_VARIABLE(,16,0xBE0F,x86reg,Variable);
}

void MoveSxVariableToX86regHalf(void *Variable, int32_t x86reg) {
    OPCODE_REG_VARIABLE(,16,0xBF0F,x86reg,Variable);
}

void MoveVariableToX86reg(void *Variable, int32_t x86reg) {
	OPCODE_REG_VARIABLE(,8,0x8B,x86reg,Variable);
}

void MovePointerToX86reg(void *Variable, int32_t x86reg) {
#ifdef USEX64
    OPCODE_REG_VARIABLE(,8,0x8B,x86reg|x64_Reg,Variable);
#else
    OPCODE_REG_VARIABLE(,8,0x8B,x86reg,Variable);
#endif
}

void MoveX86RegDispToX86Reg(int32_t x86reg, int32_t AddrReg, int32_t IndexReg, int32_t Multiplier) {
	OPCODE_REG_BASE_INDEX_SCALE(8,0x8B,x86reg,AddrReg,IndexReg,Index[Multiplier]);
}

void MoveVariableDispToX86Reg(void *Variable, int32_t x86reg, int32_t AddrReg, int32_t Multiplier) {
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

void MoveVariableToX86regByte(void *Variable, int32_t x86reg) {
	OPCODE_REG_VARIABLE(,8,0x8A,x86reg,Variable);
}

void MoveVariableToX86regHalf(void *Variable, int32_t x86reg) {
	BreakPoint();
    OPCODE_REG_VARIABLE(PUTDST8(RecompPos,0x66),8,0x8B,x86reg,Variable);
}

void MoveX86regByteToVariable(int32_t x86reg, void * Variable) {
    OPCODE_REG_VARIABLE(,8,0x88,x86reg,Variable);
}

void MoveX86regByteToX86regPointer(int32_t x86reg, int32_t AddrReg1, int32_t AddrReg2) {
	OPCODE_REG_BASE_INDEX(8,0x88,x86reg,AddrReg1,AddrReg2);
}

void MoveX86regHalfToVariable(int32_t x86reg, void * Variable) {
    OPCODE_REG_VARIABLE(PUTDST8(RecompPos,0x66), 8,0x89,x86reg,Variable);
}

void MoveX86regHalfToX86regPointer(int32_t x86reg, int32_t AddrReg1, int32_t AddrReg2) {
	PUTDST8(RecompPos,0x66);
	OPCODE_REG_BASE_INDEX(8,0x89,x86reg,AddrReg1,AddrReg2);
}

void MoveX86PointerToX86reg(int32_t x86reg, int32_t X86Pointer) {
	OPCODE_REG_MREG(8,0x8B,x86reg,X86Pointer);
}

void MoveX86regPointerToX86reg(int32_t AddrReg1, int32_t AddrReg2, int32_t x86reg) {
	OPCODE_REG_BASE_INDEX(8,0x8B,x86reg,AddrReg1,AddrReg2);
}

void MoveX86regPointerToX86regDisp8(int32_t AddrReg1, int32_t AddrReg2, int32_t x86reg, uint8_t offset) {
	OPCODE_REG_BASE_INDEX_IMM8(8,0x8B,x86reg,AddrReg1,AddrReg2,offset);
}

void MoveX86regToMemory(int32_t x86reg, int32_t AddrReg, uint32_t Disp) {
	OPCODE_REG_MREG_IMM32(8,0x89,x86reg,AddrReg,Disp);
}

void MoveX86regToVariable(int32_t x86reg, void * Variable) {
    OPCODE_REG_VARIABLE(,8,0x89,x86reg,Variable);
}

void MoveX86RegToX86Reg(int32_t Source, int32_t Destination) {
	OPCODE_REG_REG(8,0x89,Source|X64_Reg, Destination|X64_Reg);
}

void MoveX86regToX86Pointer(int32_t x86reg, int32_t X86Pointer) {
	OPCODE_REG_MREG(8,0x89,x86reg, X86Pointer);
}

void MoveX86regToX86regPointer(int32_t x86reg, int32_t AddrReg1, int32_t AddrReg2) {
    OPCODE_REG_BASE_INDEX(8,0x89,x86reg,AddrReg1,AddrReg2);
}

void MoveZxByteX86regPointerToX86reg(int32_t AddrReg1, int32_t AddrReg2, int32_t x86reg) {
	OPCODE_REG_BASE_INDEX(16,0xB60F,x86reg,AddrReg1,AddrReg2);
}

void MoveZxHalfX86regPointerToX86reg(int32_t AddrReg1, int32_t AddrReg2, int32_t x86reg) {
	OPCODE_REG_BASE_INDEX(16,0xB70F,x86reg,AddrReg1,AddrReg2);
}

void MoveZxVariableToX86regByte(void *Variable, int32_t x86reg) {
    OPCODE_REG_VARIABLE(,16,0xB60F,x86reg,Variable);
}

void MoveZxVariableToX86regHalf(void *Variable, int32_t x86reg) {
    OPCODE_REG_VARIABLE(,16,0xB70F,x86reg,Variable);
}

void MulX86reg(int32_t x86reg) {
	OPCODE_REG_REG(8,0xF7,OP_D4,x86reg);
}

void NotX86Reg(int32_t  x86reg) {
	OPCODE_REG_REG(8,0xF7,OP_D2,x86reg);
}

void OrConstToVariable(uint32_t Const, void * Variable) {
	if(Const < 0x80) {
		OPCODE_REG_VARIABLE(,8,0x83,OP_D1,Variable);
		PUTDST8(RecompPos,Const);
	} else {
		OPCODE_REG_VARIABLE(,8,0x81,OP_D1,Variable);
		PUTDST32(RecompPos,Const);
	}
}

void OrConstToX86Reg(uint32_t Const, int32_t  x86reg) {
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		OPCODE_REG_REG(8,0x81,OP_D1,x86reg);
		PUTDST32(RecompPos, Const);
	} else {
		OPCODE_REG_REG(8,0x83,OP_D1,x86reg);
		PUTDST8(RecompPos, Const);
	}
}

void OrVariableToX86Reg(void * Variable, int32_t x86reg) {
    OPCODE_REG_VARIABLE(,8,0xb,x86reg,Variable);
}

void OrX86RegToVariable(void * Variable, int32_t x86reg) {
    OPCODE_REG_VARIABLE(,8,0x9,x86reg,Variable);
}

void OrX86RegToX86Reg(int32_t Destination, int32_t Source) {
    OPCODE_REG_REG(8,0x0B,Destination,Source);
}

void Pushfd() {
	PUTDST8(RecompPos,0x9c);
}

void Popfd() {
	PUTDST8(RecompPos,0x9d);
}

void Popad(void) {
#ifdef USEX64
	PUTDST16(RecompPos,0x5f41);
	PUTDST16(RecompPos,0x5e41);
	PUTDST16(RecompPos,0x5d41);
	PUTDST16(RecompPos,0x5c41);
	PUTDST16(RecompPos,0x5b41);
	PUTDST16(RecompPos,0x5a41);
	PUTDST16(RecompPos,0x5941);
	PUTDST16(RecompPos,0x5841);
	PUTDST8(RecompPos,0x5f);
	PUTDST8(RecompPos,0x5e);
	PUTDST8(RecompPos,0x5b);
	PUTDST8(RecompPos,0x5a);
	PUTDST8(RecompPos,0x59);
	PUTDST8(RecompPos,0x58);
#else
	PUTDST8(RecompPos,0x61);
#endif

}

void Pushad(void) {
#ifdef USEX64
	PUTDST8(RecompPos,0x50);
	PUTDST8(RecompPos,0x51);
	PUTDST8(RecompPos,0x52);
	PUTDST8(RecompPos,0x53);
	PUTDST8(RecompPos,0x56);
	PUTDST8(RecompPos,0x57);
	PUTDST16(RecompPos,0x5041);
	PUTDST16(RecompPos,0x5141);
	PUTDST16(RecompPos,0x5241);
	PUTDST16(RecompPos,0x5341);
	PUTDST16(RecompPos,0x5441);
	PUTDST16(RecompPos,0x5541);
	PUTDST16(RecompPos,0x5641);
	PUTDST16(RecompPos,0x5741);
#else
	PUTDST8(RecompPos,0x60);
#endif
}

void Push(int32_t x86reg) {
#ifdef USEX64
	PUTDST8(RecompPos, 0x40 | ((x86reg & 0x20) >> 5));
#endif
	switch(x86reg&0xf) {
	case x86_EAX: PUTDST8(RecompPos, 0x50); break;
	case x86_EBX: PUTDST8(RecompPos, 0x53); break;
	case x86_ECX: PUTDST8(RecompPos, 0x51); break;
	case x86_EDX: PUTDST8(RecompPos, 0x52); break;
	case x86_ESI: PUTDST8(RecompPos, 0x56); break;
	case x86_EDI: PUTDST8(RecompPos, 0x57); break;
	case x86_ESP: PUTDST8(RecompPos, 0x54); break;
	case x86_EBP: PUTDST8(RecompPos, 0x55); break;
	}
}

void Pop(int32_t x86reg) {
#ifdef USEX64
	PUTDST8(RecompPos, 0x40 | ((x86reg & 0x20) >> 5));
#endif
	switch(x86reg&0xf) {

	case x86_EAX: PUTDST8(RecompPos, 0x58); break;
	case x86_EBX: PUTDST8(RecompPos, 0x5B); break;
	case x86_ECX: PUTDST8(RecompPos, 0x59); break;
	case x86_EDX: PUTDST8(RecompPos, 0x5A); break;
	case x86_ESI: PUTDST8(RecompPos, 0x5E); break;
	case x86_EDI: PUTDST8(RecompPos, 0x5F); break;
	case x86_ESP: PUTDST8(RecompPos, 0x5C); break;
	case x86_EBP: PUTDST8(RecompPos, 0x5D); break;
	}
}

void PushImm32(uint32_t Value) {
	PUTDST8(RecompPos,0x68);
	PUTDST32(RecompPos,Value);
}

void Ret(void) {
	PUTDST8(RecompPos,0xC3);
}

void Seta(int32_t x86reg) {
	OPCODE_REG_REG(16,0x970F,OP_D0,x86reg);
}

void SetaVariable(void * Variable) {
    OPCODE_REG_VARIABLE(,16,0x970F,OP_D0,Variable);
}

void Setae(int32_t x86reg) {
	OPCODE_REG_REG(16,0x930F,OP_D0,x86reg);
}

void Setb(int32_t x86reg) {
	OPCODE_REG_REG(16,0x920F,OP_D0,x86reg);
}

void SetbVariable(void * Variable) {
    OPCODE_REG_VARIABLE(,16,0x920F,OP_D0,Variable);
}

void Setg(int32_t x86reg) {
	OPCODE_REG_REG(16,0x9F0F,OP_D0,x86reg);
}

void SetgVariable(void * Variable) {
    OPCODE_REG_VARIABLE(,16,0x9F0F,OP_D0,Variable);
}

void Setl(int32_t x86reg) {
	OPCODE_REG_REG(16,0x9C0F,OP_D0,x86reg);
}

void SetlVariable(void * Variable) {
    OPCODE_REG_VARIABLE(,16,0x9C0F,OP_D0,Variable);
}


void Setz(int32_t x86reg) {
	OPCODE_REG_REG(16,0x940F,OP_D0,x86reg);
}

void Setnz(int32_t x86reg) {
	OPCODE_REG_REG(16,0x950F,OP_D0,x86reg);
}

void ShiftLeftDouble(int32_t Destination, int32_t Source) {
	OPCODE_REG_REG(16,0xA50F,Source,Destination);
}

void ShiftLeftDoubleImmed(int32_t Destination, int32_t Source, uint8_t Immediate) {
	OPCODE_REG_REG(16,0xA40F,Source,Destination);
	PUTDST8(RecompPos,Immediate);
}

void ShiftLeftSign(int32_t x86reg) {
	OPCODE_REG_REG(8,0xD3,OP_D4,x86reg);
}

void ShiftLeftSignImmed(int32_t x86reg, uint8_t Immediate) {
	OPCODE_REG_REG(8,0xC1,OP_D4,x86reg);
	PUTDST8(RecompPos,Immediate);
}

void ShiftRightSign(int32_t x86reg) {
	OPCODE_REG_REG(8,0xD3,OP_D7,x86reg);
}

void ShiftRightSignImmed(int32_t x86reg, uint8_t Immediate) {
	OPCODE_REG_REG(8,0xC1,OP_D7,x86reg);
	PUTDST8(RecompPos,Immediate);
}

void ShiftRightUnsign(int32_t x86reg) {
	OPCODE_REG_REG(8,0xD3,OP_D5,x86reg);
}

void ShiftRightDouble(int32_t Destination, int32_t Source) {
	OPCODE_REG_REG(16,0xAD0F,Source,Destination);
}

void ShiftRightDoubleImmed(int32_t Destination, int32_t Source, uint8_t Immediate) {
	OPCODE_REG_REG(16,0xAC0F,Source,Destination);
	PUTDST8(RecompPos,Immediate);
}

void ShiftRightUnsignImmed(int32_t x86reg, uint8_t Immediate) {
	OPCODE_REG_REG(8,0xC1,OP_D5,x86reg);
	PUTDST8(RecompPos,Immediate);
}

void SbbConstFromX86Reg (int32_t x86reg, uint32_t Const) {
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		OPCODE_REG_REG(8,0x81,OP_D3,x86reg);
		PUTDST32(RecompPos, Const);
	} else {
		OPCODE_REG_REG(8,0x83,OP_D3,x86reg);
		PUTDST8(RecompPos, Const);
	}
}

void SbbVariableFromX86reg(int32_t x86reg, void * Variable) {
	OPCODE_REG_VARIABLE(,8,0x1b,x86reg,Variable);
}

void SbbX86RegToX86Reg(int32_t Destination, int32_t Source) {
	OPCODE_REG_REG(8,0x1B,Destination,Source);
}

void SubConstFromVariable (uint32_t Const, void *Variable) {
    OPCODE_REG_VARIABLE(,8,0x81,OP_D5,Variable);
	PUTDST32(RecompPos,Const);
}

void SubConstFromX86Reg (int32_t x86reg, uint32_t Const) {
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		OPCODE_REG_REG(8,0x81,OP_D5,x86reg);
		PUTDST32(RecompPos, Const);
	} else {
		OPCODE_REG_REG(8,0x83,OP_D5,x86reg);
		PUTDST8(RecompPos, Const);
	}
}

void SubVariableFromX86reg(int32_t x86reg, void * Variable) {
    OPCODE_REG_VARIABLE(,8,0x2B,x86reg,Variable);
}

void SubX86RegToX86Reg(int32_t Destination, int32_t Source) {
	OPCODE_REG_REG(8,0x2B,Destination,Source);
}

void TestConstToX86Reg(uint32_t Const, int32_t x86reg) {
    OPCODE_REG_REG(8,0xF7,OP_D0,x86reg);
	PUTDST32(RecompPos,Const);
}

void TestVariable(uint32_t Const, void * Variable) {
	OPCODE_REG_VARIABLE(,8,0xf7,OP_D0,Variable);
	PUTDST32(RecompPos,Const);
}

void TestX86RegToX86Reg(int32_t Destination, int32_t Source) {
	OPCODE_REG_REG(8,0x85,Destination,Source);
}

void TestVariableToX86Reg(uint32_t x86reg, void * Variable) {
    OPCODE_REG_VARIABLE(,8,0x85,x86reg,Variable);
}

void XorConstToX86Reg(int32_t x86reg, uint32_t Const) {
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		OPCODE_REG_REG(8,0x81,OP_D6,x86reg);
		PUTDST32(RecompPos, Const);
	} else {
		OPCODE_REG_REG(8,0x83,OP_D6,x86reg);
		PUTDST8(RecompPos, Const);
	}
}

void XorX86RegToX86Reg(int32_t Source, int32_t Destination) {
	OPCODE_REG_REG(8,0x31,Destination,Source);
}

void XorVariableToX86reg(void *Variable, int32_t x86reg) {
	OPCODE_REG_VARIABLE(,8,0x33,x86reg,Variable);
}


