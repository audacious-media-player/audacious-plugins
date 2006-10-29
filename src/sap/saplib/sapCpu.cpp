#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sapGlobals.h"

void cpuInit( void )
{
	cpuReg_PC = 0xFFFF;
	cpuReg_S = 0xFF;
	cpuReg_A = 0x00;
	cpuReg_X = 0x00;
	cpuReg_Y = 0x00;
	cpuSetFlags( 0x20 );
}


void cpuSetFlags( BYTE flags )
{
	cpuFlag_N = flags;
	cpuFlag_V = (flags>>6)&1;
	cpuFlag_B = (flags>>4)&1;
	cpuFlag_D = (flags>>3)&1;
	cpuFlag_I = (flags>>2)&1;
	cpuFlag_Z = (flags&2)==0 ? 1:0;
	cpuFlag_C = flags&1;
}


BYTE cpuGetFlags( void )
{
BYTE flags;

	flags = (cpuFlag_N&0x80) | ((cpuFlag_V&1)<<6) | 0x20 | ((cpuFlag_B&1)<<4) | ((cpuFlag_D&1)<<3) | ((cpuFlag_I&1)<<2) | (cpuFlag_Z==0 ? 2:0) | (cpuFlag_C&1);
	return flags;
}

#define CPY		{\
					cB ^= 255;\
					int cpuIntValue2;\
					cpuIntValue2 = ((int)cpuReg_Y) + ((int)cB) + 1;\
					cpuFlag_N = cpuFlag_Z = (BYTE)cpuIntValue2; cpuFlag_C = (BYTE)(cpuIntValue2>>8);\
					cpuFlag_V = (( ((cpuReg_Y^cB)^0x80) & (cpuReg_Y^cpuIntValue2) & 0x80))&0x80 ? 1:0;\
				}
#define CMP		{\
					cB ^= 255;\
					int cpuIntValue2;\
					cpuIntValue2 = ((int)cpuReg_A) + ((int)cB) + 1;\
					cpuFlag_N = cpuFlag_Z = (BYTE)cpuIntValue2; cpuFlag_C = (BYTE)(cpuIntValue2>>8);\
					cpuFlag_V = (( ((cpuReg_A^cB)^0x80) & (cpuReg_A^cpuIntValue2) & 0x80))&0x80 ? 1:0;\
				}
#define CPX		{\
					cB ^= 255;\
					int cpuIntValue2;\
					cpuIntValue2 = ((int)cpuReg_X) + ((int)cB) + 1;\
					cpuFlag_N = cpuFlag_Z = (BYTE)cpuIntValue2; cpuFlag_C = (BYTE)(cpuIntValue2>>8);\
					cpuFlag_V = (( ((cpuReg_X^cB)^0x80) & (cpuReg_X^cpuIntValue2) & 0x80))&0x80 ? 1:0;\
				}

#define DECCMP	{\
					cB--;\
					int cpuIntValue2;\
					cpuIntValue2 = ((int)cpuReg_A) + ((int)cB^255) + 1;\
					cpuFlag_N = cpuFlag_Z = (BYTE)cpuIntValue2; cpuFlag_C = (BYTE)(cpuIntValue2>>8);\
					cpuFlag_V = (( ((cpuReg_A^cB^255)^0x80) & (cpuReg_A^cpuIntValue2) & 0x80))&0x80 ? 1:0;\
				}
#define DEC		{ cB--; cpuFlag_N = cpuFlag_Z = cB; }
#define INC		{ cB++; cpuFlag_N = cpuFlag_Z = cB; }
#define	AND		{ cpuReg_A &= cB; cpuFlag_N = cpuFlag_Z = cpuReg_A; }
#define	ASL		{ cpuFlag_C = cB>>7; cB<<=1; cpuFlag_N = cpuFlag_Z = cB; }
#define	EOR		{ cpuReg_A ^= cB; cpuFlag_N = cpuFlag_Z = cpuReg_A; }
#define	ORA		{ cpuReg_A |= cB; cpuFlag_N = cpuFlag_Z = cpuReg_A; }
#define ADC		{\
					if( cpuFlag_D&1 )\
					{\
						BYTE al,ah;\
						al = (cpuReg_A & 0x0f) + (cB & 0x0f) + (cpuFlag_C&1);\
						if (al > 9) al += 6;\
						ah = ((cpuReg_A >> 4)&0x0F) + ((cB >> 4)&0x0F); if (al > 0x0f) ah++;\
						cpuFlag_N = cpuFlag_Z =  cpuReg_A + cB + (cpuFlag_C&1);\
						cpuFlag_V = (((ah << 4) ^ cpuReg_A) & 0x80) && !((cpuReg_A ^ cB) & 0x80) ? 1:0;\
						if (ah > 9) ah += 6; cpuFlag_C = (ah > 0x0f) ? 1:0; cpuReg_A = (ah << 4) | (al & 0x0f);\
					}\
					else\
					{\
						WORD te;\
						te = cpuReg_A + cB + (cpuFlag_C&1);\
						cpuFlag_C = (BYTE)(te>>8);\
						cpuFlag_V = (( ((cpuReg_A^cB)^0x80) & (cpuReg_A^te) & 0x80)) ? 1:0;\
						cpuFlag_N = cpuFlag_Z = cpuReg_A = (BYTE)te;\
					}\
				}
#define SBC		{\
					if( cpuFlag_D&1 )\
					{\
						BYTE al,ah; unsigned int tmp; tmp = (DWORD)cpuReg_A - (DWORD)cB - (WORD)((cpuFlag_C&1)^1);\
						al = (cpuReg_A & 0x0f) - (cB & 0x0f) - ((cpuFlag_C&1)^1);\
						ah = ((cpuReg_A >> 4)&0x0F) - ((cB >> 4)&0x0F); if (al & 0x10) { al -= 6; ah--; } if (ah & 0x10) ah -= 6;\
						cpuFlag_C = (tmp < (unsigned int)0x100) ? 1:0; cpuFlag_N = cpuFlag_Z = (BYTE)tmp;\
						cpuFlag_V = (((cpuReg_A ^ tmp) & 0x80) && ((cpuReg_A ^ cB) & 0x80) ) ? 1:0;\
						cpuReg_A = (ah << 4) | (al & 0x0f);\
					}\
					else\
					{\
						WORD te;\
						te = cpuReg_A + (cB^255) + (cpuFlag_C&1);\
						cpuFlag_C = (BYTE)(te>>8);\
						cpuFlag_V = (( ((cpuReg_A^cB^255)^0x80) & (cpuReg_A^te) & 0x80)) ? 1:0;\
						cpuFlag_N = cpuFlag_Z = cpuReg_A = (BYTE)te;\
					}\
				}
#define INCSBC	{\
					cB++;\
					if( cpuFlag_D&1 )\
					{\
						BYTE al,ah; unsigned int tmp; tmp = (DWORD)cpuReg_A - (DWORD)cB - (WORD)((cpuFlag_C&1)^1);\
						al = (cpuReg_A & 0x0f) - (cB & 0x0f) - ((cpuFlag_C&1)^1);\
						ah = ((cpuReg_A >> 4)&0x0F) - ((cB >> 4)&0x0F); if (al & 0x10) { al -= 6; ah--; } if (ah & 0x10) ah -= 6;\
						cpuFlag_C = (tmp < (unsigned int)0x100) ? 1:0; cpuFlag_N = cpuFlag_Z = (BYTE)tmp;\
						cpuFlag_V = (((cpuReg_A ^ tmp) & 0x80) && ((cpuReg_A ^ cB) & 0x80) ) ? 1:0;\
						cpuReg_A = (ah << 4) | (al & 0x0f);\
					}\
					else\
					{\
						WORD te;\
						te = cpuReg_A + (cB^255) + (cpuFlag_C&1);\
						cpuFlag_C = (BYTE)(te>>8);\
						cpuFlag_V = (( ((cpuReg_A^cB^255)^0x80) & (cpuReg_A^te) & 0x80)) ? 1:0;\
						cpuFlag_N = cpuFlag_Z = cpuReg_A = (BYTE)te;\
					}\
				}

#define ROL		{ BYTE cFlag; cFlag = cB>>7; cB = (cB<<1) + (cpuFlag_C&1); cpuFlag_C = cFlag; cpuFlag_N = cpuFlag_Z = cB; }
#define ROR		{ BYTE cFlag; cFlag = cB; cB = (cB>>1) + (cpuFlag_C<<7); cpuFlag_C = cFlag; cpuFlag_N = cpuFlag_Z = cB; }
#define LSR		{ cpuFlag_C = cB; cB>>=1; cpuFlag_N = cpuFlag_Z = cB; }
#define	ASLORA	{ cpuFlag_C = cB>>7; cB<<=1; cpuReg_A |= cB; cpuFlag_N = cpuFlag_Z = cpuReg_A; }
#define	ROLAND	{ BYTE cFlag; cFlag = cB>>7; cB = (cB<<1) + (cpuFlag_C&1); cpuFlag_C = cFlag; cpuReg_A &= cB; cpuFlag_N = cpuFlag_Z = cpuReg_A; }
#define LSREOR	{ cpuFlag_C = cB; cB>>=1; cpuReg_A ^= cB; cpuFlag_N = cpuFlag_Z = cpuReg_A; }
#define RORADC	{\
					BYTE cC = cB; cB = (cB>>1) + (cpuFlag_C<<7); cpuFlag_C = cC;\
					if( cpuFlag_D&1 )\
					{\
						BYTE al,ah;\
						al = (cpuReg_A & 0x0f) + (cB & 0x0f) + (cpuFlag_C&1);\
						if (al > 9) al += 6;\
						ah = ((cpuReg_A >> 4)&0x0F) + ((cB >> 4)&0x0F); if (al > 0x0f) ah++;\
						cpuFlag_N = cpuFlag_Z =  cpuReg_A + cB + (cpuFlag_C&1);\
						cpuFlag_V = (((ah << 4) ^ cpuReg_A) & 0x80) && !((cpuReg_A ^ cB) & 0x80) ? 1:0;\
						if (ah > 9) ah += 6; cpuFlag_C = (ah > 0x0f) ? 1:0; cpuReg_A = (ah << 4) | (al & 0x0f);\
					}\
					else\
					{\
						WORD te;\
						te = cpuReg_A + cB + (cpuFlag_C&1);\
						cpuFlag_C = (BYTE)(te>>8);\
						cpuFlag_V = (( ((cpuReg_A^cB)^0x80) & (cpuReg_A^te) & 0x80)) ? 1:0;\
						cpuFlag_N = cpuFlag_Z = cpuReg_A = (BYTE)te;\
					}\
				}

#define CondJump( a ) cpuReg_PC += (a) ? (WORD)(signed short int)(signed char)atariMem[cpuReg_PC+1]+2:2;

//-------

#define FREDDIEWRITEBYTE(ad,val)\
	{\
		if( (ad&0xFF00)==0xD200 )\
		{\
			if( isStereo==false )\
				pokeyWriteByte0( ad, val );\
			else if( (ad&0x10)==0 )\
				pokeyWriteByte0( ad, val );\
			else\
				pokeyWriteByte1( ad, val );\
		}\
		else if( ad==0xD40A )\
			holded = true;\
		else atariMem[ad] = val;\
	}

#define Load_IMD(a)		BYTE cB; {																							cB = atariMem[cpuReg_PC+1]; cpuReg_PC+=2; a; }
#define Load_ZP(a)		BYTE cB; { BYTE cA=atariMem[cpuReg_PC+1];															cB = atariMem[cA];	cpuReg_PC+=2; a; }
#define Load_ZPX(a)		BYTE cB; { BYTE cA=atariMem[cpuReg_PC+1]+cpuReg_X;													cB = atariMem[cA];	cpuReg_PC+=2; a; }
#define Load_ZPY(a)		BYTE cB; { BYTE cA=atariMem[cpuReg_PC+1]+cpuReg_Y;													cB = atariMem[cA];	cpuReg_PC+=2; a; }
#define Load_ABS(a)		BYTE cB; { WORD cWA = ((WORD*)&atariMem[cpuReg_PC+1])[0];											cB = freddieReadByte(cWA); cpuReg_PC+=3; a; }
#define Load_ABSX(a)	BYTE cB; { WORD cWA = ((WORD*)&atariMem[cpuReg_PC+1])[0] + (WORD)cpuReg_X;							cB = freddieReadByte(cWA); cpuReg_PC+=3; a; }
#define Load_ABSY(a)	BYTE cB; { WORD cWA = ((WORD*)&atariMem[cpuReg_PC+1])[0] + (WORD)cpuReg_Y;							cB = freddieReadByte(cWA); cpuReg_PC+=3; a; }
#define Load_PreX(a)	BYTE cB; { BYTE cA=(atariMem[cpuReg_PC+1]+cpuReg_X)&255; WORD cWA = ((WORD*)&atariMem[cA])[0];		cB = freddieReadByte(cWA); cpuReg_PC+=2; a; }
#define Load_PostY(a)	BYTE cB; { BYTE cA=atariMem[cpuReg_PC+1]; WORD cWA = ((WORD*)&atariMem[cA])[0] + (WORD)cpuReg_Y;	cB = freddieReadByte(cWA); cpuReg_PC+=2; a; }

#define LoadReg_IMD(a)		{																							a = cpuFlag_N = cpuFlag_Z = atariMem[cpuReg_PC+1]; cpuReg_PC+=2; }
#define LoadReg_ZP(a)		{ BYTE cA=atariMem[cpuReg_PC+1];															a = cpuFlag_N = cpuFlag_Z = atariMem[cA];	cpuReg_PC+=2; }
#define LoadReg_ZPX(a)		{ BYTE cA=atariMem[cpuReg_PC+1]+cpuReg_X;													a = cpuFlag_N = cpuFlag_Z = atariMem[cA];	cpuReg_PC+=2; }
#define LoadReg_ZPY(a)		{ BYTE cA=atariMem[cpuReg_PC+1]+cpuReg_Y;													a = cpuFlag_N = cpuFlag_Z = atariMem[cA];	cpuReg_PC+=2; }
#define LoadReg_ABS(a)		{ WORD cWA = ((WORD*)&atariMem[cpuReg_PC+1])[0];											a = cpuFlag_N = cpuFlag_Z = freddieReadByte(cWA); cpuReg_PC+=3; }
#define LoadReg_ABSX(a)		{ WORD cWA = ((WORD*)&atariMem[cpuReg_PC+1])[0] + (WORD)cpuReg_X;							a = cpuFlag_N = cpuFlag_Z = freddieReadByte(cWA); cpuReg_PC+=3; }
#define LoadReg_ABSY(a)		{ WORD cWA = ((WORD*)&atariMem[cpuReg_PC+1])[0] + (WORD)cpuReg_Y;							a = cpuFlag_N = cpuFlag_Z = freddieReadByte(cWA); cpuReg_PC+=3; }
#define LoadReg_PreX(a)		{ BYTE cA=(atariMem[cpuReg_PC+1]+cpuReg_X)&255; WORD cWA = ((WORD*)&atariMem[cA])[0];		a = cpuFlag_N = cpuFlag_Z = freddieReadByte(cWA); cpuReg_PC+=2; }
#define LoadReg_PostY(a)	{ BYTE cA=atariMem[cpuReg_PC+1]; WORD cWA = ((WORD*)&atariMem[cA])[0] + (WORD)cpuReg_Y;		a = cpuFlag_N = cpuFlag_Z = freddieReadByte(cWA); cpuReg_PC+=2; }

#define Modify_ZP(a)	BYTE cB; { BYTE cA=atariMem[cpuReg_PC+1];															cB = atariMem[cA];	cpuReg_PC+=2; a; atariMem[cA] = cB; }
#define Modify_ZPX(a)	BYTE cB; { BYTE cA=atariMem[cpuReg_PC+1]+cpuReg_X;													cB = atariMem[cA];	cpuReg_PC+=2; a; atariMem[cA] = cB; }
#define Modify_ABS(a)	BYTE cB; { WORD cWA = ((WORD*)&atariMem[cpuReg_PC+1])[0];											cB = freddieReadByte(cWA); cpuReg_PC+=3; a; FREDDIEWRITEBYTE(cWA,cB); }
#define Modify_ABSX(a)	BYTE cB; { WORD cWA = ((WORD*)&atariMem[cpuReg_PC+1])[0] + (WORD)cpuReg_X;							cB = freddieReadByte(cWA); cpuReg_PC+=3; a; FREDDIEWRITEBYTE(cWA,cB); }
#define Modify_ABSY(a)	BYTE cB; { WORD cWA = ((WORD*)&atariMem[cpuReg_PC+1])[0] + (WORD)cpuReg_Y;							cB = freddieReadByte(cWA); cpuReg_PC+=3; a; FREDDIEWRITEBYTE(cWA,cB); }
#define Modify_PreX(a)	BYTE cB; { BYTE cA=(atariMem[cpuReg_PC+1]+cpuReg_X)&255; WORD cWA = ((WORD*)&atariMem[cA])[0];		cB = freddieReadByte(cWA); cpuReg_PC+=2; a; FREDDIEWRITEBYTE(cWA,cB); }
#define Modify_PostY(a)	BYTE cB; { BYTE cA=atariMem[cpuReg_PC+1]; WORD cWA = ((WORD*)&atariMem[cA])[0] + (WORD)cpuReg_Y;	cB = freddieReadByte(cWA); cpuReg_PC+=2; a; FREDDIEWRITEBYTE(cWA,cB); }

#define Store_ZP(a)		{ BYTE cA=atariMem[cpuReg_PC+1];															cpuReg_PC+=2;	atariMem[cA] = a; }
#define Store_ZPX(a)	{ BYTE cA=atariMem[cpuReg_PC+1]+cpuReg_X;													cpuReg_PC+=2;	atariMem[cA] = a; }
#define Store_ABS(a)	{ WORD cWA = ((WORD*)&atariMem[cpuReg_PC+1])[0];											cpuReg_PC+=3;	FREDDIEWRITEBYTE(cWA,a); }
#define Store_ABSX(a)	{ WORD cWA = ((WORD*)&atariMem[cpuReg_PC+1])[0] + (WORD)cpuReg_X;							cpuReg_PC+=3;	FREDDIEWRITEBYTE(cWA,a); }
#define Store_ABSY(a)	{ WORD cWA = ((WORD*)&atariMem[cpuReg_PC+1])[0] + (WORD)cpuReg_Y;							cpuReg_PC+=3;	FREDDIEWRITEBYTE(cWA,a); }
#define Store_PreX(a)	{ BYTE cA=(atariMem[cpuReg_PC+1]+cpuReg_X)&255; WORD cWA = ((WORD*)&atariMem[cA])[0];		cpuReg_PC+=2;	FREDDIEWRITEBYTE(cWA,a); }
#define Store_PostY(a)	{ BYTE cA=atariMem[cpuReg_PC+1]; WORD cWA = ((WORD*)&atariMem[cA])[0] + (WORD)cpuReg_Y;		cpuReg_PC+=2;	FREDDIEWRITEBYTE(cWA,a); }

int opcode_0x00(bool &holded)	/* 0x00 - BRK				7 cycles	*/
{
	cpuReg_PC++;
	return 20;
}
int opcode_0x01(bool &holded)	/* 0x01 - ORA (,X)			6 cycles	*/
{
	Load_PreX( ORA );
	return 6;
}
int opcode_0x02(bool &holded)	/* 0x02 - ...				hang		*/
{
	return 20;
}
int opcode_0x03(bool &holded)	/* 0x03 - ASL:ORA (,X)		8 cycles	*/
{
	Modify_PreX( ASLORA );
	return 8;
}
int opcode_0x04(bool &holded)	/* 0x04 - NOP2				3 cycles	*/
{
	cpuReg_PC+=2;
	return 3;
}
int opcode_0x05(bool &holded)	/* 0x05 - ORA ZP			3 cycles	*/
{
	Load_ZP( ORA );
	return 3;
}
int opcode_0x06(bool &holded)	/* 0x06 - ASL ZP			5 cycles	*/
{
	Modify_ZP( ASL )
	return 5;
}
int opcode_0x07(bool &holded)	/* 0x07 - ASL:ORA ZP		5 cycles	*/
{
	Modify_ZP( ASLORA )
	return 5;
}
int opcode_0x08(bool &holded)	/* 0x08 - PHP				3 cycles	*/
{
BYTE te;
	cpuReg_PC++;
	te = (cpuFlag_N&0x80) | ((cpuFlag_V&1)<<6) | 0x20 | ((cpuFlag_B&1)<<4) | ((cpuFlag_D&1)<<3) | ((cpuFlag_I&1)<<2) | (cpuFlag_Z==0 ? 2:0) | (cpuFlag_C&1);
	atariMem[0x100 + cpuReg_S] = te; cpuReg_S--;
	return 3;
}
int opcode_0x09(bool &holded)	/* 0x09 - ORA #				2 cycles	*/
{
	Load_IMD( ORA );
	return 2;
}
int opcode_0x0A(bool &holded)	/* 0x0A - ASL @				2 cycles	*/
{
	cpuReg_PC++;
	cpuFlag_C = cpuReg_A>>7; cpuReg_A<<=1; cpuFlag_N = cpuFlag_Z = cpuReg_A;
	return 2;
}
int opcode_0x0B(bool &holded)	/* 0x0B - ????							*/
{
	return 20;
}
int opcode_0x0C(bool &holded)	/* 0x0C - NOP3				4 cycles	*/
{
	cpuReg_PC+=3;
	return 4;
}
int opcode_0x0D(bool &holded)	/* 0x0D - ORA ABS			4 cycles	*/
{
	Load_ABS( ORA );
	return 4;
}
int opcode_0x0E(bool &holded)	/* 0x0E - ASL ABS			6 cycles	*/
{
	Modify_ABS( ASL );
	return 6;
}
int opcode_0x0F(bool &holded)	/* 0x0F - ASL:ORA ABS		6 cycles	*/
{
	Modify_ABS( ASLORA );
	return 6;
}
int opcode_0x10(bool &holded)	/* 0x10 - BPL				2-4 cycles	*/
{
	CondJump( (cpuFlag_N&0x80)==0 );
	return 3;
}
int opcode_0x11(bool &holded)	/* 0x11 - ORA (),Y			5,6 cycles	*/
{
	Load_PostY( ORA );
	return 5;
}
int opcode_0x12(bool &holded)	/* 0x12 - ...				hang		*/
{
	return 20;
}
int opcode_0x13(bool &holded)	/* 0x13 - ASL:ORA (),Y		8 cycles	*/
{
	Modify_PostY( ASLORA );
	return 8;
}
int opcode_0x14(bool &holded)	/* 0x14 - NOP2							*/
{
	cpuReg_PC+=2;
	return 3;
}
int opcode_0x15(bool &holded)	/* 0x15 - ORA ZP,X			4 cycles	*/
{
	Load_ZPX( ORA );
	return 4;
}
int opcode_0x16(bool &holded)	/* 0x16 - ASL ZP,X			6 cycles	*/
{
	Modify_ZPX( ASL );
	return 6;
}
int opcode_0x17(bool &holded)	/* 0x17 - ASL:ORA ZP,X		6 cycles	*/
{
	Modify_ZPX( ASLORA );
	return 6;
}
int opcode_0x18(bool &holded)	/* 0x18 - CLC				2 cycles	*/
{
	cpuReg_PC++;
	cpuFlag_C = 0;
	return 2;
}
int opcode_0x19(bool &holded)	/* 0x19 - ORA ABS,Y			4,5 cycles	*/
{
	Load_ABSY( ORA );
	return 4;
}
int opcode_0x1A(bool &holded)	/* 0x1A - NOP1				2 cycles	*/
{
	cpuReg_PC++;
	return 2;
}
int opcode_0x1B(bool &holded)	/* 0x1B - ASL:ORA ABS,Y		7 cycles	*/
{
	Modify_ABSY( ASLORA );
	return 7;
}
int opcode_0x1C(bool &holded)	/* 0x1C - NOP3				7 cycles	*/
{
	cpuReg_PC+=3;
	return 7;
}
int opcode_0x1D(bool &holded)	/* 0x1D - ORA ABS,X			4,5 cycles	*/
{
	Load_ABSX( ORA );
	return 4;
}
int opcode_0x1E(bool &holded)	/* 0x1E - ASL ABS,X			7 cycles	*/
{
	Modify_ABSX( ASL );
	return 7;
}
int opcode_0x1F(bool &holded)	/* 0x1F - ASL:ORA ABS,X		7 cycles	*/
{
	Modify_ABSX( ASLORA );
	return 7;
}
int opcode_0x20(bool &holded)	/* 0x20 - JSR ABS			6 cycles	*/
{
	cpuReg_PC+=2;
	atariMem[0x100 + cpuReg_S] = (cpuReg_PC>>8)&0xFF; cpuReg_S--;
	atariMem[0x100 + cpuReg_S] = cpuReg_PC&0xFF; cpuReg_S--;
	cpuReg_PC = ((WORD*)&atariMem[cpuReg_PC-1])[0];
	return 6;
}
int opcode_0x21(bool &holded)	/* 0x21 - AND (,X)			6 cycles	*/
{
	Load_PreX( AND );
	return 6;
}
int opcode_0x22(bool &holded)	/* 0x22 - ...				hang		*/
{
	return 20;
}
int opcode_0x23(bool &holded)	/* 0x23 - ROL:AND (,X)		8 cycles	*/
{
	Modify_PreX( ROLAND );
	return 8;
}
int opcode_0x24(bool &holded)	/* 0x24 - BIT ZP			3 cycles	*/
{
BYTE cb;
	cb = atariMem[cpuReg_PC+1];
	cpuFlag_Z = cb&cpuReg_A; cpuFlag_N = cb; cpuFlag_V = cb>>6;
	cpuReg_PC+=2;
	return 3;
}
int opcode_0x25(bool &holded)	/* 0x25 - AND ZP			3 cycles	*/
{
	Load_ZP( AND );
	return 3;
}
int opcode_0x26(bool &holded)	/* 0x26 - ROL ZP			5 cycles	*/
{
	Modify_ZP( ROL );
	return 5;
}
int opcode_0x27(bool &holded)	/* 0x27 - ROL:AND ZP		5 cycles	*/
{
	Modify_ZP( ROLAND );
	return 5;
}
int opcode_0x28(bool &holded)	/* 0x28 - PLP				4 cycles	*/
{
BYTE te;
	cpuReg_PC++; cpuReg_S++; te = atariMem[0x100 + cpuReg_S];
	cpuFlag_N = te; cpuFlag_V = (te>>6)&1; cpuFlag_B = (te>>4)&1; cpuFlag_D = (te>>3)&1; cpuFlag_I = (te>>2)&1; cpuFlag_Z = (te&2)^2; cpuFlag_C = te&1;
	pokeyGenerateCheckIRQline();
	return 4;
}
int opcode_0x29(bool &holded)	/* 0x29 - AND #				2 cycles	*/
{
	Load_IMD( AND );
	return 2;
}
int opcode_0x2A(bool &holded)	/* 0x2A - ROL @				2 cycles	*/
{
BYTE cC;
	cpuReg_PC++;
	cC = cpuReg_A>>7; cpuReg_A = (cpuReg_A<<1) + (cpuFlag_C&1); cpuFlag_C = cC; cpuFlag_N = cpuFlag_Z = cpuReg_A;
	return 2;
}
int opcode_0x2B(bool &holded)	/* 0x2B - ????							*/
{
	return 20;
}
int opcode_0x2C(bool &holded)	/* 0x2C - BIT ABS			4 cycles	*/
{
BYTE cb;
	cb = atariMem[((WORD*)&atariMem[cpuReg_PC+1])[0]];
	cpuFlag_Z = cb&cpuReg_A; cpuFlag_N = cb; cpuFlag_V = cb>>6;
	cpuReg_PC+=3;
	return 4;
}
int opcode_0x2D(bool &holded)	/* 0x2D - AND ABS			4 cycles	*/
{
	Load_ABS( AND );
	return 4;
}
int opcode_0x2E(bool &holded)	/* 0x2E - ROL ABS			6 cycles	*/
{
	Modify_ABS( ROL );
	return 6;
}
int opcode_0x2F(bool &holded)	/* 0x2F - ROL:AND ABS		6 cycles	*/
{
	Modify_ABS( ROLAND );
	return 6;
}
int opcode_0x30(bool &holded)	/* 0x30 - BMI				2-4 cycles	*/
{
	CondJump( (cpuFlag_N&0x80) );
	return 2;
}
int opcode_0x31(bool &holded)	/* 0x31 - AND (),Y			5,6 cycles	*/
{
	Load_PostY( AND );
	return 5;
}
int opcode_0x32(bool &holded)	/* 0x32 - ...				hang		*/
{
	return 20;
}
int opcode_0x33(bool &holded)	/* 0x33 - ROL:AND (),Y		8 cycles	*/
{
	Modify_PostY( ROLAND );
	return 8;
}
int opcode_0x34(bool &holded)	/* 0x34 - NOP2						*/
{
	cpuReg_PC+=2;
	return 8;
}
int opcode_0x35(bool &holded)	/* 0x35 - AND ZP,X			4 cycles	*/
{
	Load_ZPX( AND );
	return 4;
}
int opcode_0x36(bool &holded)	/* 0x36 - ROL ZP,X			6 cycles	*/
{
	Modify_ZPX( ROL );
	return 6;
}
int opcode_0x37(bool &holded)	/* 0x37 - ROL:AND ZP,X		6 cycles	*/
{
	Modify_ZPX( ROLAND );
	return 6;
}
int opcode_0x38(bool &holded)	/* 0x38 - SEC				2 cycles	*/
{
	cpuReg_PC++;
	cpuFlag_C = 1;
	return 2;
}
int opcode_0x39(bool &holded)	/* 0x39 - AND ABS,Y			4,5 cycles	*/
{
	Load_ABSY( AND );
	return 4;
}
int opcode_0x3A(bool &holded)	/* 0x3A - NOP1						*/
{
	cpuReg_PC++;
	return 2;
}
int opcode_0x3B(bool &holded)	/* 0x3B - ROL:AND ABS,Y		7 cycles	*/
{
	Modify_ABSY( ROLAND );
	return 7;
}
int opcode_0x3C(bool &holded)	/* 0x3C - NOP3						*/
{
	cpuReg_PC+=3;
	return 4;
}
int opcode_0x3D(bool &holded)	/* 0x3D - AND ABS,X			4,5 cycles	*/
{
	Load_ABSX( AND );
	return 4;
}
int opcode_0x3E(bool &holded)	/* 0x3E - ROL ABS,X			7 cycles	*/
{
	Modify_ABSX( ROL );
	return 7;
}
int opcode_0x3F(bool &holded)	/* 0x3F - ROL:AND ABS,X		7 cycles	*/
{
	Modify_ABSX( ROLAND );
	return 7;
}
int opcode_0x40(bool &holded)	/* 0x40 - RTI				6 cycles	*/
{
BYTE te;
	cpuReg_PC++;
	cpuReg_S++; te = atariMem[0x100 + cpuReg_S];
	cpuFlag_N = te; cpuFlag_V = (te>>6)&1; cpuFlag_B = (te>>4)&1; cpuFlag_D = (te>>3)&1; cpuFlag_I = (te>>2)&1; cpuFlag_Z = (te&2)^2; cpuFlag_C = te&1;
	cpuReg_S++; cpuReg_PC =  (WORD)atariMem[0x100 + cpuReg_S];
	cpuReg_S++; cpuReg_PC+= ((WORD)atariMem[0x100 + cpuReg_S])*256;
	return 6;
}
int opcode_0x41(bool &holded)	/* 0x41 - EOR (,X)			6 cycles	*/
{
	Load_PreX( EOR );
	return 6;
}
int opcode_0x42(bool &holded)	/* 0x42 - ...				hang		*/
{
	return 20;
}
int opcode_0x43(bool &holded)	/* 0x43 - LSR:EOR (,X)		8 cycles	*/
{
	Modify_PreX( LSREOR );
	return 8;
}
int opcode_0x44(bool &holded)	/* 0x44 - NOP2				3 cycles	*/
{
	cpuReg_PC+=2;
	return 3;
}
int opcode_0x45(bool &holded)	/* 0x45 - EOR ZP			3 cycles	*/
{
	Load_ZP( EOR );
	return 3;
}
int opcode_0x46(bool &holded)	/* 0x46 - LSR ZP			5 cycles	*/
{
	Modify_ZP( LSR );
	return 5;
}
int opcode_0x47(bool &holded)	/* 0x47 - LSR:EOR ZP		5 cycles	*/
{
	Modify_ZP( LSREOR );
	return 5;
}
int opcode_0x48(bool &holded)	/* 0x48 - PHA				3 cycles	*/
{
	cpuReg_PC++;
	atariMem[0x100 + cpuReg_S] = cpuReg_A; cpuReg_S--;
	return 3;
}
int opcode_0x49(bool &holded)	/* 0x49 - EOR #				2 cycles	*/
{
	Load_IMD( EOR );
	return 2;
}
int opcode_0x4A(bool &holded)	/* 0x4A - LSR @				2 cycles	*/
{
	cpuReg_PC++;
	cpuFlag_C = cpuReg_A; cpuReg_A>>=1; cpuFlag_N = cpuFlag_Z = cpuReg_A;
	return 2;
}
int opcode_0x4B(bool &holded)	/* 0x4B - ????							*/
{
	cpuReg_PC--;
	return 20;
}
int opcode_0x4C(bool &holded)	/* 0x4C - JMP				3 cycles	*/
{
	cpuReg_PC = ((WORD*)&atariMem[cpuReg_PC+1])[0];
	return 3;
}
int opcode_0x4D(bool &holded)	/* 0x4D - EOR ABS			4 cycles	*/
{
	Load_ABS( EOR );
	return 4;
}
int opcode_0x4E(bool &holded)	/* 0x4E - LSR ABS			6 cycles	*/
{
	Modify_ABS( LSR );
	return 6;
}
int opcode_0x4F(bool &holded)	/* 0x4F - LSR:EOR ABS		6 cycles	*/
{
	Modify_ABS( LSREOR );
	return 6;
}

int opcode_0x50(bool &holded)	/* 0x50 - BVC				2-4 cycles	*/
{
	CondJump( (cpuFlag_V&1)==0 );
	return 2;
}

int opcode_0x51(bool &holded)	/* 0x51 - EOR (),Y			5 cycles	*/
{
	Load_PostY( EOR );
	return 5;
}
int opcode_0x52(bool &holded)	/* 0x52 - ...				hang		*/
{
	return 20;
}
int opcode_0x53(bool &holded)	/* 0x53 - LSR:EOR (),Y		8 cycles	*/
{
	Modify_PostY( LSREOR );
	return 8;
}
int opcode_0x54(bool &holded)	/* 0x54 - NOP2							*/
{
	cpuReg_PC+=2;
	return 3;
}
int opcode_0x55(bool &holded)	/* 0x55 - EOR ZP,X			4 cycles	*/
{
	Load_ZPX( EOR );
	return 4;
}
int opcode_0x56(bool &holded)	/* 0x56 - LSR ZP,X			6 cycles	*/
{
	Modify_ZPX( LSR );
	return 6;
}
int opcode_0x57(bool &holded)	/* 0x57 - LSR:EOR ZP,X			6 cycles	*/
{
	Modify_ZPX( LSREOR );
	return 6;
}
int opcode_0x58(bool &holded)	/* 0x58 - CLI				2 cycles	*/
{
	cpuReg_PC++;
	cpuFlag_I = 0;
	pokeyGenerateCheckIRQline();
	return 2;
}
int opcode_0x59(bool &holded)	/* 0x59 - EOR ABS,Y			4,5 cycles	*/
{
	Load_ABSY( EOR );
	return 4;
}
int opcode_0x5A(bool &holded)	/* 0x5A - NOP1				2 cycles	*/
{
	cpuReg_PC++;
	return 2;
}
int opcode_0x5B(bool &holded)	/* 0x5B - LSR:EOR ABS,Y		7 cycles	*/
{
	Modify_ABSY( EOR );
	return 7;
}
int opcode_0x5C(bool &holded)	/* 0x5C - NOP3				7 cycles	*/
{
	cpuReg_PC+=3;
	return 7;
}
int opcode_0x5D(bool &holded)	/* 0x5D - EOR ABS,X			4,5 cycles	*/
{
	Load_ABSX( EOR );
	return 4;
}
int opcode_0x5E(bool &holded)	/* 0x5E - LSR ABS,X			7 cycles	*/
{
	Modify_ABSX( EOR );
	return 7;
}
int opcode_0x5F(bool &holded)	/* 0x5F - LSR:EOR ABS,X		7 cycles	*/
{
	Modify_ABSX( LSREOR );
	return 7;
}

int opcode_0x60(bool &holded)	/* 0x60 - RTS				6 cycles	*/
{
	cpuReg_S++; cpuReg_PC =  (WORD)atariMem[0x100 + cpuReg_S];
	cpuReg_S++; cpuReg_PC+= ((WORD)atariMem[0x100 + cpuReg_S])*256 + 1;
	return 6;
}
int opcode_0x61(bool &holded)	/* 0x61 - ADC (,X)			6 cycles	*/
{
	Load_PreX( ADC );
	return 6;
}
int opcode_0x62(bool &holded)	/* 0x62 - ...				hang		*/
{
	return 20;
}
int opcode_0x63(bool &holded)	/* 0x63 - ROR:ADC (,X)		8 cycles	*/
{
	Modify_PreX( RORADC );
	return 8;
}
int opcode_0x64(bool &holded)	/* 0x64 - NOP2				3 cycles	*/
{
	cpuReg_PC+=2;
	return 3;
}
int opcode_0x65(bool &holded)	/* 0x65 - ADC ZP			3 cycles	*/
{
	Load_ZP( ADC );
	return 3;
}
int opcode_0x66(bool &holded)	/* 0x66 - ROR ZP			5 cycles	*/
{
	Modify_ZP( ROR );
	return 5;
}
int opcode_0x67(bool &holded)	/* 0x67 - ROR:ADC ZP		5 cycles	*/
{
	Modify_ZP( RORADC );
	return 5;
}
int opcode_0x68(bool &holded)	/* 0x68 - PLA				4 cycles	*/
{			
	cpuReg_PC++;
	cpuReg_S++; cpuFlag_N = cpuFlag_Z = cpuReg_A = atariMem[0x100 + cpuReg_S];
	return 4;
}
int opcode_0x69(bool &holded)	/* 0x69 - ADC #				2 cycles	*/
{
	Load_IMD( ADC );
	return 2;
}
int opcode_0x6A(bool &holded)	/* 0x6A - ROR @				2 cycles	*/
{
BYTE cC;
	cpuReg_PC++;
	cC = cpuReg_A; cpuReg_A = (cpuReg_A>>1) + (cpuFlag_C<<7); cpuFlag_C = cC; cpuFlag_N = cpuFlag_Z = cpuReg_A;
	return 2;
}
int opcode_0x6B(bool &holded)	/* 0x6B - ????							*/
{
	return 20;
}
int opcode_0x6C(bool &holded)	/* 0x6C - JMP ( )			6 cycles	*/
{
	WORD wA = ((WORD*)&atariMem[cpuReg_PC+1])[0];
	wA = ((WORD*)&atariMem[wA])[0];
	cpuReg_PC = wA;
	return 6;
}
int opcode_0x6D(bool &holded)	/* 0x6D - ADC ABS			4 cycles	*/
{
	Load_ABS( ADC );
	return 4;
}
int opcode_0x6E(bool &holded)	/* 0x6E - ROR ABS			6 cycles	*/
{
	Modify_ABS( ROR );
	return 6;
}
int opcode_0x6F(bool &holded)	/* 0x6F - ROR:ADC ABS		6 cycles	*/
{
	Modify_ABS( RORADC );
	return 6;
}
int opcode_0x70(bool &holded)	/* 0x70 - BVS				2-4 cycles	*/
{
	CondJump( (cpuFlag_V&1) );
	return 2;
}
int opcode_0x71(bool &holded)	/* 0x71 - ADC ( ),Y			5,6 cycles	*/
{
	Load_PostY( ADC );
	return 5;
}
int opcode_0x72(bool &holded)	/* 0x72 - ...				hang		*/
{
	return 20;
}
int opcode_0x73(bool &holded)	/* 0x73 - ROR:ADC ( ),Y		8 cycles	*/
{
	Modify_PostY( RORADC );
	return 8;
}
int opcode_0x74(bool &holded)	/* 0x74 - NOP2				4 cycles	*/
{
	return 4;
}
int opcode_0x75(bool &holded)	/* 0x75 - ADC ZP,X			4 cycles	*/
{
	Load_ZPX( ADC );
	return 4;
}
int opcode_0x76(bool &holded)	/* 0x76 - ROR ZP,X			6 cycles	*/
{
	Modify_ZPX( ROR );
	return 6;
}
int opcode_0x77(bool &holded)	/* 0x77 - ROR:ADC ZP,X		6 cycles	*/
{
	Modify_ZPX( RORADC );
	return 6;
}
int opcode_0x78(bool &holded)	/* 0x78 - SEI				2 cycles	*/
{
	cpuReg_PC++;
	cpuFlag_I = 1;
	return 2;
}
int opcode_0x79(bool &holded)	/* 0x79 - ADC ABS,Y			4,5 cycles	*/
{
	Load_ABSY( ADC );
	return 4;
}
int opcode_0x7A(bool &holded)	/* 0x7A - NOP1				2 cycles	*/
{
	cpuReg_PC++;
	return 2;
}
int opcode_0x7B(bool &holded)	/* 0x7B - ROR:ADC ABS,Y		6 cycles	*/
{
	Modify_ABSY( RORADC );
	return 6;
}
int opcode_0x7C(bool &holded)	/* 0x7C - NOP3				7 cycles	*/
{
	cpuReg_PC+=3;
	return 7;
}
int opcode_0x7D(bool &holded)	/* 0x7D - ADC ABS,X			4,5 cycles	*/
{
	Load_ABSX( ADC );
	return 4;
}
int opcode_0x7E(bool &holded)	/* 0x7E - ROR ABS,X			7 cycles	*/
{
	Modify_ABSX( ROR );
	return 7;
}
int opcode_0x7F(bool &holded)	/* 0x7F - ROR:ADC ABS,X		7 cycles	*/
{
	Modify_ABSX( RORADC );
	return 7;
}

int opcode_0x80(bool &holded)	/* 0x80 - NOP2				2 cycles	*/
{
	cpuReg_PC+=2;
	return 2;
}
int opcode_0x81(bool &holded)	/* 0x81 - STA ( ,X)			6 cycles	*/
{
	Store_PreX( cpuReg_A );
	return 6;
}
int opcode_0x82(bool &holded)	/* 0x82 - NOP2				2 cycles	*/
{
	cpuReg_PC+=2;
	return 2;
}
int opcode_0x83(bool &holded)	/* 0x83 - STORE(A&X)	(,X)	6 cycles	*/
{
	Store_PreX( (cpuReg_A&cpuReg_X) );
	return 6;
}
int opcode_0x84(bool &holded)	/* 0x84 - STY ZP				3 cycles	*/
{
	atariMem[atariMem[cpuReg_PC+1]] = cpuReg_Y;
	cpuReg_PC+=2;
	return 3;
}
int opcode_0x85(bool &holded)	/* 0x85 - STA ZP				3 cycles	*/
{
	atariMem[atariMem[cpuReg_PC+1]] = cpuReg_A;
	cpuReg_PC+=2;
	return 3;
}
int opcode_0x86(bool &holded)	/* 0x86 - STX ZP				3 cycles	*/
{
	atariMem[atariMem[cpuReg_PC+1]] = cpuReg_X;
	cpuReg_PC+=2;
	return 3;
}
int opcode_0x87(bool &holded)	/* 0x87 - STORE(A&X)	ZP		3 cycles	*/
{
	atariMem[atariMem[cpuReg_PC+1]] = cpuReg_A&cpuReg_X;
	cpuReg_PC+=2;
	return 3;
}
int opcode_0x88(bool &holded)	/* 0x88 - DEY				2 cycles	*/
{
	cpuReg_PC++;
	cpuReg_Y--;
	cpuFlag_N = cpuFlag_Z = cpuReg_Y;
	return 2;
}
int opcode_0x89(bool &holded)	/* 0x89 - NOP2				2 cycles	*/
{
	cpuReg_PC+=2;
	return 2;
}
int opcode_0x8A(bool &holded)	/* 0x8A - TXA				2 cycles	*/
{
	cpuReg_PC+=1;
	cpuFlag_N = cpuFlag_Z = cpuReg_A = cpuReg_X;
	return 2;
}
int opcode_0x8B(bool &holded)	/* 0x8B - TXA:AND #			2 cycles	*/
{
	cpuReg_A = cpuReg_X;
	cpuFlag_N = cpuFlag_Z = cpuReg_A = cpuReg_A & atariMem[cpuReg_PC+1];
	cpuReg_PC+=2;
	return 2;
}
int opcode_0x8C(bool &holded)	/* 0x8C - STY ABS			4 cycles	*/
{
	Store_ABS( cpuReg_Y );
	return 4;
}
int opcode_0x8D(bool &holded)	/* 0x8D - STA ABS			4 cycles	*/
{
	Store_ABS( cpuReg_A );
	return 4;
}
int opcode_0x8E(bool &holded)	/* 0x8E - STX ABS			4 cycles	*/
{
	Store_ABS( cpuReg_X );
	return 4;
}
int opcode_0x8F(bool &holded)	/* 0x8F - STORE(A&X)	ABS		4 cycles	*/
{
	Store_ABS( (cpuReg_A&cpuReg_X) );
	return 4;
}
int opcode_0x90(bool &holded)	/* 0x90 - BCC				2-4 cycles	*/
{
	CondJump( (cpuFlag_C&1)==0 );
	return 2;
}
int opcode_0x91(bool &holded)	/* 0x91 - STA ( ),Y			6 cycles	*/
{
	Store_PostY( cpuReg_A );
	return 6;
}
int opcode_0x92(bool &holded)	/* 0x92 - ...				hang		*/
{
	return 20;
}
int opcode_0x93(bool &holded)	/* 0x93 - STORE(A&X)	(),Y	4,5 cycles	*/
{
	Store_PostY( (cpuReg_A&cpuReg_X) );
	return 6;
}
int opcode_0x94(bool &holded)	/* 0x94 - STY ZP,X			4 cycles	*/
{
	atariMem[(atariMem[cpuReg_PC+1]+cpuReg_X)&0xFF] = cpuReg_Y;
	cpuReg_PC+=2;
	return 4;
}
int opcode_0x95(bool &holded)	/* 0x95 - STA ZP,X			4 cycles	*/
{
	atariMem[(atariMem[cpuReg_PC+1]+cpuReg_X)&0xFF] = cpuReg_A;
	cpuReg_PC+=2;
	return 4;
}
int opcode_0x96(bool &holded)	/* 0x96 - STX ZP,Y			4 cycles	*/
{
	atariMem[(atariMem[cpuReg_PC+1]+cpuReg_Y)&0xFF] = cpuReg_X;
	cpuReg_PC+=2;
	return 4;
}
int opcode_0x97(bool &holded)	/* 0x97 - STORE(A&X)	ZP,Y	4 cycles	*/
{
	atariMem[(atariMem[cpuReg_PC+1]+cpuReg_Y)&0xFF] = cpuReg_X&cpuReg_A;
	cpuReg_PC+=2;
	return 4;
}
int opcode_0x98(bool &holded)	/* 0x98 - TYA				2 cycles	*/
{
	cpuReg_PC++;
	cpuFlag_N = cpuFlag_Z = cpuReg_A = cpuReg_Y;
	return 2;
}
int opcode_0x99(bool &holded)	/* 0x99 - STA ABS,Y			5 cycles	*/
{
	Store_ABSY( cpuReg_A );
	return 5;
}
int opcode_0x9A(bool &holded)	/* 0x9A - TXS				2 cycles	*/
{
	cpuReg_PC++;
	cpuReg_S = cpuReg_X;
	return 2;
}
int opcode_0x9B(bool &holded)	/* 0x9B - ????							*/
{
	return 20;
}
int opcode_0x9C(bool &holded)	/* 0x9C - ????							*/
{
	return 20;
}
int opcode_0x9D(bool &holded)	/* 0x9D - STA ABS,X			5 cycles	*/
{
	Store_ABSX( cpuReg_A );
	return 5;
}
int opcode_0x9E(bool &holded)	/* 0x9E - ????							*/
{
	return 20;
}
int opcode_0x9F(bool &holded)	/* 0x9F - ????							*/
{
	return 20;
}
int opcode_0xA0(bool &holded)	/* 0xA0 - LDY #				2 cycles	*/
{
	LoadReg_IMD( cpuReg_Y );
	return 2;
}
int opcode_0xA1(bool &holded)	/* 0xA1 - LDA (,X)			6 cycles	*/
{
	LoadReg_PreX( cpuReg_A );
	return 6;
}
int opcode_0xA2(bool &holded)	/* 0xA2 - LDX #				2 cycles	*/
{
	LoadReg_IMD( cpuReg_X );
	return 2;
}
int opcode_0xA3(bool &holded)	/* 0xA3 - LDA:TAX (,X)		6 cycles	*/
{
	LoadReg_IMD( cpuReg_A );
	cpuReg_X = cpuReg_A;
	return 6;
}
int opcode_0xA4(bool &holded)	/* 0xA4 - LDY ZP				3 cycles	*/
{
	LoadReg_ZP( cpuReg_Y );
	return 3;
}
int opcode_0xA5(bool &holded)	/* 0xA5 - LDA ZP				3 cycles	*/
{
	LoadReg_ZP( cpuReg_A );
	return 3;
}
int opcode_0xA6(bool &holded)	/* 0xA6 - LDX ZP				3 cycles	*/
{
	LoadReg_ZP( cpuReg_X );
	return 3;
}
int opcode_0xA7(bool &holded)	/* 0xA7 - LDA:TAX ZP			3 cycles	*/
{
	LoadReg_ZP( cpuReg_A );
	cpuReg_X = cpuReg_A;
	return 3;
}
int opcode_0xA8(bool &holded)	/* 0xA8 - TAY				2 cycles	*/
{
	cpuReg_PC++;
	cpuFlag_N = cpuFlag_Z = cpuReg_Y = cpuReg_A;
	return 2;
}
int opcode_0xA9(bool &holded)	/* 0xA9 - LDA #				2 cycles	*/
{
	LoadReg_IMD( cpuReg_A );
	return 2;
}
int opcode_0xAA(bool &holded)	/* 0xAA - TAX				2 cycles	*/
{
	cpuReg_PC++;
	cpuFlag_N = cpuFlag_Z = cpuReg_X = cpuReg_A;
	return 2;
}
int opcode_0xAB(bool &holded)	/* 0xAB - AND #:TAX			2 cycles	*/
{
	cpuFlag_N = cpuFlag_Z = cpuReg_A = cpuReg_X = cpuReg_A & atariMem[cpuReg_PC+1];
	cpuReg_PC+=2;
	return 2;
}
int opcode_0xAC(bool &holded)	/* 0xAC - LDY ABS			4 cycles	*/
{
	LoadReg_ABS( cpuReg_Y );
	return 4;
}
int opcode_0xAD(bool &holded)	/* 0xAD - LDA ABS			4 cycles	*/
{
	LoadReg_ABS( cpuReg_A );
	return 4;
}
int opcode_0xAE(bool &holded)	/* 0xAE - LDX ABS			4 cycles	*/
{
	LoadReg_ABS( cpuReg_X );
	return 4;
}
int opcode_0xAF(bool &holded)	/* 0xAF - LDA:TAX ABS		4 cycles	*/
{
	LoadReg_ABS( cpuReg_A );
	cpuReg_X = cpuReg_A;
	return 4;
}
int opcode_0xB0(bool &holded)	/* 0xB0 - BCS				2-4 cycles	*/
{
	CondJump( (cpuFlag_C&1) );
	return 2;
}
int opcode_0xB1(bool &holded)	/* 0xB1 - LDA ( ),Y			5,6 cycles	*/
{
	LoadReg_PostY( cpuReg_A );
	return 5;
}
int opcode_0xB2(bool &holded)	/* 0xB2 - ...				hang		*/
{
	return 20;
}
int opcode_0xB3(bool &holded)	/* 0xB3 - LDA:TAX ( ),Y		5,6 cycles	*/
{
	LoadReg_PostY( cpuReg_A );
	cpuReg_X = cpuReg_A;
	return 5;
}
int opcode_0xB4(bool &holded)	/* 0xB4 - LDY ZP,X			4 cycles	*/
{
	LoadReg_ZPX( cpuReg_Y );
	return 4;
}
int opcode_0xB5(bool &holded)	/* 0xB5 - LDA ZP,X			4 cycles	*/
{
	LoadReg_ZPX( cpuReg_A );
	return 4;
}
int opcode_0xB6(bool &holded)	/* 0xB6 - LDX ZP,Y			4 cycles	*/
{
	LoadReg_ZPY( cpuReg_X );
	return 4;
}
int opcode_0xB7(bool &holded)	/* 0xB7 - LDA:TAX ZP,Y		4 cycles	*/
{
	LoadReg_ZPY( cpuReg_X );
	cpuReg_A = cpuReg_X;
	return 4;
}
int opcode_0xB8(bool &holded)	/* 0xB8 - CLV				2 cycles	*/
{
	cpuReg_PC++;
	cpuFlag_V = 0;
	return 2;
}
int opcode_0xB9(bool &holded)	/* 0xB9 - LDA ABS,Y			4,5 cycles	*/
{
	LoadReg_ABSY( cpuReg_A );
	return 4;
}
int opcode_0xBA(bool &holded)	/* 0xBA - TSX				2 cycles	*/
{
	cpuReg_PC++;
	cpuFlag_N = cpuFlag_Z = cpuReg_X = cpuReg_S;
	return 2;
}
int opcode_0xBB(bool &holded)	/* 0xBB - ????							*/
{
	return 20;
}
int opcode_0xBC(bool &holded)	/* 0xBC - LDY ABS,X			4,5 cycles	*/
{
	LoadReg_ABSX( cpuReg_Y );
	return 4;
}
int opcode_0xBD(bool &holded)	/* 0xBD - LDA ABS,X			4,5 cycles	*/
{
	LoadReg_ABSX( cpuReg_A );
	return 4;
}
int opcode_0xBE(bool &holded)	/* 0xBE - LDX ABS,Y			4,5 cycles	*/
{
	LoadReg_ABSY( cpuReg_X );
	return 4;
}
int opcode_0xBF(bool &holded)	/* 0xBF - LDA:TAX ABS,Y		4,5 cycles	*/
{
	LoadReg_ABSY( cpuReg_X );
	cpuReg_A = cpuReg_X;
	return 4;
}
int opcode_0xC0(bool &holded)	/* 0xC0 - CPY #				2 cycles	*/
{
	Load_IMD( CPY );
	return 2;
}
int opcode_0xC1(bool &holded)	/* 0xC1 - CMP (,X)			6 cycles	*/
{
	Load_PreX( CMP );
	return 6;
}
int opcode_0xC2(bool &holded)	/* 0xC2 - NOP2				2 cycles	*/
{
	cpuReg_PC+=2;
	return 2;
}
int opcode_0xC3(bool &holded)	/* 0xC3 - DEC:CMP (,X)		8 cycles	*/
{
	Modify_PreX( DECCMP );
	return 8;
}
int opcode_0xC4(bool &holded)	/* 0xC4 - CPY ZP				3 cycles	*/
{
	Load_ZP( CPY );
	return 3;
}
int opcode_0xC5(bool &holded)	/* 0xC5 - CMP ZP				3 cycles	*/
{
	Load_ZP( CMP );
	return 3; 
}
int opcode_0xC6(bool &holded)	/* 0xC6 - DEC ZP				5 cycles	*/
{
	Modify_ZP( DEC );
	return 5;
}
int opcode_0xC7(bool &holded)	/* 0xC7 - DEC:CMP ZP			5 cycles	*/
{
	Modify_ZP( DECCMP );
	return 8;
}
int opcode_0xC8(bool &holded)	/* 0xC8 - INY				2 cycles	*/
{
	cpuReg_PC++;
	cpuFlag_N = cpuFlag_Z = cpuReg_Y = cpuReg_Y+1;
	return 2;
}
int opcode_0xC9(bool &holded)	/* 0xC9 - CMP #				2 cycles	*/
{
	Load_IMD( CMP );
	return 2;
}
int opcode_0xCA(bool &holded)	/* 0xCA - DEX				2 cycles	*/
{
	cpuReg_PC++;
	cpuFlag_N = cpuFlag_Z = cpuReg_X = cpuReg_X-1;
	return 2;
}
int opcode_0xCB(bool &holded)	/* 0xCB - ????							*/
{
	return 20;
}
int opcode_0xCC(bool &holded)	/* 0xCC - CPY ABS			4 cycles	*/
{
	Load_ABS( CPY );
	return 4;
}
int opcode_0xCD(bool &holded)	/* 0xCD - CMP ABS			4 cycles	*/
{
	Load_ABS( CMP );
	return 4;
}
int opcode_0xCE(bool &holded)	/* 0xCE - DEC ABS			6 cycles	*/
{
	Modify_ABS( DEC );
	return 6;
}
int opcode_0xCF(bool &holded)	/* 0xCF - DEC:CMP ABS		6 cycles	*/
{
	Modify_ABS( DECCMP );
	return 6;
}
int opcode_0xD0(bool &holded)	/* 0xD0 - BNE				2-4 cycles	*/
{
	CondJump( cpuFlag_Z );
	return 2;
}
int opcode_0xD1(bool &holded)	/* 0xD1 - CMP (),Y			5,6 cycles	*/
{
	Load_PostY( CMP );
	return 5;
}
int opcode_0xD2(bool &holded)	/* 0xD2 - ...				hang		*/
{
	return 20;
}
int opcode_0xD3(bool &holded)	/* 0xD3 - DEC:CMP (),Y			8 cycles	*/
{
	Modify_PostY( DECCMP );
	return 8;
}
int opcode_0xD4(bool &holded)	/* 0xD4 - NOP2				4 cycles	*/
{
	cpuReg_PC+=2;
	return 4;
}
int opcode_0xD5(bool &holded)	/* 0xD5 - CMP ZP,X			4 cycles	*/
{
	Load_ZPX( CMP );
	return 4;
}
int opcode_0xD6(bool &holded)	/* 0xD6 - DEC ZP,X			6 cycles	*/
{
	Modify_ZPX( DEC );
	return 6;
}
int opcode_0xD7(bool &holded)	/* 0xD7 - DEC:CMP ZP,X		6 cycles	*/
{
	Modify_ZPX( DECCMP );
	return 6;
}
int opcode_0xD8(bool &holded)	/* 0xD8 - CLD				2 cycles	*/
{
	cpuReg_PC++;
	cpuFlag_D = 0;
	return 2;
}
int opcode_0xD9(bool &holded)	/* 0xD9 - CMP ABS,Y			4,5 cycles	*/
{			
	Load_ABSY( CMP );
	return 4;
}
int opcode_0xDA(bool &holded)	/* 0xDA - NOP1				2 cycles	*/
{
	cpuReg_PC++;
	return 2;
}
int opcode_0xDB(bool &holded)	/* 0xDB - DEC:CMP ABS,Y		7 cycles	*/
{
	Modify_ABSY( DECCMP );
	return 7;
}
int opcode_0xDC(bool &holded)	/* 0xDC - NOP3				4 cycles	*/
{
	cpuReg_PC+=3;
	return 4;
}
int opcode_0xDD(bool &holded)	/* 0xDD - CMP ABS,X			4,5 cycles	*/
{
	Load_ABSX( CMP );
	return 4;
}
int opcode_0xDE(bool &holded)	/* 0xDE - DEC ABS,X			7 cycles	*/
{
	Modify_ABSX( DEC );
	return 7;
}
int opcode_0xDF(bool &holded)	/* 0xDF - DEC:CMP ABS,X		7 cycles	*/
{			
	Modify_ABSX( DECCMP );
	return 7;
}
int opcode_0xE0(bool &holded)	/* 0xE0 - CPX #				2 cycles	*/
{
	Load_IMD( CPX );
	return 2;
}
int opcode_0xE1(bool &holded)	/* 0xE1 - SBC (,X)			6 cycles	*/
{
	Load_PreX( SBC );
	return 6;
}
int opcode_0xE2(bool &holded)	/* 0xE2 - NOP2				2 cycles	*/
{
	cpuReg_PC+=2;
	return 2;
}
int opcode_0xE3(bool &holded)	/* 0xE3 - INC:SBC (,X)		8 cycles	*/
{
	Modify_PreX( INCSBC );
	return 8;
}
int opcode_0xE4(bool &holded)	/* 0xE4 - CPX ZP				3 cycles	*/
{
	Load_ZP( CPX );
	return 3;
}
int opcode_0xE5(bool &holded)	/* 0xE5 - SBC ZP				3 cycles	*/
{
	Load_ZP( SBC );
	return 3;
}
int opcode_0xE6(bool &holded)	/* 0xE6 - INC ZP				5 cycles	*/
{
	Modify_ZP( INC );
	return 5;
}
int opcode_0xE7(bool &holded)	/* 0xE7 - INC:SBC ZP			5 cycles	*/
{
	Modify_ZP( INCSBC );
	return 5;
}
int opcode_0xE8(bool &holded)	/* 0xE8 - INX				2 cycles	*/
{
	cpuReg_PC++;
	cpuFlag_N = cpuFlag_Z = cpuReg_X = cpuReg_X+1;
	return 2;
}
int opcode_0xE9(bool &holded)	/* 0xE9 - SBC #				2 cycles	*/
{
	Load_IMD( SBC );
	return 2;
}
int opcode_0xEA(bool &holded)	/* 0xEA - NOP				2 cycles	*/
{
	cpuReg_PC++;
	return 2;
}
int opcode_0xEB(bool &holded)	/* 0xEB - ????							*/
{
	return 20;
}
int opcode_0xEC(bool &holded)	/* 0xEC - CPX ABS			4 cycles	*/
{
	Load_ABS( CPX );
	return 4;
}
int opcode_0xED(bool &holded)	/* 0xED - SBC ABS			4 cycles	*/
{
	Load_ABS( SBC );
	return 4;
}
int opcode_0xEE(bool &holded)	/* 0xEE - INC ABS			6 cycles	*/
{
	Modify_ABS( INC );
	return 6;
}
int opcode_0xEF(bool &holded)	/* 0xEF - INC:SBC ABS		6 cycles	*/
{
	Modify_ABS( INCSBC );
	return 6;
}
int opcode_0xF0(bool &holded)	/* 0xF0 - BEQ				2-4 cycles	*/
{
	CondJump( (cpuFlag_Z==0) );
	return 2;
}
int opcode_0xF1(bool &holded)	/* 0xF1 - SBC (),Y			5,6 cycles	*/
{
	Load_PostY( SBC );
	return 5;
}
int opcode_0xF2(bool &holded)	/* 0xF2 - ...				hang		*/
{
	return 20;
}
int opcode_0xF3(bool &holded)	/* 0xF3 - INC:SBC (),Y			8 cycles	*/
{
	Modify_PostY( INCSBC );
	return 8;
}
int opcode_0xF4(bool &holded)	/* 0xF4 - NOP2				4 cycles	*/
{
	cpuReg_PC+=2;
	return 4;
}
int opcode_0xF5(bool &holded)	/* 0xF5 - SBC ZP,X			4 cycles	*/
{
	Load_ZPX( SBC );
	return 4;
}
int opcode_0xF6(bool &holded)	/* 0xF6 - INC ZP,X			6 cycles	*/
{
	Modify_ZPX( INC );
	return 6;
}
int opcode_0xF7(bool &holded)	/* 0xF7 - INC:SBC ZP,X		6 cycles	*/
{
	Modify_ZPX( INCSBC );
	return 6;
}
int opcode_0xF8(bool &holded)	/* 0xF8 - SED				2 cycles	*/
{
	cpuReg_PC++;
	cpuFlag_D = 1;
	return 2;
}
int opcode_0xF9(bool &holded)	/* 0xF9 - SBC ABS,Y			4,5 cycles	*/
{
	Load_ABSY( SBC );
	return 4;
}
int opcode_0xFA(bool &holded)	/* 0xFA - NOP1				2 cycles	*/
{
	cpuReg_PC++;
	return 2;
}
int opcode_0xFB(bool &holded)	/* 0xFB - INC:SBC ABS,Y		7 cycles	*/
{
	Modify_ABSY( INCSBC );
	return 7;
}
int opcode_0xFC(bool &holded)	/* 0xDC - NOP3				4 cycles	*/
{
	cpuReg_PC+=3;
	return 4;
}
int opcode_0xFD(bool &holded)	/* 0xFD - SBC ABS,X			4,5 cycles	*/
{
	Load_ABSX( SBC );
	return 4;
}
int opcode_0xFE(bool &holded)	/* 0xFE - INC ABS,X			7 cycles	*/
{
	Modify_ABSX( INC );
	return 7;
}
int opcode_0xFF(bool &holded)	/* 0xDF - INC:SBC ABS,X		7 cycles	*/
{
	Modify_ABSX( INCSBC );
	return 7;
}

opcodeFunc opcodes_0x00_0xFF[256]={
	&opcode_0x00, &opcode_0x01, &opcode_0x02, &opcode_0x03, &opcode_0x04, &opcode_0x05, &opcode_0x06, &opcode_0x07,
	&opcode_0x08, &opcode_0x09, &opcode_0x0A, &opcode_0x0B, &opcode_0x0C, &opcode_0x0D, &opcode_0x0E, &opcode_0x0F,
	&opcode_0x10, &opcode_0x11, &opcode_0x12, &opcode_0x13, &opcode_0x14, &opcode_0x15, &opcode_0x16, &opcode_0x17,
	&opcode_0x18, &opcode_0x19, &opcode_0x1A, &opcode_0x1B, &opcode_0x1C, &opcode_0x1D, &opcode_0x1E, &opcode_0x1F,
	&opcode_0x20, &opcode_0x21, &opcode_0x22, &opcode_0x23, &opcode_0x24, &opcode_0x25, &opcode_0x26, &opcode_0x27,
	&opcode_0x28, &opcode_0x29, &opcode_0x2A, &opcode_0x2B, &opcode_0x2C, &opcode_0x2D, &opcode_0x2E, &opcode_0x2F,
	&opcode_0x30, &opcode_0x31, &opcode_0x32, &opcode_0x33, &opcode_0x34, &opcode_0x35, &opcode_0x36, &opcode_0x37,
	&opcode_0x38, &opcode_0x39, &opcode_0x3A, &opcode_0x2B, &opcode_0x3C, &opcode_0x3D, &opcode_0x3E, &opcode_0x3F,
	&opcode_0x40, &opcode_0x41, &opcode_0x42, &opcode_0x43, &opcode_0x44, &opcode_0x45, &opcode_0x46, &opcode_0x47,
	&opcode_0x48, &opcode_0x49, &opcode_0x4A, &opcode_0x4B, &opcode_0x4C, &opcode_0x4D, &opcode_0x4E, &opcode_0x4F,
	&opcode_0x50, &opcode_0x51, &opcode_0x52, &opcode_0x53, &opcode_0x54, &opcode_0x55, &opcode_0x56, &opcode_0x57,
	&opcode_0x58, &opcode_0x59, &opcode_0x5A, &opcode_0x5B, &opcode_0x5C, &opcode_0x5D, &opcode_0x5E, &opcode_0x5F,
	&opcode_0x60, &opcode_0x61, &opcode_0x62, &opcode_0x63, &opcode_0x64, &opcode_0x65, &opcode_0x66, &opcode_0x67,
	&opcode_0x68, &opcode_0x69, &opcode_0x6A, &opcode_0x6B, &opcode_0x6C, &opcode_0x6D, &opcode_0x6E, &opcode_0x6F,
	&opcode_0x70, &opcode_0x71, &opcode_0x72, &opcode_0x73, &opcode_0x74, &opcode_0x75, &opcode_0x76, &opcode_0x77,
	&opcode_0x78, &opcode_0x79, &opcode_0x7A, &opcode_0x7B, &opcode_0x7C, &opcode_0x7D, &opcode_0x7E, &opcode_0x7F,
	&opcode_0x80, &opcode_0x81, &opcode_0x82, &opcode_0x83, &opcode_0x84, &opcode_0x85, &opcode_0x86, &opcode_0x87,
	&opcode_0x88, &opcode_0x89, &opcode_0x8A, &opcode_0x8B, &opcode_0x8C, &opcode_0x8D, &opcode_0x8E, &opcode_0x8F,
	&opcode_0x90, &opcode_0x91, &opcode_0x92, &opcode_0x93, &opcode_0x94, &opcode_0x95, &opcode_0x96, &opcode_0x97,
	&opcode_0x98, &opcode_0x99, &opcode_0x9A, &opcode_0x9B, &opcode_0x9C, &opcode_0x9D, &opcode_0x9E, &opcode_0x9F,
	&opcode_0xA0, &opcode_0xA1, &opcode_0xA2, &opcode_0xA3, &opcode_0xA4, &opcode_0xA5, &opcode_0xA6, &opcode_0xA7,
	&opcode_0xA8, &opcode_0xA9, &opcode_0xAA, &opcode_0xAB, &opcode_0xAC, &opcode_0xAD, &opcode_0xAE, &opcode_0xAF,
	&opcode_0xB0, &opcode_0xB1, &opcode_0xB2, &opcode_0xB3, &opcode_0xB4, &opcode_0xB5, &opcode_0xB6, &opcode_0xB7,
	&opcode_0xB8, &opcode_0xB9, &opcode_0xBA, &opcode_0xBB, &opcode_0xBC, &opcode_0xBD, &opcode_0xBE, &opcode_0xBF,
	&opcode_0xC0, &opcode_0xC1, &opcode_0xC2, &opcode_0xC3, &opcode_0xC4, &opcode_0xC5, &opcode_0xC6, &opcode_0xC7,
	&opcode_0xC8, &opcode_0xC9, &opcode_0xCA, &opcode_0xCB, &opcode_0xCC, &opcode_0xCD, &opcode_0xCE, &opcode_0xCF,
	&opcode_0xD0, &opcode_0xD1, &opcode_0xD2, &opcode_0xD3, &opcode_0xD4, &opcode_0xD5, &opcode_0xD6, &opcode_0xD7,
	&opcode_0xD8, &opcode_0xD9, &opcode_0xDA, &opcode_0xDB, &opcode_0xDC, &opcode_0xDD, &opcode_0xDE, &opcode_0xDF,
	&opcode_0xE0, &opcode_0xE1, &opcode_0xE2, &opcode_0xE3, &opcode_0xE4, &opcode_0xE5, &opcode_0xE6, &opcode_0xE7,
	&opcode_0xE8, &opcode_0xE9, &opcode_0xEA, &opcode_0xEB, &opcode_0xEC, &opcode_0xED, &opcode_0xEE, &opcode_0xEF,
	&opcode_0xF0, &opcode_0xF1, &opcode_0xF2, &opcode_0xF3, &opcode_0xF4, &opcode_0xF5, &opcode_0xF6, &opcode_0xF7,
	&opcode_0xF8, &opcode_0xF9, &opcode_0xFA, &opcode_0xFB, &opcode_0xFC, &opcode_0xFD, &opcode_0xFE, &opcode_0xFF,
};

