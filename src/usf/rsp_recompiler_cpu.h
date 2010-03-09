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

extern uint32_t RSPCompilePC, RSPNextInstruction;

//#define CompilerWarning if (ShowErrors) DisplayError
//#define CompilerWarning(blah,blah2)
#define CompilerWarning(...)

#define High16BitAccum		1
#define Middle16BitAccum	2
#define Low16BitAccum		4
#define EntireAccum			(Low16BitAccum|Middle16BitAccum|High16BitAccum)

int32_t WriteToAccum (int32_t Location, int32_t PC);
int32_t WriteToVectorDest (uint32_t DestReg, int32_t PC);
int32_t UseRspFlags (int32_t PC);

int32_t DelaySlotAffectBranch(uint32_t PC);
int32_t CompareInstructions(uint32_t PC, RSPOPCODE * Top, RSPOPCODE * Bottom);
int32_t IsOpcodeBranch(uint32_t PC, RSPOPCODE RspOp);
int32_t IsOpcodeNop(uint32_t PC);

int32_t IsNextInstructionMmx(uint32_t PC);
int32_t IsRegisterConstant (uint32_t Reg, uint32_t * Constant);

void RSP_Element2Mmx(int32_t MmxReg);
void RSP_MultiElement2Mmx(int32_t MmxReg1, int32_t MmxReg2);

#define MainBuffer			0
#define SecondaryBuffer		1

void RunRecompilerCPU ( uint32_t Cycles );
void BuildRecompilerCPU ( void );

void CompilerRSPBlock ( void );
void CompilerToggleBuffer (void);
int32_t RSP_DoSections(void);

typedef struct {
	uint32_t StartPC, CurrPC;		/* block start */

	struct {
		uint32_t TargetPC;			/* Target for this unknown branch */
		uint32_t * X86JumpLoc;		/* Our x86 dword to fill */
	} BranchesToResolve[200];	/* Branches inside or outside block */

	uint32_t ResolveCount;			/* Branches with NULL jump table */
	uint8_t IMEM[0x1000];			/* Saved off for re-order */
} RSP_BLOCK;

extern RSP_BLOCK RSPCurrentBlock;

typedef struct {
	int32_t bIsRegConst[32];		/* BOOLean toggle for constant */
	uint32_t MipsRegConst[32];		/* Value of register 32-bit */
	uint32_t BranchLabels[200];
	uint32_t LabelCount;
	uint32_t BranchLocations[200];
	uint32_t BranchCount;
} RSP_CODE;

extern RSP_CODE RspCode;

#define IsRegConst(i)	(RspCode.bIsRegConst[i])
#define MipsRegConst(i) (RspCode.MipsRegConst[i])

typedef struct {
	int32_t mmx, mmx2, sse;	/* CPU specs and compiling */
	int32_t bFlags;			/* RSP Flag Analysis */
	int32_t bReOrdering;		/* Instruction reordering */
	int32_t bSections;			/* Microcode sections */
	int32_t bDest;				/* Vector destionation toggle */
	int32_t bAccum;			/* Accumulator toggle */
	int32_t bGPRConstants;		/* Analyze GPR constants */
	int32_t bAlignVector;		/* Align known vector loads */
	int32_t bAlignGPR;			/* Align known gpr loads */
} RSP_COMPILER;

extern RSP_COMPILER Compiler;


extern uint8_t * pLastSecondary, * pLastPrimary;
extern uint32_t RSPBlockID;
extern uint32_t dwBuffer;

#define IsMmxEnabled	(Compiler.mmx)
#define IsMmx2Enabled	(Compiler.mmx2)
#define IsSseEnabled	(Compiler.sse)