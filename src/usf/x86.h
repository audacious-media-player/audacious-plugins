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


#define PUTDST8(dest,value)  {(*((uint8_t *)(dest))=(uint8_t)(value)); dest += 1;}
#define PUTDST16(dest,value) {(*((uint16_t *)(dest))=(uint16_t)(value)); dest += 2;}
#define PUTDST32(dest,value) {(*((uint32_t *)(dest))=(uint32_t)(value)); dest += 4;}
#define PUTDST64(dest,value) {(*((uint64_t *)(dest))=(uint64_t)(value)); dest += 8;}

#include "types.h"

#ifdef USEX64

#define x64_Reg		0x10
#define X64_Reg		0x10
#define X64_Ext		0x20

extern uint8_t Index[9];

enum x86RegValues {

    x86_Any	= 0,
    x86_EAX,x86_ECX,x86_EDX,x86_EBX,x86_ESP,x86_EBP,x86_ESI,x86_EDI,x86_Any8Bit=0x40,x64_Any = 0x40,

    x86_RAX = 0x11,x86_RCX,x86_RDX,x86_RBX,x86_RSP,x86_RBP,x86_RSI,x86_RDI,

	x86_R8D = 0x21,x86_R9D,x86_R10D,x86_R11D,x86_R12D,x86_R13D,x86_R14D,x86_R15D,

	x86_R8 = 0x31,x86_R9,x86_R10,x86_R11,x86_R12,x86_R13,x86_R14,x86_R15,

};

enum x86FpuValues {
	x86_ST0,x86_ST1,x86_ST2,x86_ST3,x86_ST4,x86_ST5,x86_ST6,x86_ST7
};

#define	x86_TEMP		x86_R8
#define	x86_TEMPD		x86_R8D

#define OP_D0	1
#define OP_D1	2
#define OP_D2	3
#define OP_D3	4
#define OP_D4	5
#define OP_D5	6
#define OP_D6	7
#define OP_D7	8

#define	LOAD_VARIABLE(reg,variable) \
	PUTDST8(RecompPos, 0x48 | (((reg)&0x20)>>5)); \
	PUTDST8(RecompPos,0xB8 | ((reg)-1)&0xf); \
	PUTDST64(RecompPos,variable);

// 	// 43 0F B6 0C 0D 40 83 C7 04 movzx       ecx,byte ptr [r9+4C78340h]

#define OPCODE_REG_REG(oplen,opcode,reg,rm) \
	PUTDST8(RecompPos, 0x40 | (((rm)&0x20)>>5) | ((((rm|reg))&0x10)>>1) | (((reg)&0x20)>>3)); \
	PUTDST##oplen  (RecompPos, opcode); \
	PUTDST8(RecompPos, 0xC0 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3));

#define OPCODE_REG_MREG(oplen,opcode,reg,rm) \
	PUTDST8(RecompPos, 0x40 | (((rm)&0x20)>>5) | (((reg)&0x10)>>1) | (((reg)&0x20)>>3)); \
	PUTDST##oplen  (RecompPos, opcode); \
	if(((rm)&0xf)==0x6) { \
		PUTDST8(RecompPos, 0x40 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
		PUTDST8(RecompPos, 0); \
	} else if(((rm)&0xf)==0x5) { \
		PUTDST8(RecompPos, 0x0 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
		PUTDST8(RecompPos, 0x24); \
	} else { \
		PUTDST8(RecompPos, 0x00 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
	}

#define OPCODE_REG_MREG_IMM32(oplen,opcode,reg,rm,imm32) \
	PUTDST8(RecompPos, 0x40 | (((rm)&0x20)>>5) | (((reg)&0x10)>>1) | (((reg)&0x20)>>3)); \
	PUTDST##oplen  (RecompPos, opcode); \
	if(((rm)&0xf)==0x5) { \
		PUTDST8(RecompPos, 0x80 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
		PUTDST8(RecompPos, 0x24); \
	} else { \
		PUTDST8(RecompPos, 0x80 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
	} \
	PUTDST32(RecompPos,imm32);

#define OPCODE_REG_BASE_INDEX(oplen,opcode,reg,base,index) \
    PUTDST8(RecompPos, 0x40 | (((base)&0x20)>>5) | (((reg)&0x10)>>1) | (((reg)&0x20)>>3)| (((index)&0x20)>>4)); \
	PUTDST##oplen  (RecompPos, opcode); \
	if(((base)&0xf)!=0x6) { \
	PUTDST8(RecompPos, 0x04 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RecompPos, 0x00 | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	} else {\
	PUTDST8(RecompPos, 0x44 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RecompPos, 0x00 | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	PUTDST8(RecompPos, 0); \
	}

#define OPCODE_REG_MREG_IMM8(oplen,opcode,reg,rm,imm8) \
	PUTDST8(RecompPos, 0x40 | (((rm)&0x20)>>5) | (((reg)&0x10)>>1) | (((reg)&0x20)>>3)); \
	PUTDST##oplen  (RecompPos, opcode); \
	PUTDST8(RecompPos, 0x40 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
	PUTDST32(RecompPos,imm8);

/*scale is: 0=1,0x40=2,0x80=4,0xc0=8 ??*/

#define OPCODE_REG_INDEX_SCALE(oplen,opcode,reg,base,index,scale) \
    PUTDST8(RecompPos, 0x40 | (((base)&0x20)>>5) | (((reg)&0x10)>>1) | (((reg)&0x20)>>3)| (((index)&0x20)>>4)); \
	PUTDST##oplen  (RecompPos, opcode); \
	PUTDST8(RecompPos, 0x04 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RecompPos, (((scale)-1)*0x40) | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) );

#define OPCODE_REG_BASE_INDEX_SCALE(oplen,opcode,reg,base,index,scale) \
    PUTDST8(RecompPos, 0x40 | (((base)&0x20)>>5) | (((reg)&0x10)>>1) | (((reg)&0x20)>>3)| (((index)&0x20)>>4)); \
	PUTDST##oplen  (RecompPos, opcode); \
	PUTDST8(RecompPos, 0x04 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RecompPos, (scale) | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) );

#define OPCODE_REG_BASE_INDEX_SCALE_IMM32(oplen,opcode,reg,base,index,scale,imm32) \
    PUTDST8(RecompPos, 0x40 | (((base)&0x20)>>5) | (((reg)&0x10)>>1) | (((reg)&0x20)>>3)| (((index)&0x20)>>4)); \
	PUTDST##oplen  (RecompPos, opcode); \
	PUTDST8(RecompPos, 0x84 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RecompPos, (scale) | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	PUTDST32(RecompPos,imm32);

#define OPCODE_REG_BASE_INDEX_SCALE_IMM8(oplen,opcode,reg,base,index,scale,imm32) \
    PUTDST8(RecompPos, 0x40 | (((base)&0x20)>>5) | (((reg)&0x10)>>1) | (((reg)&0x20)>>3)| (((index)&0x20)>>4)); \
	PUTDST##oplen  (RecompPos, opcode); \
	PUTDST8(RecompPos, 0x44 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RecompPos, (scale) | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	PUTDST32(RecompPos,imm32);


#define OPCODE_REG_BASE_INDEX_IMM8(oplen,opcode,reg,base,index,imm8) \
    PUTDST8(RecompPos, 0x40 | (((base)&0x20)>>5) | (((reg)&0x10)>>1) | (((reg)&0x20)>>3)| (((index)&0x20)>>4)); \
	PUTDST##oplen  (RecompPos, opcode); \
	PUTDST8(RecompPos, 0x44 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RecompPos, (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	PUTDST8(RecompPos, imm8);

#define OPCODE_REG_BASE_INDEX_IMM32(oplen,opcode,reg,base,index,imm32) \
    PUTDST8(RecompPos, 0x40 | (((base)&0x20)>>5) | (((reg)&0x10)>>1) | (((reg)&0x20)>>3)| (((index)&0x20)>>4)); \
	PUTDST##oplen  (RecompPos, opcode); \
	PUTDST8(RecompPos, 0x44 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RecompPos, (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	PUTDST8(RecompPos, imm32);

#else /* !USEX64 */

#define x64_Reg		0
#define X64_Reg		0
#define X64_Ext		0


extern uint8_t Index[9];

enum x86RegValues {

    x64_Any = 0, x86_Any	= 0,
    x86_EAX,x86_ECX,x86_EDX,x86_EBX,x86_ESP,x86_EBP,x86_ESI,x86_EDI,x86_Any8Bit=0x40,

};

enum x86FpuValues {
	x86_ST0,x86_ST1,x86_ST2,x86_ST3,x86_ST4,x86_ST5,x86_ST6,x86_ST7
};

#define OP_D0	1
#define OP_D1	2
#define OP_D2	3
#define OP_D3	4
#define OP_D4	5
#define OP_D5	6
#define OP_D6	7
#define OP_D7	8

// 	// 43 0F B6 0C 0D 40 83 C7 04 movzx       ecx,byte ptr [r9+4C78340h]

#define OPCODE_REG_DISP(oplen,opcode,reg,disp32) \
	PUTDST##oplen  (RecompPos, opcode); \
	PUTDST8(RecompPos, 0x5 | ((((reg)-1)&0x7) << 3)); \
	PUTDST32(RecompPos,disp32);

#define OPCODE_REG_REG(oplen,opcode,reg,rm) \
	PUTDST##oplen  (RecompPos, opcode); \
	PUTDST8(RecompPos, 0xC0 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3));

#define OPCODE_REG_MREG(oplen,opcode,reg,rm) \
	PUTDST##oplen  (RecompPos, opcode); \
	if(((rm)&0xf)==0x6) { \
		PUTDST8(RecompPos, 0x40 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
		PUTDST8(RecompPos, 0); \
	} else if(((rm)&0xf)==0x5) { \
		PUTDST8(RecompPos, 0x0 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
		PUTDST8(RecompPos, 0x24); \
	} else { \
		PUTDST8(RecompPos, 0x00 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
	}

#define OPCODE_REG_MREG_IMM32(oplen,opcode,reg,rm,imm32) \
	PUTDST##oplen  (RecompPos, opcode); \
	if(((rm)&0xf)==0x5) { \
		PUTDST8(RecompPos, 0x80 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
		PUTDST8(RecompPos, 0x24); \
	} else { \
		PUTDST8(RecompPos, 0x80 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
	} \
	PUTDST32(RecompPos,imm32);

#define OPCODE_REG_BASE_INDEX(oplen,opcode,reg,base,index) \
	PUTDST##oplen  (RecompPos, opcode); \
	if(((base)&0xf)!=0x6) { \
	PUTDST8(RecompPos, 0x04 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RecompPos, 0x00 | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	} else {\
	PUTDST8(RecompPos, 0x44 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RecompPos, 0x00 | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	PUTDST8(RecompPos, 0); \
	}

#define OPCODE_REG_MREG_IMM8(oplen,opcode,reg,rm,imm8) \
	PUTDST##oplen  (RecompPos, opcode); \
	PUTDST8(RecompPos, 0x40 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
	PUTDST32(RecompPos,imm8);

/*scale is: 0=1,0x40=2,0x80=4,0xc0=8 ??*/

#define OPCODE_REG_INDEX_SCALE(oplen,opcode,reg,base,index,scale) \
	PUTDST##oplen  (RecompPos, opcode); \
	PUTDST8(RecompPos, 0x04 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RecompPos, (((scale)-1)*0x40) | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) );

#define OPCODE_REG_BASE_INDEX_SCALE(oplen,opcode,reg,base,index,scale) \
	PUTDST##oplen  (RecompPos, opcode); \
	PUTDST8(RecompPos, 0x04 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RecompPos, (scale) | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) );

#define OPCODE_REG_BASE_INDEX_SCALE_IMM32(oplen,opcode,reg,base,index,scale,imm32) \
	PUTDST##oplen  (RecompPos, opcode); \
	PUTDST8(RecompPos, 0x84 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RecompPos, (scale) | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	PUTDST32(RecompPos,imm32);

#define OPCODE_REG_BASE_INDEX_SCALE_IMM8(oplen,opcode,reg,base,index,scale,imm32) \
	PUTDST##oplen  (RecompPos, opcode); \
	PUTDST8(RecompPos, 0x44 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RecompPos, (scale) | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	PUTDST32(RecompPos,imm32);


#define OPCODE_REG_BASE_INDEX_IMM8(oplen,opcode,reg,base,index,imm8) \
	PUTDST##oplen  (RecompPos, opcode); \
	PUTDST8(RecompPos, 0x44 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RecompPos, (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	PUTDST8(RecompPos, imm8);

#define OPCODE_REG_BASE_INDEX_IMM32(oplen,opcode,reg,base,index,imm32) \
	PUTDST##oplen  (RecompPos, opcode); \
	PUTDST8(RecompPos, 0x44 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RecompPos, (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	PUTDST8(RecompPos, imm32);

#define OPCODE_REG_INDEX_SCALE_IMM32(oplen,opcode,reg,index,scale,imm32) \
	PUTDST##oplen  (RecompPos, opcode); \
	PUTDST8(RecompPos, 0x4 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RecompPos, 0x5 | (scale) | ((((index)-1)&0x7) << 3) ); \
	PUTDST32(RecompPos,imm32);

#endif

#ifndef USEX64

#define OPCODE_REG_VARIABLE(PREFIX,oplen,opcode,reg,variable) \
	PREFIX \
	OPCODE_REG_DISP(oplen,opcode,reg,variable);
#else

#define OPCODE_REG_VARIABLE(PREFIX,oplen,opcode,reg,Variable) \
	if(((uintptr_t)(Variable) - (uintptr_t)TLB_Map) < 0x7FFFFFFF) { \
		PREFIX \
		OPCODE_REG_MREG_IMM32(oplen,opcode,reg,x86_R15,(uintptr_t)(Variable) - (uintptr_t)TLB_Map); \
    } else { \
    	LOAD_VARIABLE(x86_TEMP, (Variable)); \
    	PREFIX \
    	OPCODE_REG_MREG(oplen,opcode,reg,x86_TEMP); \
    }
#endif

#ifdef USEX64
	#define LOAD_FROM_TLB(dstreg,srcreg) MoveX86RegDispToX86Reg(dstreg, x86_R15, srcreg, 8);
#else
	#define LOAD_FROM_TLB(dstreg,srcreg) MoveVariableDispToX86Reg(TLB_Map,dstreg,srcreg,4);
#endif


void MoveX86RegDispToX86Reg(int32_t x86Reg, int32_t AddrReg, int32_t IndexReg, int32_t Multiplier);
void MoveVariableToX86reg64(void *Variable, int32_t x86reg);
void MovePointerToX86reg(void *Variable, int32_t x86reg);
void AddConstToX86Reg64 (int32_t x86Reg, uint32_t Const);
void MoveConstQwordToX86reg(uint64_t Const, int32_t x86reg);

void AdcX86regToVariable             ( int32_t x86reg, void * Variable );
void AdcConstToVariable              ( void *Variable, uint8_t Constant );
void AdcConstToX86Reg                ( int32_t x86Reg, uint32_t Const );
void AdcVariableToX86reg             ( int32_t x86reg, void * Variable );
void AdcX86RegToX86Reg               ( int32_t Destination, int32_t Source );
void AddConstToVariable              ( uint32_t Const, void *Variable );
void AddConstToX86Reg                ( int32_t x86Reg, uint32_t Const );
void AddVariableToX86reg             ( int32_t x86reg, void * Variable );
void AddX86regToVariable             ( int32_t x86reg, void * Variable );
void AddX86RegToX86Reg               ( int32_t Destination, int32_t Source );
void AndConstToVariable              ( uint32_t Const, void *Variable );
void AndConstToX86Reg                ( int32_t x86Reg, uint32_t Const );
void AndVariableToX86Reg             ( void * Variable, int32_t x86Reg );
void AndVariableDispToX86Reg         ( void * Variable, int32_t x86Reg, int32_t AddrReg, int32_t Multiplier);
void AndX86RegToX86Reg               ( int32_t Destination, int32_t Source );
void BreakPoint                      ( void );
void Call_Direct                     ( void * FunctAddress);
void Call_Indirect                   ( void * FunctAddress);
void CompConstToVariable             ( uint32_t Const, void * Variable );
void CompConstToX86reg               ( int32_t x86Reg, uint32_t Const );
void CompX86regToVariable            ( int32_t x86Reg, void * Variable );
void CompVariableToX86reg	         ( int32_t x86Reg, void * Variable );
void CompX86RegToX86Reg              ( int32_t Destination, int32_t Source );
void DecX86reg                       ( int32_t x86Reg );
void DivX86reg                       ( int32_t x86reg );
void idivX86reg                      ( int32_t x86reg );
void imulX86reg                      ( int32_t x86reg );
void IncX86reg                       ( int32_t x86Reg );
void JaeLabel8                       ( uint8_t Value );
void JaeLabel32                      ( uint32_t Value );
void JaLabel8                        ( uint8_t Value );
void JaLabel32                       ( uint32_t Value );
void JbLabel8                        ( uint8_t Value );
void JbLabel32                       ( uint32_t Value );
void JecxzLabel8                     ( uint8_t Value );
void JeLabel8                        ( uint8_t Value );
void JeLabel32                       ( uint32_t Value );
void JgeLabel32                      ( uint32_t Value );
void JgLabel8                        ( uint8_t Value );
void JgLabel32                       ( uint32_t Value );
void JleLabel8                       ( uint8_t Value );
void JleLabel32                      ( uint32_t Value );
void JlLabel8                        ( uint8_t Value );
void JlLabel32                       ( uint32_t Value );
void JzLabel8						 ( uint8_t Value );
void JnzLabel8						 ( uint8_t Value );
void JmpDirectReg                    ( int32_t x86reg );
void JmpIndirectLabel32              ( uint32_t location );
void JmpIndirectReg                  ( int32_t x86reg );
void JmpLabel8                       ( uint8_t Value );
void JmpLabel32                      ( uint32_t Value );
void JneLabel8                       ( uint8_t Value );
void JneLabel32                      ( uint32_t Value );
void JnsLabel8                       ( uint8_t Value );
void JnsLabel32                      ( uint32_t Value );
void JsLabel32                       ( uint32_t Value );
void LeaRegReg                       ( int32_t x86RegDest, int32_t x86RegSrc, int32_t multiplier );
void LeaSourceAndOffset              ( int32_t x86DestReg, int32_t x86SourceReg, int32_t offset );
void MoveConstByteToN64Mem           ( uint8_t Const, int32_t AddrReg );
void MoveConstHalfToN64Mem           ( uint16_t Const, int32_t AddrReg );
void MoveConstByteToVariable         ( uint8_t Const,void *Variable );
void MoveConstByteToX86regPointer    ( uint8_t Const, int32_t AddrReg1, int32_t AddrReg2 );
void MoveConstHalfToVariable         ( uint16_t Const, void *Variable );
void MoveConstHalfToX86regPointer    ( uint16_t Const, int32_t AddrReg1, int32_t AddrReg2 );
void MoveConstToMemoryDisp           ( uint32_t Const, int32_t AddrReg, uint32_t Disp );
void MoveConstToN64Mem               ( uint32_t Const, int32_t AddrReg );
void MoveConstToN64MemDisp           ( uint32_t Const, int32_t AddrReg, uint8_t Disp );
void MoveConstToVariable             ( uint32_t Const, void *Variable );
void MoveConstToX86Pointer           ( uint32_t Const, int32_t X86Pointer );
void MoveConstToX86reg               ( uint32_t Const, int32_t x86reg );
void MoveConstToX86regPointer        ( uint32_t Const, int32_t AddrReg1, int32_t AddrReg2 );
void MoveN64MemDispToX86reg          ( int32_t x86reg, int32_t AddrReg, uint8_t Disp );
void MoveN64MemToX86reg              ( int32_t x86reg, int32_t AddrReg );
void MoveN64MemToX86regByte          ( int32_t x86reg, int32_t AddrReg );
void MoveN64MemToX86regHalf          ( int32_t x86reg, int32_t AddrReg );
void MoveSxByteX86regPointerToX86reg ( int32_t AddrReg1, int32_t AddrReg2, int32_t x86reg );
void MoveSxHalfX86regPointerToX86reg ( int32_t AddrReg1, int32_t AddrReg2, int32_t x86reg );
void MoveSxN64MemToX86regByte        ( int32_t x86reg, int32_t AddrReg );
void MoveSxN64MemToX86regHalf        ( int32_t x86reg, int32_t AddrReg );
void MoveSxVariableToX86regByte      ( void *Variable, int32_t x86reg );
void MoveSxVariableToX86regHalf      ( void *Variable, int32_t x86reg );
void MoveVariableDispToX86Reg        ( void *Variable, int32_t x86Reg, int32_t AddrReg, int32_t Multiplier );
void MoveVariableToX86reg            ( void *Variable, int32_t x86reg );
void MoveVariableToX86regByte        ( void *Variable, int32_t x86reg );
void MoveVariableToX86regHalf        ( void *Variable, int32_t x86reg );
void MoveX86PointerToX86reg          ( int32_t x86reg, int32_t X86Pointer );
void MoveX86regByteToN64Mem          ( int32_t x86reg, int32_t AddrReg );
void MoveX86regByteToVariable        ( int32_t x86reg, void * Variable );
void MoveX86regByteToX86regPointer   ( int32_t x86reg, int32_t AddrReg1, int32_t AddrReg2 );
void MoveX86regHalfToN64Mem          ( int32_t x86reg, int32_t AddrReg );
void MoveX86regHalfToVariable        ( int32_t x86reg, void * Variable );
void MoveX86regHalfToX86regPointer   ( int32_t x86reg, int32_t AddrReg1, int32_t AddrReg2 );
void MoveX86regPointerToX86reg       ( int32_t AddrReg1, int32_t AddrReg2, int32_t x86reg );
void MoveX86regPointerToX86regDisp8  ( int32_t AddrReg1, int32_t AddrReg2, int32_t x86reg, uint8_t offset );
void MoveX86regToMemory              ( int32_t x86reg, int32_t AddrReg, uint32_t Disp );
void MoveX86regToN64Mem              ( int32_t x86reg, int32_t AddrReg );
void MoveX86regToN64MemDisp          ( int32_t x86reg, int32_t AddrReg, uint8_t Disp );
void MoveX86regToVariable            ( int32_t x86reg, void * Variable );
void MoveX86RegToX86Reg              ( int32_t Source, int32_t Destination );
void MoveX86regToX86Pointer          ( int32_t x86reg, int32_t X86Pointer );
void MoveX86regToX86regPointer       ( int32_t x86reg, int32_t AddrReg1, int32_t AddrReg2 );
void MoveZxByteX86regPointerToX86reg ( int32_t AddrReg1, int32_t AddrReg2, int32_t x86reg );
void MoveZxHalfX86regPointerToX86reg ( int32_t AddrReg1, int32_t AddrReg2, int32_t x86reg );
void MoveZxN64MemToX86regByte        ( int32_t x86reg, int32_t AddrReg );
void MoveZxN64MemToX86regHalf        ( int32_t x86reg, int32_t AddrReg );
void MoveZxVariableToX86regByte      ( void *Variable, int32_t x86reg );
void MoveZxVariableToX86regHalf      ( void *Variable, int32_t x86reg );
void MulX86reg                       ( int32_t x86reg );
void NotX86Reg                       ( int32_t x86Reg );
void OrConstToVariable               ( uint32_t Const, void * Variable );
void OrConstToX86Reg                 ( uint32_t Const, int32_t  x86Reg );
void OrVariableToX86Reg              ( void * Variable, int32_t x86Reg );
void OrX86RegToVariable              ( void * Variable, int32_t x86Reg );
void OrX86RegToX86Reg                ( int32_t Destination, int32_t Source );
void Popad                           ( void );
void Pushad                          ( void );
void Push					         ( int32_t x86reg );
void Pop					         ( int32_t x86reg );
void PushImm32                       ( uint32_t Value );
void Ret                             ( void );
void Seta                            ( int32_t x86reg );
void Setae                           ( int32_t x86reg );
void SetaVariable                    ( void * Variable );
void Setb                            ( int32_t x86reg );
void SetbVariable                    ( void * Variable );
void Setg                            ( int32_t x86reg );
void SetgVariable                    ( void * Variable );
void Setl                            ( int32_t x86reg );
void SetlVariable                    ( void * Variable );
void Setz					         ( int32_t x86reg );
void Setnz					         ( int32_t x86reg );
void ShiftLeftDouble                 ( int32_t Destination, int32_t Source );
void ShiftLeftDoubleImmed            ( int32_t Destination, int32_t Source, uint8_t Immediate );
void ShiftLeftSign                   ( int32_t x86reg );
void ShiftLeftSignImmed              ( int32_t x86reg, uint8_t Immediate );
void ShiftRightDouble                ( int32_t Destination, int32_t Source );
void ShiftRightDoubleImmed           ( int32_t Destination, int32_t Source, uint8_t Immediate );
void ShiftRightSign                  ( int32_t x86reg );
void ShiftRightSignImmed             ( int32_t x86reg, uint8_t Immediate );
void ShiftRightUnsign                ( int32_t x86reg );
void ShiftRightUnsignImmed           ( int32_t x86reg, uint8_t Immediate );
void SbbConstFromX86Reg              ( int32_t x86Reg, uint32_t Const );
void SbbVariableFromX86reg           ( int32_t x86reg, void * Variable );
void SbbX86RegToX86Reg               ( int32_t Destination, int32_t Source );
void SubConstFromVariable            ( uint32_t Const, void *Variable );
void SubConstFromX86Reg              ( int32_t x86Reg, uint32_t Const );
void SubVariableFromX86reg           ( int32_t x86reg, void * Variable );
void SubX86RegToX86Reg               ( int32_t Destination, int32_t Source );
void TestConstToX86Reg               ( uint32_t Const, int32_t x86reg );
void TestVariable                    ( uint32_t Const, void * Variable );
void TestX86RegToX86Reg              ( int32_t Destination, int32_t Source );
void TestVariableToX86Reg			 ( uint32_t x86reg, void * Variable);
void XorConstToX86Reg                ( int32_t x86Reg, uint32_t Const );
void XorX86RegToX86Reg               ( int32_t Source, int32_t Destination );
void XorVariableToX86reg             ( void *Variable, int32_t x86reg );


void fpuAbs					         ( void );
void fpuAddDword			         ( void *Variable );
void fpuAddDwordRegPointer           ( int32_t x86Pointer );
void fpuAddQword			         ( void *Variable );
void fpuAddQwordRegPointer           ( int32_t x86Pointer );
void fpuAddReg				         ( int32_t x86reg );
void fpuAddRegPop			         ( int32_t * StackPos, int32_t x86reg );
void fpuComDword			         ( void *Variable, uint32_t Pop );
void fpuComDwordRegPointer           ( int32_t x86Pointer, uint32_t Pop );
void fpuComQword			         ( void *Variable, uint32_t Pop );
void fpuComQwordRegPointer           ( int32_t x86Pointer, uint32_t Pop );
void fpuComReg                       ( int32_t x86reg, uint32_t Pop );
void fpuDivDword			         ( void *Variable );
void fpuDivDwordRegPointer           ( int32_t x86Pointer );
void fpuDivQword			         ( void *Variable );
void fpuDivQwordRegPointer           ( int32_t x86Pointer );
void fpuDivReg                       ( int32_t Reg );
void fpuDivRegPop			         ( int32_t x86reg );
void fpuExchange                     ( int32_t Reg );
void fpuFree                         ( int32_t Reg );
void fpuDecStack                     ( int32_t * StackPos );
void fpuIncStack                     ( int32_t * StackPos );
void fpuLoadControl			         ( void *Variable );
void fpuLoadDword			         ( int32_t * StackPos, void *Variable );
void fpuLoadDwordFromX86Reg          ( int32_t * StackPos, int32_t x86reg );
void fpuLoadDwordFromN64Mem          ( int32_t * StackPos, int32_t x86reg );
void fpuLoadInt32bFromN64Mem         ( int32_t * StackPos, int32_t x86reg );
void fpuLoadIntegerDword	         ( int32_t * StackPos, void *Variable );
void fpuLoadIntegerDwordFromX86Reg   ( int32_t * StackPos,int32_t x86Reg );
void fpuLoadIntegerQword	         ( int32_t * StackPos, void *Variable );
void fpuLoadIntegerQwordFromX86Reg   ( int32_t * StackPos,int32_t x86Reg );
void fpuLoadQword			         ( int32_t * StackPos, void *Variable );
void fpuLoadQwordFromX86Reg          ( int32_t * StackPos, int32_t x86Reg );
void fpuLoadQwordFromN64Mem          ( int32_t * StackPos, int32_t x86reg );
void fpuLoadReg                      ( int32_t * StackPos, int32_t Reg );
void fpuMulDword                     ( void *Variable);
void fpuMulDwordRegPointer           ( int32_t x86Pointer );
void fpuMulQword                     ( void *Variable);
void fpuMulQwordRegPointer           ( int32_t x86Pointer );
void fpuMulReg                       ( int32_t x86reg );
void fpuMulRegPop                    ( int32_t x86reg );
void fpuNeg					         ( void );
void fpuRound				         ( void );
void fpuSqrt				         ( void );
void fpuStoreControl		         ( void *Variable );
void fpuStoreDword			         ( int32_t * StackPos, void *Variable, uint32_t pop );
void fpuStoreDwordFromX86Reg         ( int32_t * StackPos,int32_t x86Reg, uint32_t pop );
void fpuStoreDwordToN64Mem	         ( int32_t * StackPos, int32_t x86reg, uint32_t Pop );
void fpuStoreIntegerDword            ( int32_t * StackPos, void *Variable, uint32_t pop );
void fpuStoreIntegerDwordFromX86Reg  ( int32_t * StackPos,int32_t x86Reg, uint32_t pop );
void fpuStoreIntegerQword            ( int32_t * StackPos, void *Variable, uint32_t pop );
void fpuStoreIntegerQwordFromX86Reg  ( int32_t * StackPos, int32_t x86Reg, uint32_t pop );
void fpuStoreQword			         ( int32_t * StackPos, void *Variable, uint32_t pop );
void fpuStoreQwordFromX86Reg         ( int32_t * StackPos, int32_t x86Reg, uint32_t pop );
void fpuStoreStatus			         ( void );
void fpuSubDword			         ( void *Variable );
void fpuSubDwordRegPointer           ( int32_t x86Pointer );
void fpuSubDwordReverse              ( void *Variable );
void fpuSubQword			         ( void *Variable );
void fpuSubQwordRegPointer           ( int32_t x86Pointer );
void fpuSubQwordReverse              ( void *Variable );
void fpuSubReg				         ( int32_t x86reg );
void fpuSubRegPop			         ( int32_t x86reg );

void MoveVariable64ToX86reg(void *Variable, int32_t x86reg);
