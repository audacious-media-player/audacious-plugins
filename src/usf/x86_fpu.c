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

void fpuAbs(void) {
	PUTDST16(RecompPos,0xE1D9);
}

void fpuAddDword(void *Variable) {
	BreakPoint();
	PUTDST16(RecompPos,0x05D8);
	PUTDST32(RecompPos,Variable);
}

void fpuAddDwordRegPointer(int x86Pointer) {
	OPCODE_REG_MREG(8,0xD8,OP_D0,x86Pointer);
}

void fpuAddQword(void *Variable) {
	BreakPoint();
	PUTDST16(RecompPos,0x05DC);
	PUTDST32(RecompPos,Variable);
}

void fpuAddQwordRegPointer(int x86Pointer) {
	OPCODE_REG_MREG(8,0xDC,OP_D0,x86Pointer);
}

void fpuAddReg(int x86reg) {
	switch (x86reg&0xf) {
	case x86_ST0: PUTDST16(RecompPos,0xC0D8); break;
	case x86_ST1: PUTDST16(RecompPos,0xC1D8); break;
	case x86_ST2: PUTDST16(RecompPos,0xC2D8); break;
	case x86_ST3: PUTDST16(RecompPos,0xC3D8); break;
	case x86_ST4: PUTDST16(RecompPos,0xC4D8); break;
	case x86_ST5: PUTDST16(RecompPos,0xC5D8); break;
	case x86_ST6: PUTDST16(RecompPos,0xC6D8); break;
	case x86_ST7: PUTDST16(RecompPos,0xC7D8); break;
	}
}

void fpuAddRegPop(int * StackPos, int x86reg) {
	*StackPos = (*StackPos + 1) & 7;
	switch (x86reg&0xf) {
	case x86_ST0: PUTDST16(RecompPos,0xC0DE); break;
	case x86_ST1: PUTDST16(RecompPos,0xC1DE); break;
	case x86_ST2: PUTDST16(RecompPos,0xC2DE); break;
	case x86_ST3: PUTDST16(RecompPos,0xC3DE); break;
	case x86_ST4: PUTDST16(RecompPos,0xC4DE); break;
	case x86_ST5: PUTDST16(RecompPos,0xC5DE); break;
	case x86_ST6: PUTDST16(RecompPos,0xC6DE); break;
	case x86_ST7: PUTDST16(RecompPos,0xC7DE); break;
	}
}

void fpuComDword(void *Variable, uint32_t Pop) {
	BreakPoint();
	PUTDST16(RecompPos, (Pop == 1) ? 0x1DD8 : 0x15D8);
	PUTDST32(RecompPos,Variable);
}

void fpuComDwordRegPointer(int x86Pointer, uint32_t Pop) {
	if(Pop) {
		OPCODE_REG_MREG(8,0xD8,OP_D3,x86Pointer);
	} else {
		OPCODE_REG_MREG(8,0xD8,OP_D2,x86Pointer);
	}
}

void fpuComQword(void *Variable, uint32_t Pop) {
	BreakPoint();
	PUTDST16(RecompPos, (Pop == 1) ? 0x1DDC : 0x15DC);
	PUTDST32(RecompPos,Variable);
}

void fpuComQwordRegPointer(int x86Pointer, uint32_t Pop) {
    if(Pop) {
		OPCODE_REG_MREG(8,0xDC,OP_D3,x86Pointer);
	} else {
		OPCODE_REG_MREG(8,0xDC,OP_D2,x86Pointer);
	}
}

void fpuComReg(int x86reg, uint32_t Pop) {
	int s = (Pop == 1) ? 0x0800 : 0x0000;

	switch (x86reg&0xf) {
	case x86_ST0: PUTDST16(RecompPos,0xD0D8|s); break;
	case x86_ST1: PUTDST16(RecompPos,0xD1D8|s); break;
	case x86_ST2: PUTDST16(RecompPos,0xD2D8|s); break;
	case x86_ST3: PUTDST16(RecompPos,0xD3D8|s); break;
	case x86_ST4: PUTDST16(RecompPos,0xD4D8|s); break;
	case x86_ST5: PUTDST16(RecompPos,0xD5D8|s); break;
	case x86_ST6: PUTDST16(RecompPos,0xD6D8|s); break;
	case x86_ST7: PUTDST16(RecompPos,0xD7D8|s); break;
	}
}

void fpuDivDword(void *Variable) {
	BreakPoint();
	PUTDST16(RecompPos,0x35D8);
	PUTDST32(RecompPos,Variable);
}

void fpuDivDwordRegPointer(int x86Pointer) {
	OPCODE_REG_MREG(8,0xD8,OP_D6,x86Pointer);
}

void fpuDivQword(void *Variable) {
	BreakPoint();
	PUTDST16(RecompPos,0x35DC);
	PUTDST32(RecompPos,Variable);
}

void fpuDivQwordRegPointer(int x86Pointer) {
	OPCODE_REG_MREG(8,0xDC,OP_D6,x86Pointer);
}

void fpuDivReg(int Reg) {
	switch (Reg) {
	case x86_ST0: PUTDST16(RecompPos,0xF0D8); break;
	case x86_ST1: PUTDST16(RecompPos,0xF1D8); break;
	case x86_ST2: PUTDST16(RecompPos,0xF2D8); break;
	case x86_ST3: PUTDST16(RecompPos,0xF3D8); break;
	case x86_ST4: PUTDST16(RecompPos,0xF4D8); break;
	case x86_ST5: PUTDST16(RecompPos,0xF5D8); break;
	case x86_ST6: PUTDST16(RecompPos,0xF6D8); break;
	case x86_ST7: PUTDST16(RecompPos,0xF7D8); break;
	}
}

void fpuDivRegPop(int x86reg) {
	switch (x86reg&0xf) {
	case x86_ST0: PUTDST16(RecompPos,0xF8DE); break;
	case x86_ST1: PUTDST16(RecompPos,0xF9DE); break;
	case x86_ST2: PUTDST16(RecompPos,0xFADE); break;
	case x86_ST3: PUTDST16(RecompPos,0xFBDE); break;
	case x86_ST4: PUTDST16(RecompPos,0xFCDE); break;
	case x86_ST5: PUTDST16(RecompPos,0xFDDE); break;
	case x86_ST6: PUTDST16(RecompPos,0xFEDE); break;
	case x86_ST7: PUTDST16(RecompPos,0xFFDE); break;
	}
}

void fpuExchange(int Reg) {
	switch (Reg) {
	case x86_ST0: PUTDST16(RecompPos,0xC8D9); break;
	case x86_ST1: PUTDST16(RecompPos,0xC9D9); break;
	case x86_ST2: PUTDST16(RecompPos,0xCAD9); break;
	case x86_ST3: PUTDST16(RecompPos,0xCBD9); break;
	case x86_ST4: PUTDST16(RecompPos,0xCCD9); break;
	case x86_ST5: PUTDST16(RecompPos,0xCDD9); break;
	case x86_ST6: PUTDST16(RecompPos,0xCED9); break;
	case x86_ST7: PUTDST16(RecompPos,0xCFD9); break;
	}
}

void fpuFree(int Reg) {
	switch (Reg) {
	case x86_ST0: PUTDST16(RecompPos,0xC0DD); break;
	case x86_ST1: PUTDST16(RecompPos,0xC1DD); break;
	case x86_ST2: PUTDST16(RecompPos,0xC2DD); break;
	case x86_ST3: PUTDST16(RecompPos,0xC3DD); break;
	case x86_ST4: PUTDST16(RecompPos,0xC4DD); break;
	case x86_ST5: PUTDST16(RecompPos,0xC5DD); break;
	case x86_ST6: PUTDST16(RecompPos,0xC6DD); break;
	case x86_ST7: PUTDST16(RecompPos,0xC7DD); break;
	}
}

void fpuDecStack(int * StackPos) {
	*StackPos = (*StackPos - 1) & 7;
	PUTDST16(RecompPos,0xF6D9);
}

void fpuIncStack(int * StackPos) {
	*StackPos = (*StackPos + 1) & 7;
	PUTDST16(RecompPos,0xF7D9);
}

void fpuLoadControl(void *Variable) {
#ifdef USEX64
	PUTDST16(RecompPos,0xB849);
	PUTDST64(RecompPos,Variable);
	PUTDST8(RecompPos,0x41);
	PUTDST16(RecompPos,0x28D9);
#else
	PUTDST16(RecompPos,0x2DD9);
	PUTDST32(RecompPos,Variable);
#endif
}

void fpuLoadDword(int * StackPos,void *Variable) {
	*StackPos = (*StackPos - 1) & 7;
	BreakPoint();
	PUTDST16(RecompPos,0x05D9);
	PUTDST32(RecompPos,Variable);
}

void fpuLoadDwordFromX86Reg(int * StackPos, int x86reg) {
	*StackPos = (*StackPos - 1) & 7;
	OPCODE_REG_MREG(8,0xD9,OP_D0,x86reg);
}

void fpuLoadIntegerDword(int * StackPos,void *Variable) {
	*StackPos = (*StackPos - 1) & 7;
	BreakPoint();
	PUTDST16(RecompPos,0x05DB);
	PUTDST32(RecompPos,Variable);
}

void fpuLoadIntegerDwordFromX86Reg(int * StackPos,int x86reg) {
	*StackPos = (*StackPos - 1) & 7;
	OPCODE_REG_MREG(8,0xDB,OP_D0,x86reg);
}

void fpuLoadIntegerQword(int * StackPos,void *Variable) {
	*StackPos = (*StackPos - 1) & 7;
	BreakPoint();
	PUTDST16(RecompPos,0x2DDF);
	PUTDST32(RecompPos,Variable);
}

void fpuLoadIntegerQwordFromX86Reg(int * StackPos,int x86reg) {
	*StackPos = (*StackPos - 1) & 7;
	OPCODE_REG_MREG(8,0xDF,OP_D5,x86reg);
}

void fpuLoadQword(int * StackPos,void *Variable) {
	*StackPos = (*StackPos - 1) & 7;
	BreakPoint();
	PUTDST16(RecompPos,0x05DD);
	PUTDST32(RecompPos,Variable);
}

void fpuLoadQwordFromX86Reg(int * StackPos, int x86reg) {
	*StackPos = (*StackPos - 1) & 7;
	OPCODE_REG_MREG(8,0xDD,OP_D0,x86reg);
}
void fpuLoadReg(int * StackPos,int Reg) {
	*StackPos = (*StackPos - 1) & 7;
	switch (Reg) {
	case x86_ST0: PUTDST16(RecompPos,0xC0D9); break;
	case x86_ST1: PUTDST16(RecompPos,0xC1D9); break;
	case x86_ST2: PUTDST16(RecompPos,0xC2D9); break;
	case x86_ST3: PUTDST16(RecompPos,0xC3D9); break;
	case x86_ST4: PUTDST16(RecompPos,0xC4D9); break;
	case x86_ST5: PUTDST16(RecompPos,0xC5D9); break;
	case x86_ST6: PUTDST16(RecompPos,0xC6D9); break;
	case x86_ST7: PUTDST16(RecompPos,0xC7D9); break;
		break;
	}
}

void fpuMulDword(void *Variable) {
	BreakPoint();
	PUTDST16(RecompPos,0x0DD8);
	PUTDST32(RecompPos,Variable);
}

void fpuMulDwordRegPointer(int x86Pointer) {
	OPCODE_REG_MREG(8,0xD8,OP_D1,x86Pointer);
}

void fpuMulQword(void *Variable) {
BreakPoint();
	PUTDST16(RecompPos,0x0DDC);
	PUTDST32(RecompPos,Variable);
}

void fpuMulQwordRegPointer(int x86Pointer) {
	OPCODE_REG_MREG(8,0xDC,OP_D1,x86Pointer);
}

void fpuMulReg(int x86reg) {
	switch (x86reg&0xf) {
	case x86_ST0: PUTDST16(RecompPos,0xC8D8); break;
	case x86_ST1: PUTDST16(RecompPos,0xC9D8); break;
	case x86_ST2: PUTDST16(RecompPos,0xCAD8); break;
	case x86_ST3: PUTDST16(RecompPos,0xCBD8); break;
	case x86_ST4: PUTDST16(RecompPos,0xCCD8); break;
	case x86_ST5: PUTDST16(RecompPos,0xCDD8); break;
	case x86_ST6: PUTDST16(RecompPos,0xCED8); break;
	case x86_ST7: PUTDST16(RecompPos,0xCFD8); break;
	}
}

void fpuMulRegPop(int x86reg) {
	switch (x86reg&0xf) {
	case x86_ST0: PUTDST16(RecompPos,0xC8DE); break;
	case x86_ST1: PUTDST16(RecompPos,0xC9DE); break;
	case x86_ST2: PUTDST16(RecompPos,0xCADE); break;
	case x86_ST3: PUTDST16(RecompPos,0xCBDE); break;
	case x86_ST4: PUTDST16(RecompPos,0xCCDE); break;
	case x86_ST5: PUTDST16(RecompPos,0xCDDE); break;
	case x86_ST6: PUTDST16(RecompPos,0xCEDE); break;
	case x86_ST7: PUTDST16(RecompPos,0xCFDE); break;
	}
}

void fpuNeg(void) {
	PUTDST16(RecompPos,0xE0D9);
}

void fpuRound(void) {
	PUTDST16(RecompPos,0xFCD9);
}

void fpuSqrt(void) {
	PUTDST16(RecompPos,0xFAD9);
}

void fpuStoreControl(void *Variable) {

#ifdef USEX64
	PUTDST16(RecompPos,0xB849);
	PUTDST64(RecompPos,Variable);
	PUTDST8(RecompPos,0x41);
	PUTDST16(RecompPos,0x38D9);
#else
	PUTDST16(RecompPos,0x3DD9);
	PUTDST32(RecompPos,Variable);
#endif
}

void fpuStoreDword(int * StackPos,void *Variable, uint32_t pop) {
	if (pop) { *StackPos = (*StackPos + 1) & 7; }
	BreakPoint();
	PUTDST16(RecompPos,(pop == 0) ? 0x15D9 : 0x1DD9);
	PUTDST32(RecompPos,Variable);
}

void fpuStoreDwordFromX86Reg(int * StackPos,int x86reg, uint32_t pop) {
	if (pop) { *StackPos = (*StackPos + 1) & 7; }

	if(pop) {
		OPCODE_REG_MREG(8,0xD9,OP_D3,x86reg);
	} else {
		OPCODE_REG_MREG(8,0xD9,OP_D2,x86reg);
	}
}


void fpuStoreIntegerDword(int * StackPos,void *Variable, uint32_t pop) {
	if (pop) { *StackPos = (*StackPos + 1) & 7; }
	BreakPoint();
	PUTDST16(RecompPos, (pop == 0) ? 0x15DB : 0x1DDB);
	PUTDST32(RecompPos,Variable);
}

void fpuStoreIntegerDwordFromX86Reg(int * StackPos,int x86reg, uint32_t pop) {
	if (pop) { *StackPos = (*StackPos + 1) & 7; }

	if(pop) {
		OPCODE_REG_MREG(8,0xDB,OP_D3,x86reg);
	} else {
		OPCODE_REG_MREG(8,0xDB,OP_D2,x86reg);
	}
}

void fpuStoreIntegerQword(int * StackPos,void *Variable, uint32_t pop) {
	if (pop) { *StackPos = (*StackPos + 1) & 7; }
	BreakPoint();
	PUTDST16(RecompPos, (pop == 0) ? 0x35DF : 0x3DDF);
	PUTDST32(RecompPos,Variable);
	if (!pop) { BreakPoint(); }
}

void fpuStoreIntegerQwordFromX86Reg(int * StackPos, int x86reg, uint32_t pop) {
	if (pop) { *StackPos = (*StackPos + 1) & 7; }

	BreakPoint();
	if(pop) {
		OPCODE_REG_MREG(8,0xDF,OP_D5,x86reg);
	} else {
		OPCODE_REG_MREG(8,0xDF,OP_D4,x86reg);
	}

}

void fpuStoreQwordFromX86Reg(int * StackPos, int x86reg, uint32_t pop) {
	if (pop) { *StackPos = (*StackPos + 1) & 7; }

	if(pop) {
		OPCODE_REG_MREG(8,0xDD,OP_D3,x86reg);
	} else {
		OPCODE_REG_MREG(8,0xDD,OP_D2,x86reg);
	}

}

void fpuStoreStatus(void) {
	PUTDST16(RecompPos,0xE0DF);
}

void fpuSubDword(void *Variable) {
	BreakPoint();
	PUTDST16(RecompPos,0x25D8);
	PUTDST32(RecompPos,Variable);
}

void fpuSubDwordRegPointer(int x86Pointer) {
    OPCODE_REG_MREG(8,0xD8,OP_D4,x86Pointer);
}

void fpuSubDwordReverse(void *Variable) {
BreakPoint();
	PUTDST16(RecompPos,0x2DD8);
	PUTDST32(RecompPos,Variable);
}

void fpuSubQword(void *Variable) {
BreakPoint();
	PUTDST16(RecompPos,0x25DC);
	PUTDST32(RecompPos,Variable);
}

void fpuSubQwordRegPointer(int x86Pointer) {
    OPCODE_REG_MREG(8,0xDC,OP_D4,x86Pointer);
}

void fpuSubQwordReverse(void *Variable) {
	BreakPoint();
	PUTDST16(RecompPos,0x2DDC);
	PUTDST32(RecompPos,Variable);
}

void fpuSubReg(int x86reg) {
	switch (x86reg&0xf) {
	case x86_ST0: PUTDST16(RecompPos,0xE0D8); break;
	case x86_ST1: PUTDST16(RecompPos,0xE1D8); break;
	case x86_ST2: PUTDST16(RecompPos,0xE2D8); break;
	case x86_ST3: PUTDST16(RecompPos,0xE3D8); break;
	case x86_ST4: PUTDST16(RecompPos,0xE4D8); break;
	case x86_ST5: PUTDST16(RecompPos,0xE5D8); break;
	case x86_ST6: PUTDST16(RecompPos,0xE6D8); break;
	case x86_ST7: PUTDST16(RecompPos,0xE7D8); break;
		break;
	}
}

void fpuSubRegPop(int x86reg) {
	switch (x86reg&0xf) {
	case x86_ST0: PUTDST16(RecompPos,0xE8DE); break;
	case x86_ST1: PUTDST16(RecompPos,0xE9DE); break;
	case x86_ST2: PUTDST16(RecompPos,0xEADE); break;
	case x86_ST3: PUTDST16(RecompPos,0xEBDE); break;
	case x86_ST4: PUTDST16(RecompPos,0xECDE); break;
	case x86_ST5: PUTDST16(RecompPos,0xEDDE); break;
	case x86_ST6: PUTDST16(RecompPos,0xEEDE); break;
	case x86_ST7: PUTDST16(RecompPos,0xEFDE); break;
	}
}
void fpuDivDwordReverse(void *Variable) {
	BreakPoint();
	PUTDST16(RecompPos,0x3DD8);
	PUTDST32(RecompPos,Variable);
}


void fpuDivQwordReverse(void *Variable) {
	BreakPoint();
	PUTDST16(RecompPos,0x3DDC);
	PUTDST32(RecompPos,Variable);
}

unsigned int fpucontrol;

/* returns and pushes current fpu state, bool for set normal */
int fpuSaveControl(uint32_t bSetNormal) {
/*	_asm {
		fnstcw word ptr [fpucontrol]
	}

	if (bSetNormal == 1) {
		unsigned short fpunormal = fpucontrol & 0xF3FF;
		_asm {
			fldcw word ptr [fpunormal]
		}
	}
*/
	return fpucontrol;
}

/* returns and pops fpu state previously pushed */
int fpuRestoreControl() {
/*	_asm {
		fldcw word ptr [fpucontrol]
	}*/
	return fpucontrol;
}

void fpuSetupDouble(void) {
	int temp = 0;
/*
	_asm {
		fnstcw word ptr [temp]
		or [temp], 0x300
		and [temp], 0xFFFFF3FF
		fldcw word ptr [temp]
	}  */
}

