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

/*enum x86RegValues {
	x86_EAX = 0, x86_EBX = 1, x86_ECX = 2, x86_EDX = 3,
	x86_ESI = 4, x86_EDI = 5, x86_EBP = 6, x86_ESP = 7
};*/

#include "types.h"

enum mmxRegValues {
	x86_MM0 = 1, x86_MM1 = 2, x86_MM2 = 3, x86_MM3 = 4,
	x86_MM4 = 5, x86_MM5 = 6, x86_MM6 = 7, x86_MM7 = 8
};

enum sseRegValues {
	x86_XMM0 = 1, x86_XMM1 = 2, x86_XMM2 = 3, x86_XMM3 = 4,
	x86_XMM4 = 5, x86_XMM5 = 6, x86_XMM6 = 7, x86_XMM7 = 8
};

#ifdef USEX64
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
	PUTDST8(RSPRecompPos, 0x48 | (((reg)&0x20)>>5)); \
	PUTDST8(RSPRecompPos,0xB8 | ((reg)-1)&0xf); \
	PUTDST64(RSPRecompPos,variable);

// 	// 43 0F B6 0C 0D 40 83 C7 04 movzx       ecx,byte ptr [r9+4C78340h]

#define OPCODE_REG_REG(oplen,opcode,reg,rm) \
	PUTDST8(RSPRecompPos, 0x40 | (((rm)&0x20)>>5) | ((((rm|reg))&0x10)>>1) | (((reg)&0x20)>>3)); \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	PUTDST8(RSPRecompPos, 0xC0 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3));

#define OPCODE_REG_MREG(oplen,opcode,reg,rm) \
	PUTDST8(RSPRecompPos, 0x40 | (((rm)&0x20)>>5) | (((reg)&0x10)>>1) | (((reg)&0x20)>>3)); \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	if(((rm)&0xf)==0x6) { \
		PUTDST8(RSPRecompPos, 0x40 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
		PUTDST8(RSPRecompPos, 0); \
	} else if(((rm)&0xf)==0x5) { \
		PUTDST8(RSPRecompPos, 0x0 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
		PUTDST8(RSPRecompPos, 0x24); \
	} else { \
		PUTDST8(RSPRecompPos, 0x00 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
	}

#define OPCODE_REG_MREG_IMM32(oplen,opcode,reg,rm,imm32) \
	PUTDST8(RSPRecompPos, 0x40 | (((rm)&0x20)>>5) | (((reg)&0x10)>>1) | (((reg)&0x20)>>3)); \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	if(((rm)&0xf)==0x5) { \
		PUTDST8(RSPRecompPos, 0x80 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
		PUTDST8(RSPRecompPos, 0x24); \
	} else { \
		PUTDST8(RSPRecompPos, 0x80 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
	} \
	PUTDST32(RSPRecompPos,imm32);

#define OPCODE_REG_BASE_INDEX(oplen,opcode,reg,base,index) \
    PUTDST8(RSPRecompPos, 0x40 | (((base)&0x20)>>5) | (((reg)&0x10)>>1) | (((reg)&0x20)>>3)| (((index)&0x20)>>4)); \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	if(((base)&0xf)!=0x6) { \
	PUTDST8(RSPRecompPos, 0x04 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RSPRecompPos, 0x00 | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	} else {\
	PUTDST8(RSPRecompPos, 0x44 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RSPRecompPos, 0x00 | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	PUTDST8(RSPRecompPos, 0); \
	}

#define OPCODE_REG_MREG_IMM8(oplen,opcode,reg,rm,imm8) \
	PUTDST8(RSPRecompPos, 0x40 | (((rm)&0x20)>>5) | (((reg)&0x10)>>1) | (((reg)&0x20)>>3)); \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	PUTDST8(RSPRecompPos, 0x40 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RSPRecompPos,imm8);

/*scale is: 0=1,0x40=2,0x80=4,0xc0=8 ??*/

#define OPCODE_REG_INDEX_SCALE(oplen,opcode,reg,base,index,scale) \
    PUTDST8(RSPRecompPos, 0x40 | (((base)&0x20)>>5) | (((reg)&0x10)>>1) | (((reg)&0x20)>>3)| (((index)&0x20)>>4)); \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	PUTDST8(RSPRecompPos, 0x04 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RSPRecompPos, (((scale)-1)*0x40) | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) );

#define OPCODE_REG_BASE_INDEX_SCALE(oplen,opcode,reg,base,index,scale) \
    PUTDST8(RSPRecompPos, 0x40 | (((base)&0x20)>>5) | (((reg)&0x10)>>1) | (((reg)&0x20)>>3)| (((index)&0x20)>>4)); \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	PUTDST8(RSPRecompPos, 0x04 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RSPRecompPos, (scale) | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) );

#define OPCODE_REG_BASE_INDEX_SCALE_IMM32(oplen,opcode,reg,base,index,scale,imm32) \
    PUTDST8(RSPRecompPos, 0x40 | (((base)&0x20)>>5) | (((reg)&0x10)>>1) | (((reg)&0x20)>>3)| (((index)&0x20)>>4)); \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	PUTDST8(RSPRecompPos, 0x84 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RSPRecompPos, (scale) | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	PUTDST32(RSPRecompPos,imm32);

#define OPCODE_REG_BASE_INDEX_SCALE_IMM8(oplen,opcode,reg,base,index,scale,imm32) \
    PUTDST8(RSPRecompPos, 0x40 | (((base)&0x20)>>5) | (((reg)&0x10)>>1) | (((reg)&0x20)>>3)| (((index)&0x20)>>4)); \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	PUTDST8(RSPRecompPos, 0x44 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RSPRecompPos, (scale) | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	PUTDST32(RSPRecompPos,imm32);



#define OPCODE_REG_BASE_INDEX_IMM8(oplen,opcode,reg,base,index,imm8) \
    PUTDST8(RSPRecompPos, 0x40 | (((base)&0x20)>>5) | (((reg)&0x10)>>1) | (((reg)&0x20)>>3)| (((index)&0x20)>>4)); \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	PUTDST8(RSPRecompPos, 0x44 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RSPRecompPos, (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	PUTDST8(RSPRecompPos, imm8);

#define OPCODE_REG_BASE_INDEX_IMM32(oplen,opcode,reg,base,index,imm32) \
    PUTDST8(RSPRecompPos, 0x40 | (((base)&0x20)>>5) | (((reg)&0x10)>>1) | (((reg)&0x20)>>3)| (((index)&0x20)>>4)); \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	PUTDST8(RSPRecompPos, 0x84 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RSPRecompPos, (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	PUTDST32(RSPRecompPos, imm32);

#else

enum x86RegValues {

    x86_Any	= 0,
    x86_EAX,x86_ECX,x86_EDX,x86_EBX,x86_ESP,x86_EBP,x86_ESI,x86_EDI,x86_Any8Bit=0x40,x64_Any = 0x0,
    x86_RAX = 0x1,x86_RCX,x86_RDX,x86_RBX,x86_RSP,x86_RBP,x86_RSI,x86_RDI,

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
	PUTDST##oplen  (RSPRecompPos, opcode); \
	PUTDST8(RSPRecompPos, 0x5 | ((((reg)-1)&0x7) << 3)); \
	PUTDST32(RSPRecompPos,disp32);


#define OPCODE_REG_REG(oplen,opcode,reg,rm) \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	PUTDST8(RSPRecompPos, 0xC0 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3));

#define OPCODE_REG_MREG(oplen,opcode,reg,rm) \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	if(((rm)&0xf)==0x6) { \
		PUTDST8(RSPRecompPos, 0x40 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
		PUTDST8(RSPRecompPos, 0); \
	} else if(((rm)&0xf)==0x5) { \
		PUTDST8(RSPRecompPos, 0x0 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
		PUTDST8(RSPRecompPos, 0x24); \
	} else { \
		PUTDST8(RSPRecompPos, 0x00 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
	}

#define OPCODE_REG_MREG_IMM32(oplen,opcode,reg,rm,imm32) \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	if(((rm)&0xf)==0x5) { \
		PUTDST8(RSPRecompPos, 0x80 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
		PUTDST8(RSPRecompPos, 0x24); \
	} else { \
		PUTDST8(RSPRecompPos, 0x80 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
	} \
	PUTDST32(RSPRecompPos,imm32);

#define OPCODE_REG_BASE_INDEX(oplen,opcode,reg,base,index) \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	if(((base)&0xf)!=0x6) { \
	PUTDST8(RSPRecompPos, 0x04 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RSPRecompPos, 0x00 | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	} else {\
	PUTDST8(RSPRecompPos, 0x44 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RSPRecompPos, 0x00 | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	PUTDST8(RSPRecompPos, 0); \
	}

#define OPCODE_REG_MREG_IMM8(oplen,opcode,reg,rm,imm8) \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	PUTDST8(RSPRecompPos, 0x40 | (((rm)-1)&0x7) | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RSPRecompPos,imm8);

#define OPCODE_REG_INDEX_SCALE(oplen,opcode,reg,base,index,scale) \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	PUTDST8(RSPRecompPos, 0x04 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RSPRecompPos, (((scale)-1)*0x40) | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) );

#define OPCODE_REG_BASE_INDEX_SCALE(oplen,opcode,reg,base,index,scale) \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	PUTDST8(RSPRecompPos, 0x04 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RSPRecompPos, (scale) | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) );

#define OPCODE_REG_BASE_INDEX_SCALE_IMM32(oplen,opcode,reg,base,index,scale,imm32) \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	PUTDST8(RSPRecompPos, 0x84 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RSPRecompPos, (scale) | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	PUTDST32(RSPRecompPos,imm32);

#define OPCODE_REG_BASE_INDEX_SCALE_IMM8(oplen,opcode,reg,base,index,scale,imm32) \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	PUTDST8(RSPRecompPos, 0x44 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RSPRecompPos, (scale) | (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	PUTDST32(RSPRecompPos,imm32);


#define OPCODE_REG_BASE_INDEX_IMM8(oplen,opcode,reg,base,index,imm8) \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	PUTDST8(RSPRecompPos, 0x44 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RSPRecompPos, (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	PUTDST8(RSPRecompPos, imm8);

#define OPCODE_REG_BASE_INDEX_IMM32(oplen,opcode,reg,base,index,imm32) \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	PUTDST8(RSPRecompPos, 0x84 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RSPRecompPos, (((base)-1)&0x7) | ((((index)-1)&0x7) << 3) ); \
	PUTDST32(RSPRecompPos, imm32);

#define OPCODE_REG_INDEX_SCALE_IMM32(oplen,opcode,reg,index,scale,imm32) \
	PUTDST##oplen  (RSPRecompPos, opcode); \
	PUTDST8(RSPRecompPos, 0x4 | ((((reg)-1)&0x7) << 3)); \
	PUTDST8(RSPRecompPos, 0x5 | (scale) | ((((index)-1)&0x7) << 3) ); \
	PUTDST32(RSPRecompPos,imm32);

#endif


#ifndef USEX64

#define OPCODE_REG_VARIABLE(PREFIX,oplen,opcode,reg,variable) \
	PREFIX \
	OPCODE_REG_DISP(oplen,opcode,reg,variable);

#define OPCODE_REG_ADDR_DMEM(PREFIX,oplen,opcode,reg,addr) \
	PREFIX \
	OPCODE_REG_MREG_IMM32(oplen,opcode,reg,addr,DMEM);

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

#define OPCODE_REG_ADDR_DMEM(PREFIX,oplen,opcode,reg,addr) \
	if(((uintptr_t)(DMEM) - (uintptr_t)TLB_Map) < 0x7FFFFFFF) { \
		PREFIX \
		OPCODE_REG_BASE_INDEX_IMM32(oplen,opcode,reg,x86_R15,addr,(uintptr_t)(DMEM) - (uintptr_t)TLB_Map); \
    } else { \
    	LOAD_VARIABLE(x86_TEMP, (DMEM)); \
    	PREFIX \
    	OPCODE_REG_BASE_INDEX(oplen,opcode,reg,x86_TEMP,addr); \
    }

#endif

#ifdef USEX64
	#define LOAD_FROM_DMEM(dstreg,srcreg) MoveX86RegDispToX86Reg(dstreg, x86_R15, srcreg, 8);
#else
	#define LOAD_FROM_DMEM(dstreg,srcreg) MoveVariableDispToX86Reg(DMEM,dstreg,srcreg,4);
#endif


void RSPAdcX86RegToX86Reg				( int32_t Destination, int32_t Source );
void RSPAdcX86regToVariable			( int32_t x86reg, void * Variable );
void RSPAdcX86regHalfToVariable		( int32_t x86reg, void * Variable );
void RSPAdcConstToX86reg				( uint8_t Constant, int32_t x86reg );
void RSPAdcConstToVariable				( void *Variable, uint8_t Constant );
void RSPAdcConstHalfToVariable			( void *Variable, uint8_t Constant );
void RSPAddConstToVariable				( uint32_t Const, void *Variable );
void RSPAddConstToX86Reg				( int32_t x86Reg, uint32_t Const );
void RSPAddQwordToX86Reg (int32_t x86reg, uint64_t Const);
void RSPAddVariableToX86reg			( int32_t x86reg, void * Variable );
void RSPAddX86regToVariable			( int32_t x86reg, void * Variable );
void RSPAddX86regHalfToVariable		( int32_t x86reg, void * Variable );
void RSPAddX86RegToX86Reg				( int32_t Destination, int32_t Source );
void RSPAndConstToVariable				( uint32_t Const, void *Variable );
void RSPAndConstToX86Reg				( int32_t x86Reg, uint32_t Const );
void RSPAndVariableToX86Reg			( void * Variable, int32_t x86Reg );
void RSPAndVariableToX86regHalf		( void * Variable, int32_t x86Reg );
void RSPAndX86RegToVariable			( void * Variable, int32_t x86Reg );
void RSPAndX86RegToX86Reg				( int32_t Destination, int32_t Source );
void RSPAndX86RegHalfToX86RegHalf		( int32_t Destination, int32_t Source );
void RSPBreakPoint (void);
void RSPCall_Direct					( void * FunctAddress );
void RSPCall_Indirect					( void * FunctAddress );
void RSPCondMoveEqual					( int32_t Destination, int32_t Source );
void RSPCondMoveNotEqual				( int32_t Destination, int32_t Source );
void RSPCondMoveGreater				( int32_t Destination, int32_t Source );
void RSPCondMoveGreaterEqual			( int32_t Destination, int32_t Source );
void RSPCondMoveLess					( int32_t Destination, int32_t Source );
void RSPCondMoveLessEqual				( int32_t Destination, int32_t Source );
void RSPCompConstToVariable			( uint32_t Const, void * Variable );
void RSPCompConstHalfToVariable		( uint16_t Const, void * Variable );
void RSPCompConstToX86reg				( int32_t x86Reg, uint32_t Const );
void RSPCompX86regToVariable			( int32_t x86Reg, void * Variable );
void RSPCompVariableToX86reg			( int32_t x86Reg, void * Variable );
void RSPCompX86RegToX86Reg				( int32_t Destination, int32_t Source );
void RSPMoveConstQwordToX86reg		    ( uintptr_t, int32_t);
void RSPCwd							( void );
void RSPCwde							( void );
void RSPDecX86reg						( int32_t x86Reg );
void RSPDivX86reg						( int32_t x86reg );
void RSPidivX86reg						( int32_t x86reg );
void RSPimulX86reg						( int32_t x86reg );
void RSPImulX86RegToX86Reg				( int32_t Destination, int32_t Source );
void RSPIncX86reg						( int32_t x86Reg );
void RSPJaeLabel32						( uint32_t Value );
void RSPJaLabel8						( uint8_t Value );
void RSPJaLabel32						( uint32_t Value );
void RSPJbLabel8						( uint8_t Value );
void RSPJbLabel32						( uint32_t Value );
void RSPJeLabel8						( uint8_t Value );
void RSPJeLabel32						( uint32_t Value );
void RSPJgeLabel8						( uint8_t Value );
void RSPJgeLabel32						( uint32_t Value );
void RSPJgLabel8						( uint8_t Value );
void RSPJgLabel32						( uint32_t Value );
void RSPJleLabel8						( uint8_t Value );
void RSPJleLabel32						( uint32_t Value );
void RSPJlLabel8						( uint8_t Value );
void RSPJlLabel32						( uint32_t Value );
void RSPJumpX86Reg						( int32_t x86reg );
void RSPJmpLabel8						( uint8_t Value );
void RSPJmpLabel32						( uint32_t Value);
void RSPJneLabel8						( uint8_t Value );
void RSPJneLabel32						( uint32_t Value );
void RSPJnsLabel8						( uint8_t Value );
void RSPJnsLabel32						( uint32_t Value );
void RSPJsLabel32						( uint32_t Value );
void RSPLeaSourceAndOffset				( int32_t x86DestReg, int32_t x86SourceReg, int32_t offset );
void RSPMoveConstByteToN64Mem			( uint8_t Const, int32_t AddrReg );
void RSPMoveConstHalfToN64Mem			( uint16_t Const, int32_t AddrReg );
void RSPMoveConstByteToVariable		( uint8_t Const,void *Variable );
void RSPMoveConstHalfToVariable		( uint16_t Const, void *Variable );
void RSPMoveConstToN64Mem				( uint32_t Const, int32_t AddrReg );
void RSPMoveConstToN64MemDisp			( uint32_t Const, int32_t AddrReg, uint8_t Disp );
void RSPMoveConstToVariable			( uint32_t Const, void *Variable );
void RSPMoveConstToX86reg				( uint32_t Const, int32_t x86reg );
void RSPMoveOffsetToX86reg				( uint32_t Const, int32_t x86reg );
void RSPMoveX86regByteToX86regPointer	( int32_t Source, int32_t AddrReg );
void RSPMoveX86regHalfToX86regPointer	( int32_t Source, int32_t AddrReg );
void RSPMoveX86regHalfToX86regPointerDisp ( int32_t Source, int32_t AddrReg, uint8_t Disp);
void RSPMoveX86regToX86regPointer		( int32_t Source, int32_t AddrReg );
void RSPMoveX86RegToX86regPointerDisp	( int32_t Source, int32_t AddrReg, uint8_t Disp );
void RSPMoveX86regPointerToX86regByte	( int32_t Destination, int32_t AddrReg );
void RSPMoveX86regPointerToX86regHalf	( int32_t Destination, int32_t AddrReg );
void RSPMoveX86regPointerToX86reg		( int32_t Destination, int32_t AddrReg );
void RSPMoveN64MemDispToX86reg			( int32_t x86reg, int32_t AddrReg, uint8_t Disp );
void RSPMoveN64MemToX86reg				( int32_t x86reg, int32_t AddrReg );
void RSPMoveN64MemToX86regByte			( int32_t x86reg, int32_t AddrReg );
void RSPMoveN64MemToX86regHalf			( int32_t x86reg, int32_t AddrReg );
void RSPMoveX86regByteToN64Mem			( int32_t x86reg, int32_t AddrReg );
void RSPMoveX86regByteToVariable		( int32_t x86reg, void * Variable );
void RSPMoveX86regHalfToN64Mem			( int32_t x86reg, int32_t AddrReg );
void RSPMoveX86regHalfToVariable		( int32_t x86reg, void * Variable );
void RSPMoveX86regToN64Mem				( int32_t x86reg, int32_t AddrReg );
void RSPMoveX86regToN64MemDisp			( int32_t x86reg, int32_t AddrReg, uint8_t Disp );
void RSPMoveX86regToVariable			( int32_t x86reg, void * Variable );
void RSPMoveX86RegToX86Reg				( int32_t Source, int32_t Destination );
void RSPMoveVariableToX86reg			( void *Variable, int32_t x86reg );
void RSPMoveVariableToX86regByte		( void *Variable, int32_t x86reg );
void RSPMoveVariableToX86regHalf		( void *Variable, int32_t x86reg );
void RSPMoveSxX86RegHalfToX86Reg		( int32_t Source, int32_t Destination );
void RSPMoveSxX86RegPtrDispToX86RegHalf( int32_t AddrReg, uint8_t Disp, int32_t Destination );
void RSPMoveSxN64MemToX86regByte		( int32_t x86reg, int32_t AddrReg );
void RSPMoveSxN64MemToX86regHalf		( int32_t x86reg, int32_t AddrReg );
void RSPMoveSxVariableToX86regHalf		( void *Variable, int32_t x86reg );
void RSPMoveZxX86RegHalfToX86Reg		( int32_t Source, int32_t Destination );
void RSPMoveZxX86RegPtrDispToX86RegHalf( int32_t AddrReg, uint8_t Disp, int32_t Destination );
void RSPMoveZxN64MemToX86regByte		( int32_t x86reg, int32_t AddrReg );
void RSPMoveZxN64MemToX86regHalf		( int32_t x86reg, int32_t AddrReg );
void RSPMoveZxVariableToX86regHalf		( void *Variable, int32_t x86reg );
void RSPMulX86reg						( int32_t x86reg );
void RSPNegateX86reg					( int32_t x86reg );
void RSPOrConstToVariable				( uint32_t Const, void * Variable );
void RSPOrConstToX86Reg				( uint32_t Const, int32_t  x86Reg );
void RSPOrVariableToX86Reg				( void * Variable, int32_t x86Reg );
void RSPOrVariableToX86regHalf			( void * Variable, int32_t x86Reg );
void RSPOrX86RegToVariable				( void * Variable, int32_t x86Reg );
void RSPOrX86RegToX86Reg				( int32_t Destination, int32_t Source );
void RSPPopad							( void );
void RSPPushad							( void );
void RSPPush							( int32_t x86reg );
void RSPPop							( int32_t x86reg );
void RSPPushImm32						( uint32_t Value );
void RSPRet							( void );
void RSPSeta							( int32_t x86reg );
void RSPSetae							( int32_t x86reg );
void RSPSetl							( int32_t x86reg );
void RSPSetb							( int32_t x86reg );
void RSPSetg							( int32_t x86reg );
void RSPSetz							( int32_t x86reg );
void RSPSetnz							( int32_t x86reg );
void RSPSetlVariable					( void * Variable );
void RSPSetleVariable					( void * Variable );
void RSPSetgVariable					( void * Variable );
void RSPSetgeVariable					( void * Variable );
void RSPSetbVariable					( void * Variable );
void RSPSetaVariable					( void * Variable );
void RSPSetzVariable					( void * Variable );
void RSPSetnzVariable					( void * Variable );
void RSPShiftLeftSign					( int32_t x86reg );
void RSPShiftLeftSignImmed				( int32_t x86reg, uint8_t Immediate );
void RSPShiftLeftSignVariableImmed		( void *Variable, uint8_t Immediate );
void RSPShiftRightSignImmed			( int32_t x86reg, uint8_t Immediate );
void RSPShiftRightSignVariableImmed	( void *Variable, uint8_t Immediate );
void RSPShiftRightUnsign				( int32_t x86reg );
void RSPShiftRightUnsignImmed			( int32_t x86reg, uint8_t Immediate );
void RSPShiftRightUnsignVariableImmed	( void *Variable, uint8_t Immediate );
void RSPShiftLeftDoubleImmed			( int32_t Destination, int32_t Source, uint8_t Immediate );
void RSPShiftRightDoubleImmed			( int32_t Destination, int32_t Source, uint8_t Immediate );
void RSPSubConstFromVariable			( uint32_t Const, void *Variable );
void RSPSubConstFromX86Reg				( int32_t x86Reg, uint32_t Const );
void RSPSubVariableFromX86reg			( int32_t x86reg, void * Variable );
void RSPSubX86RegToX86Reg				( int32_t Destination, int32_t Source );
void RSPSubX86regFromVariable			( int32_t x86reg, void * Variable );
void RSPSbbX86RegToX86Reg				( int32_t Destination, int32_t Source );
void RSPTestConstToVariable			( uint32_t Const, void * Variable );
void RSPTestConstToX86Reg				( uint32_t Const, int32_t x86reg );
void RSPTestX86RegToX86Reg				( int32_t Destination, int32_t Source );
void RSPXorConstToX86Reg				( int32_t x86Reg, uint32_t Const );
void RSPXorX86RegToX86Reg				( int32_t Source, int32_t Destination );
void RSPXorVariableToX86reg			( void *Variable, int32_t x86reg );
void RSPXorX86RegToVariable			( void *Variable, int32_t x86reg );
void RSPXorConstToVariable				( void *Variable, uint32_t Const );

#define _MMX_SHUFFLE(a, b, c, d)	\
	((uint8_t)(((a) << 6) | ((b) << 4) | ((c) << 2) | (d)))

void RSPMmxMoveRegToReg				( int32_t Dest, int32_t Source );
void RSPMmxMoveQwordRegToVariable		( int32_t Dest, void *Variable );
void RSPMmxMoveQwordVariableToReg		( int32_t Dest, void *Variable );
void RSPMmxPandRegToReg				( int32_t Dest, int32_t Source );
void RSPMmxPandnRegToReg				( int32_t Dest, int32_t Source );
void RSPMmxPandVariableToReg			( void * Variable, int32_t Dest );
void RSPMmxPorRegToReg					( int32_t Dest, int32_t Source );
void RSPMmxPorVariableToReg			( void * Variable, int32_t Dest );
void RSPMmxXorRegToReg					( int32_t Dest, int32_t Source );
void RSPMmxShuffleMemoryToReg			( int32_t Dest, void * Variable, uint8_t Immed );
void RSPMmxPmullwRegToReg				( int32_t Dest, int32_t Source );
void RSPMmxPmullwVariableToReg			( int32_t Dest, void * Variable );
void RSPMmxPmulhuwRegToReg				( int32_t Dest, int32_t Source );
void RSPMmxPmulhwRegToReg				( int32_t Dest, int32_t Source );
void RSPMmxPmulhwRegToVariable			( int32_t Dest, void * Variable );
void RSPMmxPsrlwImmed					( int32_t Dest, uint8_t Immed );
void RSPMmxPsrawImmed					( int32_t Dest, uint8_t Immed );
void RSPMmxPsllwImmed					( int32_t Dest, uint8_t Immed );
void RSPMmxPaddswRegToReg				( int32_t Dest, int32_t Source );
void RSPMmxPaddswVariableToReg			( int32_t Dest, void * Variable );
void RSPMmxPaddwRegToReg				( int32_t Dest, int32_t Source );
void RSPMmxPackSignedDwords			( int32_t Dest, int32_t Source );
void RSPMmxUnpackLowWord				( int32_t Dest, int32_t Source );
void RSPMmxUnpackHighWord				( int32_t Dest, int32_t Source );
void RSPMmxCompareGreaterWordRegToReg	( int32_t Dest, int32_t Source );
void RSPMmxEmptyMultimediaState		( void );

void RSPSseMoveAlignedVariableToReg	( void *Variable, int32_t sseReg );
void RSPSseMoveAlignedRegToVariable	( int32_t sseReg, void *Variable );
void RSPSseMoveAlignedN64MemToReg		( int32_t sseReg, int32_t AddrReg );
void RSPSseMoveAlignedRegToN64Mem		( int32_t sseReg, int32_t AddrReg );
void RSPSseMoveUnalignedVariableToReg	( void *Variable, int32_t sseReg );
void RSPSseMoveUnalignedRegToVariable	( int32_t sseReg, void *Variable );
void RSPSseMoveUnalignedN64MemToReg	( int32_t sseReg, int32_t AddrReg );
void RSPSseMoveUnalignedRegToN64Mem	( int32_t sseReg, int32_t AddrReg );
void RSPSseMoveRegToReg				( int32_t Dest, int32_t Source );
void RSPSseXorRegToReg					( int32_t Dest, int32_t Source );

typedef struct {
	union {
		struct {
			unsigned Reg0 : 2;
			unsigned Reg1 : 2;
			unsigned Reg2 : 2;
			unsigned Reg3 : 2;
		};
		unsigned UB:8;
	};
} SHUFFLE;

void RSPSseShuffleReg					( int32_t Dest, int32_t Source, uint8_t Immed );

void RSPx86_SetBranch32b(void * JumpByte, void * Destination);
void RSPx86_SetBranch8b(void * JumpByte, void * Destination);