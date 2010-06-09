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
#ifndef _RECOMPILER_CPU_H_
#define _RECOMPILER_CPU_H_

#include "types.h"

#pragma pack(push,1)

#define MaxCodeBlocks			50000
#define MaxOrigMem				65000

#define NotMapped				0
#define GPR_Mapped				1
#define Temp_Mapped				2
#define Stack_Mapped			3

#define BranchTypeCop1			0
#define BranchTypeRs			1
#define BranchTypeRsRt			2

//Exit Block Methods
#define Normal					0
#define Normal_NoSysCheck		1
#define DoCPU_Action			2
#define COP1_Unuseable			3
#define DoSysCall				4
#define TLBReadMiss				5
#define ExitResetRecompCode		6
#define DoBreak					7

#define STATE_KNOWN_VALUE		1
//#define STATE_UNKNOW_VALUE

#define STATE_X86_MAPPED		2
//#define STATE_CONST

#define STATE_SIGN				4
//#define STATE_ZERO

#define STATE_32BIT				8
//#define STATE_64BIT

#define STATE_UNKNOWN			0

//STATE_MAPPED_64 = 3;
//STATE_MAPPED_32_ZERO = 11
//STATE_MAPPED_32_SIGN = 15
#define STATE_MAPPED_64			(STATE_KNOWN_VALUE | STATE_X86_MAPPED)
#define STATE_MAPPED_32_ZERO	(STATE_KNOWN_VALUE | STATE_X86_MAPPED | STATE_32BIT)
#define STATE_MAPPED_32_SIGN	(STATE_KNOWN_VALUE | STATE_X86_MAPPED | STATE_32BIT | STATE_SIGN)

//STATE_CONST_64 = 1
//STATE_CONST_32 = 13
#define STATE_CONST_64			(STATE_KNOWN_VALUE)
#define STATE_CONST_32			(STATE_KNOWN_VALUE | STATE_32BIT | STATE_SIGN)

#define IsKnown(Reg)			((MipsRegState(Reg) & STATE_KNOWN_VALUE) != 0)
#define IsUnknown(Reg)			(!IsKnown(Reg))

#define IsMapped(Reg)			(IsKnown(Reg) && (MipsRegState(Reg) & STATE_X86_MAPPED) != 0)
#define IsConst(Reg)			(IsKnown(Reg) && !IsMapped(Reg))

#define IsSigned(Reg)			(IsKnown(Reg) && (MipsRegState(Reg) & STATE_SIGN) != 0)
#define IsUnsigned(Reg)			(IsKnown(Reg) && !IsSigned(Reg))

#define Is32Bit(Reg)			(IsKnown(Reg) && (MipsRegState(Reg) & STATE_32BIT) != 0)
#define Is64Bit(Reg)			(IsKnown(Reg) && !Is32Bit(Reg))

#define Is32BitMapped(Reg)		(Is32Bit(Reg) && (MipsRegState(Reg) & STATE_X86_MAPPED) != 0)
#define Is64BitMapped(Reg)		(Is64Bit(Reg) && !Is32BitMapped(Reg))

#define MipsRegState(Reg)		Section->RegWorking.MIPS_RegState[Reg]
#define MipsReg(Reg)			Section->RegWorking.MIPS_RegVal[Reg].UDW
#define MipsReg_S(Reg)			Section->RegWorking.MIPS_RegVal[Reg].DW
#define MipsRegLo(Reg)			Section->RegWorking.MIPS_RegVal[Reg].UW[0]
#define MipsRegLo_S(Reg)		Section->RegWorking.MIPS_RegVal[Reg].W[0]
#define MipsRegHi(Reg)			Section->RegWorking.MIPS_RegVal[Reg].UW[1]
#define MipsRegHi_S(Reg)		Section->RegWorking.MIPS_RegVal[Reg].W[1]

#define x86MapOrder(Reg)		Section->RegWorking.x86reg_MapOrder[Reg]
#define x86Protected(Reg)		Section->RegWorking.x86reg_Protected[Reg]
#define x86Mapped(Reg)			Section->RegWorking.x86reg_MappedTo[Reg]

#define BlockCycleCount			Section->RegWorking.CycleCount
#define BlockRandomModifier		Section->RegWorking.RandomModifier


#define StackTopPos				Section->RegWorking.Stack_TopPos
#define FpuMappedTo(Reg)		Section->RegWorking.x86fpu_MappedTo[Reg]
#define FpuState(Reg)			Section->RegWorking.x86fpu_State[Reg]
#define FpuRoundingModel(Reg)	Section->RegWorking.x86fpu_RoundingModel[Reg]
#define FpuBeenUsed				Section->RegWorking.Fpu_Used
#define CurrentRoundingModel	Section->RegWorking.RoundingModel

typedef struct {
	//r4k
	int32_t		    MIPS_RegState[32];
	MIPS_DWORD		MIPS_RegVal[32];

	uint32_t		x86reg_MappedTo[64];
	uint32_t		x86reg_MapOrder[64];
	uint32_t		x86reg_Protected[64];

	uint32_t		CycleCount;
	uint32_t		RandomModifier;

	//FPU
	uint32_t		Stack_TopPos;
	uint32_t		x86fpu_MappedTo[16];
	uint32_t		x86fpu_State[16];
	uint32_t		x86fpu_RoundingModel[16];

	uint32_t        Fpu_Used;
	uint32_t       RoundingModel;
} REG_INFO;

typedef struct {
	uint32_t		TargetPC;
	//uint8_t *		BranchLabel;
	uint8_t *		LinkLocation;
	uint8_t *		LinkLocation2;
	uint32_t		FallThrough;
	uint32_t		PermLoop;
	uint32_t		DoneDelaySlot;
	REG_INFO	RegSet;
} JUMP_INFO;

typedef struct {
	/* Block Connection info */
	void **		ParentSection;
	void *		ContinueSection;
	void *		JumpSection;
	uint8_t *		CompiledLocation;


	uint32_t		SectionID;
	uint32_t		Test;
	uint32_t		Test2;
	uint32_t		InLoop;

	uint32_t		StartPC;
	uint32_t		CompilePC;

	/* Register Info */
	REG_INFO	RegStart;
	REG_INFO	RegWorking;

	/* Jump Info */
	JUMP_INFO   Jump;
	JUMP_INFO   Cont;
} BLOCK_SECTION;

typedef struct {
	BLOCK_SECTION * Parent;
	JUMP_INFO     * JumpInfo;
} BLOCK_PARENT;

typedef struct {
	uint32_t    TargetPC;
	REG_INFO ExitRegSet;
	int32_t      reason;
	int32_t      NextInstruction;
	uint8_t *   JumpLoc; //32bit jump
} EXIT_INFO;

typedef struct {
	uint32_t	 	  StartVAddr;
	uint8_t *		  CompiledLocation;
	int32_t           NoOfSections;
	BLOCK_SECTION BlockInfo;
	EXIT_INFO  ** ExitInfo;
	int32_t           ExitCount;
} BLOCK_INFO;

typedef struct {
	void * CodeBlock;
	QWORD  OriginalMemory;
} TARGET_INFO;

typedef struct {
	uint32_t PAddr;
	uint32_t VAddr;
	uint32_t OriginalValue;
	void * CompiledLocation;
} ORIGINAL_MEMMARKER;

typedef struct {
	uint32_t NoOfRDRamBlocks[2048];
	uint32_t NoOfDMEMBlocks;
	uint32_t NoOfIMEMBlocks;
	uint32_t NoOfPifRomBlocks;
} N64_Blocks_t;

extern N64_Blocks_t N64_Blocks;

#pragma pack(pop)

uint8_t *Compiler4300iBlock    ( void );
uint8_t *CompileDelaySlot      ( void );
void CompileExit            ( uint32_t TargetPC, REG_INFO * ExitRegSet, int32_t reason, int32_t CompileNow, void (*x86Jmp)(uintptr_t Value));
void CompileSystemCheck     ( uint32_t TimerModifier, uint32_t TargetPC, REG_INFO RegSet );
void FixRandomReg           ( void );
void FreeSection            ( BLOCK_SECTION * Section, BLOCK_SECTION * Parent);
void StartRecompilerCPU     ( void );
void GenerateSectionLinkage ( BLOCK_SECTION * Section );
void InitilizeInitialCompilerVariable ( void);
int UnMap_TempRegSet (REG_INFO * RegWorking);

extern uint32_t * TLBLoadAddress, TargetIndex;
extern TARGET_INFO * TargetInfo;
extern uint16_t FPU_RoundingMode;

#define SetJump32(Loc,JumpLoc) *(uint32_t *)(Loc)= (uint32_t)(((uint32_t)(JumpLoc)) - (((uint32_t)(Loc)) + 4));
#define SetJump8(Loc,JumpLoc)  *(uint8_t  *)(Loc)= (uint8_t )(((uint8_t )(JumpLoc)) - (((uint8_t )(Loc)) + 1));

#endif
