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
#include <stdlib.h>
#include <stdio.h>
#include "main.h"
#include "cpu.h"
#include "x86.h"
#include "usf.h"
#include "types.h"


void  CreateSectionLinkage (BLOCK_SECTION * Section);
void  DetermineLoop(BLOCK_SECTION * Section, uint32_t Test, uint32_t Test2, uint32_t TestID);
uint32_t DisplaySectionInformation (BLOCK_SECTION * Section, uint32_t ID, uint32_t Test);
BLOCK_SECTION * ExistingSection(BLOCK_SECTION * StartSection, uint32_t Addr, uint32_t Test);
void  FillSectionInfo(BLOCK_SECTION * Section);
void  FixConstants ( BLOCK_SECTION * Section, uint32_t Test,int * Changed );
uint32_t GenerateX86Code (BLOCK_SECTION * Section, uint32_t Test );
uint32_t GetNewTestValue( void );
void  InheritConstants(BLOCK_SECTION * Section);
uint32_t InheritParentInfo (BLOCK_SECTION * Section);
void  InitilzeSection(BLOCK_SECTION * Section, BLOCK_SECTION * Parent, uint32_t StartAddr, uint32_t ID);
void InitilizeRegSet(REG_INFO * RegSet);
uint32_t IsAllParentLoops(BLOCK_SECTION * Section, BLOCK_SECTION * Parent, uint32_t IgnoreIfCompiled, uint32_t Test);
void MarkCodeBlock (uint32_t PAddr);
void SyncRegState (BLOCK_SECTION * Section, REG_INFO * SyncTo);

N64_Blocks_t N64_Blocks;
uint32_t * TLBLoadAddress = 0, TargetIndex;
TARGET_INFO * TargetInfo = NULL;
BLOCK_INFO BlockInfo;

void InitilizeInitialCompilerVariable ( void)
{
	memset(&BlockInfo,0,sizeof(BlockInfo));
}

void  AddParent(BLOCK_SECTION * Section, BLOCK_SECTION * Parent){
	int NoOfParents, count;


	if (Section == NULL) { return; }
	if (Parent == NULL) {
		InitilizeRegSet(&Section->RegStart);
		memcpy(&Section->RegWorking,&Section->RegStart,sizeof(REG_INFO));
		return;
	}

	if (Section->ParentSection != NULL) {
		for (NoOfParents = 0;Section->ParentSection[NoOfParents] != NULL;NoOfParents++) {
			if (Section->ParentSection[NoOfParents] == Parent) {
				return;
			}
		}
		for (NoOfParents = 0;Section->ParentSection[NoOfParents] != NULL;NoOfParents++);
		NoOfParents += 1;
	} else {
		NoOfParents = 1;
	}

	if (NoOfParents == 1) {
		Section->ParentSection = malloc((NoOfParents + 1)*sizeof(void *));
	} else {
		Section->ParentSection = realloc(Section->ParentSection,(NoOfParents + 1)*sizeof(void *));
	}
	Section->ParentSection[NoOfParents - 1] = Parent;
	Section->ParentSection[NoOfParents] = NULL;

	if (NoOfParents == 1) {
		if (Parent->ContinueSection == Section) {
			memcpy(&Section->RegStart,&Parent->Cont.RegSet,sizeof(REG_INFO));
		} else if (Parent->JumpSection == Section) {
			memcpy(&Section->RegStart,&Parent->Jump.RegSet,sizeof(REG_INFO));
		} else {
		}
		memcpy(&Section->RegWorking,&Section->RegStart,sizeof(REG_INFO));
	} else {
		if (Parent->ContinueSection == Section) {
			for (count = 0; count < 32; count++) {
				if (Section->RegStart.MIPS_RegState[count] != Parent->Cont.RegSet.MIPS_RegState[count]) {
					Section->RegStart.MIPS_RegState[count] = STATE_UNKNOWN;
				}
			}
		}
		if (Parent->JumpSection == Section) {
			for (count = 0; count < 32; count++) {
				if (Section->RegStart.MIPS_RegState[count] != Parent->Jump.RegSet.MIPS_RegState[count]) {
					Section->RegStart.MIPS_RegState[count] = STATE_UNKNOWN;
				}
			}
		}
		memcpy(&Section->RegWorking,&Section->RegStart,sizeof(REG_INFO));
	}
}

void AnalyseBlock (void) {
	int Changed;

	BLOCK_SECTION * Section = &BlockInfo.BlockInfo;

	BlockInfo.NoOfSections = 1;
	InitilzeSection (Section, NULL, BlockInfo.StartVAddr, BlockInfo.NoOfSections);
	CreateSectionLinkage (Section);
	DetermineLoop(Section,GetNewTestValue(),GetNewTestValue(), Section->SectionID);
	do {
		Changed = 0;
		FixConstants(Section,GetNewTestValue(),&Changed);
	} while (Changed == 1);

}

int ConstantsType (int64_t Value) {
	if (((Value >> 32) == -1) && ((Value & 0x80000000) != 0)) { return STATE_CONST_32; }
	if (((Value >> 32) == 0) && ((Value & 0x80000000) == 0)) { return STATE_CONST_32; }
	return STATE_CONST_64;
}


uint8_t * Compiler4300iBlock(void) {
	uintptr_t StartAddress;
	int count;

	//reset BlockInfo
	if (BlockInfo.ExitInfo)
	{
		for (count = 0; count < BlockInfo.ExitCount; count ++) {
			free(BlockInfo.ExitInfo[count]);
			BlockInfo.ExitInfo[count] = NULL;
		}
		if (BlockInfo.ExitInfo) { free(BlockInfo.ExitInfo); }
		BlockInfo.ExitInfo = NULL;
	}

	memset(&BlockInfo,0,sizeof(BlockInfo));
	BlockInfo.CompiledLocation = RecompPos;
	BlockInfo.StartVAddr = PROGRAM_COUNTER;

	AnalyseBlock();

	StartAddress = BlockInfo.StartVAddr;
	TranslateVaddr(&StartAddress);

	//BreakPoint();
	//MoveConstQwordToX86reg(Registers, x64_R14);

	MarkCodeBlock(StartAddress);

	while (GenerateX86Code(&BlockInfo.BlockInfo,GetNewTestValue()));
	for (count = 0; count < BlockInfo.ExitCount; count ++) {
		SetJump32(BlockInfo.ExitInfo[count]->JumpLoc,RecompPos);
		NextInstruction = BlockInfo.ExitInfo[count]->NextInstruction;
		CompileExit(BlockInfo.ExitInfo[count]->TargetPC,&BlockInfo.ExitInfo[count]->ExitRegSet,
			BlockInfo.ExitInfo[count]->reason,1,NULL);
	}
	FreeSection (BlockInfo.BlockInfo.ContinueSection,&BlockInfo.BlockInfo);
	FreeSection (BlockInfo.BlockInfo.JumpSection,&BlockInfo.BlockInfo);
	for (count = 0; count < BlockInfo.ExitCount; count ++) {
		free(BlockInfo.ExitInfo[count]);
		BlockInfo.ExitInfo[count] = NULL;
	}
	if (BlockInfo.ExitInfo) { free(BlockInfo.ExitInfo); }
	BlockInfo.ExitInfo = NULL;
	BlockInfo.ExitCount = 0;

	return BlockInfo.CompiledLocation;
}


uint8_t * CompileDelaySlot(void) {
	uintptr_t StartAddress = PROGRAM_COUNTER;
	BLOCK_SECTION *Section, DelaySection;
	uint8_t * Block = RecompPos;
	int count, x86Reg;

	Section = &DelaySection;

	if ((StartAddress & 0xFFC) != 0) {
		StopEmulation();
	}
	if (!r4300i_LW_VAddr(StartAddress, &Opcode.Hex)) {
		StopEmulation();
	}

	TranslateVaddr(&StartAddress);

	MarkCodeBlock(StartAddress);

	InitilzeSection (Section, NULL, PROGRAM_COUNTER, 0);
	InitilizeRegSet(&Section->RegStart);
	memcpy(&Section->RegWorking,&Section->RegStart,sizeof(REG_INFO));

	BlockCycleCount += 2;
	BlockRandomModifier += 1;

	switch (Opcode.op) {
	case R4300i_SPECIAL:
		switch (Opcode.funct) {
		case R4300i_SPECIAL_BREAK: Compile_R4300i_SPECIAL_BREAK(Section); break;
		case R4300i_SPECIAL_SLL: Compile_R4300i_SPECIAL_SLL(Section); break;
		case R4300i_SPECIAL_SRL: Compile_R4300i_SPECIAL_SRL(Section); break;
		case R4300i_SPECIAL_SRA: Compile_R4300i_SPECIAL_SRA(Section); break;
		case R4300i_SPECIAL_SLLV: Compile_R4300i_SPECIAL_SLLV(Section); break;
		case R4300i_SPECIAL_SRLV: Compile_R4300i_SPECIAL_SRLV(Section); break;
		case R4300i_SPECIAL_SRAV: Compile_R4300i_SPECIAL_SRAV(Section); break;
		case R4300i_SPECIAL_MFLO: Compile_R4300i_SPECIAL_MFLO(Section); break;
		case R4300i_SPECIAL_MTLO: Compile_R4300i_SPECIAL_MTLO(Section); break;
		case R4300i_SPECIAL_MFHI: Compile_R4300i_SPECIAL_MFHI(Section); break;
		case R4300i_SPECIAL_MTHI: Compile_R4300i_SPECIAL_MTHI(Section); break;
		case R4300i_SPECIAL_MULT: Compile_R4300i_SPECIAL_MULT(Section); break;
		case R4300i_SPECIAL_DIV: Compile_R4300i_SPECIAL_DIV(Section); break;
		case R4300i_SPECIAL_DIVU: Compile_R4300i_SPECIAL_DIVU(Section); break;
		case R4300i_SPECIAL_MULTU: Compile_R4300i_SPECIAL_MULTU(Section); break;
		case R4300i_SPECIAL_DMULTU: Compile_R4300i_SPECIAL_DMULTU(Section); break;
		case R4300i_SPECIAL_DDIVU: Compile_R4300i_SPECIAL_DDIVU(Section); break;
		case R4300i_SPECIAL_ADD: Compile_R4300i_SPECIAL_ADD(Section); break;
		case R4300i_SPECIAL_ADDU: Compile_R4300i_SPECIAL_ADDU(Section); break;
		case R4300i_SPECIAL_SUB: Compile_R4300i_SPECIAL_SUB(Section); break;
		case R4300i_SPECIAL_SUBU: Compile_R4300i_SPECIAL_SUBU(Section); break;
		case R4300i_SPECIAL_AND: Compile_R4300i_SPECIAL_AND(Section); break;
		case R4300i_SPECIAL_OR: Compile_R4300i_SPECIAL_OR(Section); break;
		case R4300i_SPECIAL_XOR: Compile_R4300i_SPECIAL_XOR(Section); break;
		case R4300i_SPECIAL_SLT: Compile_R4300i_SPECIAL_SLT(Section); break;
		case R4300i_SPECIAL_SLTU: Compile_R4300i_SPECIAL_SLTU(Section); break;
		case R4300i_SPECIAL_DADD: Compile_R4300i_SPECIAL_DADD(Section); break;
		case R4300i_SPECIAL_DADDU: Compile_R4300i_SPECIAL_DADDU(Section); break;
		case R4300i_SPECIAL_DSLL32: Compile_R4300i_SPECIAL_DSLL32(Section); break;
		case R4300i_SPECIAL_DSRA32: Compile_R4300i_SPECIAL_DSRA32(Section); break;
		default:
			Compile_R4300i_UnknownOpcode(Section); break;
		}
		break;
	case R4300i_ADDI: Compile_R4300i_ADDI(Section); break;
	case R4300i_ADDIU: Compile_R4300i_ADDIU(Section); break;
	case R4300i_SLTI: Compile_R4300i_SLTI(Section); break;
	case R4300i_SLTIU: Compile_R4300i_SLTIU(Section); break;
	case R4300i_ANDI: Compile_R4300i_ANDI(Section); break;
	case R4300i_ORI: Compile_R4300i_ORI(Section); break;
	case R4300i_XORI: Compile_R4300i_XORI(Section); break;
	case R4300i_LUI: Compile_R4300i_LUI(Section); break;
	case R4300i_CP1:
		switch (Opcode.rs) {
		case R4300i_COP1_CF: Compile_R4300i_COP1_CF(Section); break;
		case R4300i_COP1_MT: Compile_R4300i_COP1_MT(Section); break;
		case R4300i_COP1_CT: Compile_R4300i_COP1_CT(Section); break;
		case R4300i_COP1_MF: Compile_R4300i_COP1_MF(Section); break;
		case R4300i_COP1_S:
			switch (Opcode.funct) {
			case R4300i_COP1_FUNCT_ADD: Compile_R4300i_COP1_S_ADD(Section); break;
			case R4300i_COP1_FUNCT_SUB: Compile_R4300i_COP1_S_SUB(Section); break;
			case R4300i_COP1_FUNCT_MUL: Compile_R4300i_COP1_S_MUL(Section); break;
			case R4300i_COP1_FUNCT_DIV: Compile_R4300i_COP1_S_DIV(Section); break;
			case R4300i_COP1_FUNCT_ABS: Compile_R4300i_COP1_S_ABS(Section); break;
			case R4300i_COP1_FUNCT_NEG: Compile_R4300i_COP1_S_NEG(Section); break;
			case R4300i_COP1_FUNCT_SQRT: Compile_R4300i_COP1_S_SQRT(Section); break;
			case R4300i_COP1_FUNCT_MOV: Compile_R4300i_COP1_S_MOV(Section); break;
			case R4300i_COP1_FUNCT_CVT_D: Compile_R4300i_COP1_S_CVT_D(Section); break;
			case R4300i_COP1_FUNCT_ROUND_W: Compile_R4300i_COP1_S_ROUND_W(Section); break;
			case R4300i_COP1_FUNCT_TRUNC_W: Compile_R4300i_COP1_S_TRUNC_W(Section); break;
			case R4300i_COP1_FUNCT_FLOOR_W: Compile_R4300i_COP1_S_FLOOR_W(Section); break;
			case R4300i_COP1_FUNCT_C_F:   case R4300i_COP1_FUNCT_C_UN:
			case R4300i_COP1_FUNCT_C_EQ:  case R4300i_COP1_FUNCT_C_UEQ:
			case R4300i_COP1_FUNCT_C_OLT: case R4300i_COP1_FUNCT_C_ULT:
			case R4300i_COP1_FUNCT_C_OLE: case R4300i_COP1_FUNCT_C_ULE:
			case R4300i_COP1_FUNCT_C_SF:  case R4300i_COP1_FUNCT_C_NGLE:
			case R4300i_COP1_FUNCT_C_SEQ: case R4300i_COP1_FUNCT_C_NGL:
			case R4300i_COP1_FUNCT_C_LT:  case R4300i_COP1_FUNCT_C_NGE:
			case R4300i_COP1_FUNCT_C_LE:  case R4300i_COP1_FUNCT_C_NGT:
				Compile_R4300i_COP1_S_CMP(Section); break;
			default:
				Compile_R4300i_UnknownOpcode(Section); break;
			}
			break;
		case R4300i_COP1_D:
			switch (Opcode.funct) {
			case R4300i_COP1_FUNCT_ADD: Compile_R4300i_COP1_D_ADD(Section); break;
			case R4300i_COP1_FUNCT_SUB: Compile_R4300i_COP1_D_SUB(Section); break;
			case R4300i_COP1_FUNCT_MUL: Compile_R4300i_COP1_D_MUL(Section); break;
			case R4300i_COP1_FUNCT_DIV: Compile_R4300i_COP1_D_DIV(Section); break;
			case R4300i_COP1_FUNCT_ABS: Compile_R4300i_COP1_D_ABS(Section); break;
			case R4300i_COP1_FUNCT_NEG: Compile_R4300i_COP1_D_NEG(Section); break;
			case R4300i_COP1_FUNCT_SQRT: Compile_R4300i_COP1_D_SQRT(Section); break;
			case R4300i_COP1_FUNCT_MOV: Compile_R4300i_COP1_D_MOV(Section); break;
			case R4300i_COP1_FUNCT_TRUNC_W: Compile_R4300i_COP1_D_TRUNC_W(Section); break;
			case R4300i_COP1_FUNCT_CVT_S: Compile_R4300i_COP1_D_CVT_S(Section); break;
			case R4300i_COP1_FUNCT_CVT_W: Compile_R4300i_COP1_D_CVT_W(Section); break;
			case R4300i_COP1_FUNCT_C_F:   case R4300i_COP1_FUNCT_C_UN:
			case R4300i_COP1_FUNCT_C_EQ:  case R4300i_COP1_FUNCT_C_UEQ:
			case R4300i_COP1_FUNCT_C_OLT: case R4300i_COP1_FUNCT_C_ULT:
			case R4300i_COP1_FUNCT_C_OLE: case R4300i_COP1_FUNCT_C_ULE:
			case R4300i_COP1_FUNCT_C_SF:  case R4300i_COP1_FUNCT_C_NGLE:
			case R4300i_COP1_FUNCT_C_SEQ: case R4300i_COP1_FUNCT_C_NGL:
			case R4300i_COP1_FUNCT_C_LT:  case R4300i_COP1_FUNCT_C_NGE:
			case R4300i_COP1_FUNCT_C_LE:  case R4300i_COP1_FUNCT_C_NGT:
				Compile_R4300i_COP1_D_CMP(Section); break;
			default:
				Compile_R4300i_UnknownOpcode(Section); break;
			}
			break;
		case R4300i_COP1_W:
			switch (Opcode.funct) {
			case R4300i_COP1_FUNCT_CVT_S: Compile_R4300i_COP1_W_CVT_S(Section); break;
			case R4300i_COP1_FUNCT_CVT_D: Compile_R4300i_COP1_W_CVT_D(Section); break;
			default:
				Compile_R4300i_UnknownOpcode(Section); break;
			}
			break;
		default:
			Compile_R4300i_UnknownOpcode(Section); break;
		}
		break;
	case R4300i_LB: Compile_R4300i_LB(Section); break;
	case R4300i_LH: Compile_R4300i_LH(Section); break;
	case R4300i_LW: Compile_R4300i_LW(Section); break;
	case R4300i_LBU: Compile_R4300i_LBU(Section); break;
	case R4300i_LHU: Compile_R4300i_LHU(Section); break;
	case R4300i_SB: Compile_R4300i_SB(Section); break;
	case R4300i_SH: Compile_R4300i_SH(Section); break;
	case R4300i_SW: Compile_R4300i_SW(Section); break;
	case R4300i_SWR: Compile_R4300i_SWR(Section); break;
	case R4300i_CACHE: Compile_R4300i_CACHE(Section); break;
	case R4300i_LWC1: Compile_R4300i_LWC1(Section); break;
	case R4300i_LDC1: Compile_R4300i_LDC1(Section); break;
	case R4300i_LD: Compile_R4300i_LD(Section); break;
	case R4300i_SWC1: Compile_R4300i_SWC1(Section); break;
	case R4300i_SDC1: Compile_R4300i_SDC1(Section); break;
	case R4300i_SD: Compile_R4300i_SD(Section); break;
	default:
		Compile_R4300i_UnknownOpcode(Section); break;
	}

	for (count = 1; count < 64; count ++) { x86Protected(count) = 0; }

	WriteBackRegisters(Section);
	if (BlockCycleCount != 0) {
		AddConstToVariable(BlockCycleCount,&CP0[9]);
		SubConstFromVariable(BlockCycleCount,&Timers->Timer);
	}
	if (BlockRandomModifier != 0) { SubConstFromVariable(BlockRandomModifier,&CP0[1]); }
	x86Reg = Map_TempReg(Section,x86_Any,-1,0);
	MoveVariableToX86reg(&JumpToLocation,x86Reg);
	MoveX86regToVariable(x86Reg,&PROGRAM_COUNTER);
	MoveConstToVariable(NORMAL,&NextInstruction);
	Ret();
	return Block;
}

void CompileExit (uint32_t TargetPC, REG_INFO * ExitRegSet, int reason, int CompileNow, void (*x86Jmp)(uintptr_t Value)) {
	BLOCK_SECTION Section;
	uint8_t * Jump, * Jump2;

	if (!CompileNow) {
		if (BlockInfo.ExitCount == 0) {
			BlockInfo.ExitInfo = malloc(sizeof(void *));
		} else {
			BlockInfo.ExitInfo = realloc(BlockInfo.ExitInfo,(BlockInfo.ExitCount + 1) * sizeof(void *));
		}
		if (x86Jmp == NULL) {
			StopEmulation();
		}
		x86Jmp(0);
		BlockInfo.ExitInfo[BlockInfo.ExitCount] = malloc(sizeof(EXIT_INFO));
		BlockInfo.ExitInfo[BlockInfo.ExitCount]->TargetPC = TargetPC;
		BlockInfo.ExitInfo[BlockInfo.ExitCount]->ExitRegSet = *ExitRegSet;
		BlockInfo.ExitInfo[BlockInfo.ExitCount]->reason = reason;
		BlockInfo.ExitInfo[BlockInfo.ExitCount]->NextInstruction = NextInstruction;
		BlockInfo.ExitInfo[BlockInfo.ExitCount]->JumpLoc = RecompPos - 4;
		BlockInfo.ExitCount += 1;
		return;
	}

	InitilzeSection (&Section, NULL, (uint32_t)-1, 0);
	memcpy(&Section.RegWorking, ExitRegSet, sizeof(REG_INFO));


	//if(TargetPC > 0x90000000)
		//BreakPoint();

	if (TargetPC != (uint32_t)-1) { MoveConstToVariable(TargetPC,&PROGRAM_COUNTER); }
	if (ExitRegSet->CycleCount != 0) {
		AddConstToVariable(ExitRegSet->CycleCount,&CP0[9]);
		SubConstFromVariable(ExitRegSet->CycleCount,&Timers->Timer);
	}
	if (ExitRegSet->RandomModifier != 0) { SubConstFromVariable(ExitRegSet->RandomModifier,&CP0[1]); }
	WriteBackRegisters(&Section);

	switch (reason) {
	case Normal: case Normal_NoSysCheck:
		Section.RegWorking.RandomModifier = 0;
		Section.RegWorking.CycleCount = 0;
		if (reason == Normal) { CompileSystemCheck(0,(uint32_t)-1,Section.RegWorking);	}

			if (TargetPC >= 0x80000000 && TargetPC < 0x90000000) {
				uint32_t pAddr = TargetPC & 0x1FFFFFFF;
				MoveConstToX86reg(pAddr,x86_ECX);
				ShiftRightDoubleImmed(x86_ECX,x86_ECX,2);
#ifdef USEX64
				MoveVariableDispToX86Reg(JumpTable,x86_RCX,x86_RCX,8);
#else
				MoveVariableDispToX86Reg(JumpTable,x86_ECX,x86_ECX,4);
#endif
				Jump2 = NULL;
			} else if (TargetPC >= 0x90000000 && TargetPC < 0xC0000000) {
			} else {
#ifdef USEX64
				MoveConstToX86reg((TargetPC >> 12),x86_RCX);
				MoveConstToX86reg(TargetPC,x86_EBX);
				//MoveVariableDispToX86Reg(TLB_Map,x86_RCX,x86_RCX,8);
				MoveX86RegDispToX86Reg(x86_RCX, x86_R15, x86_RCX, 8);
				TestX86RegToX86Reg(x86_RCX,x86_RCX);
				JeLabel8(0);
				Jump2 = RecompPos - 1;
				AddX86RegToX86Reg(x86_RCX,x86_EBX);
				MoveConstQwordToX86reg((uintptr_t)N64MEM, x86_RBX);
				SubX86RegToX86Reg(x86_RCX,x86_RBX);
				ShiftRightDoubleImmed(x86_RCX,x86_RCX,2);
				ShiftLeftDoubleImmed(x86_RCX,x86_RCX,3);
				MoveConstQwordToX86reg((uintptr_t)JumpTable, x86_RBX);
				AddX86RegToX86Reg(x86_RCX,x86_RBX);
				MoveX86PointerToX86reg(x86_RCX,x86_RCX);
#else
				MoveConstToX86reg((TargetPC >> 12),x86_ECX);
				MoveConstToX86reg(TargetPC,x86_EBX);
				MoveVariableDispToX86Reg(TLB_Map,x86_ECX,x86_ECX,4);
				TestX86RegToX86Reg(x86_ECX,x86_ECX);
				JeLabel8(0);
				Jump2 = RecompPos - 1;
				AddConstToX86Reg(x86_ECX,(int32_t)JumpTable - (int32_t)N64MEM);
				MoveX86regPointerToX86reg(x86_ECX, x86_EBX,x86_ECX);
#endif
			}
			if (TargetPC < 0x90000000 || TargetPC >= 0xC0000000)
			{
					JecxzLabel8(0);
				Jump = RecompPos - 1;
				//	printf("TargetPC %08x\n", TargetPC);


				JmpDirectReg(x86_ECX);
				*((uint8_t *)(Jump))=(uint8_t)(RecompPos - Jump - 1);
				if (Jump2 != NULL) {
					*((uint8_t *)(Jump2))=(uint8_t)(RecompPos - Jump2 - 1);
				}
			}

		Ret();
		break;
	case DoCPU_Action:
		Pushad();
		//BreakPoint();
		Call_Direct(DoSomething);
		//RecompileDoSomething();
		Popad();
		Ret();
		break;
	case DoBreak:
		Ret();
		break;
	case DoSysCall:
		MoveConstToX86reg(NextInstruction == JUMP || NextInstruction == DELAY_SLOT,x86_EDI);
		printf("extinjg 1\n");
		exit(0);
		Pushad();
		Call_Direct(DoSysCallException);
		Popad();
		Ret();
		break;
	case COP1_Unuseable:
		Pushad();
		MoveConstToX86reg(NextInstruction == JUMP || NextInstruction == DELAY_SLOT,x86_EDI);
		MoveConstToX86reg(1,x86_ESI);
		Call_Direct(DoCopUnusableException);
		Popad();
		Ret();
		break;
	case ExitResetRecompCode:
		if (NextInstruction == JUMP || NextInstruction == DELAY_SLOT) {
			BreakPoint();
		}
		Pushad();
		BreakPoint();
		Call_Direct(ResetRecompCode);
		Popad();
		Ret();
		break;
	case TLBReadMiss:
		MoveConstToX86reg(NextInstruction == JUMP || NextInstruction == DELAY_SLOT,x86_EDI);
		MoveVariableToX86reg(TLBLoadAddress,x86_ESI);
		Pushad();
#ifdef USEX64
		Call_Direct(DoTLBMiss);
#else
		Push(x86_EDX);
		Push(x86_ECX);
		Call_Direct(DoTLBMiss);
		AddConstToX86Reg(x86_ESP, 0x8);
#endif
		Popad();
		Ret();
		break;
	}
}

void CompileSystemCheck (uint32_t TimerModifier, uint32_t TargetPC, REG_INFO RegSet) {
	BLOCK_SECTION Section;
	uint8_t *Jump, *Jump2;

	CompConstToVariable(0,WaitMode);
	JeLabel8(0);
	Jump = RecompPos - 1;
	MoveConstToVariable(-1, &Timers->Timer);

    SetJump8(Jump, RecompPos);

	// Timer
	if (TimerModifier != 0) {
		SubConstFromVariable(TimerModifier,&Timers->Timer);
	} else {
		CompConstToVariable(0,&Timers->Timer);
	}
	JnsLabel32(0);
	Jump = RecompPos - 4;
	Pushad();
	if (TargetPC != (uint32_t)-1) { MoveConstToVariable(TargetPC,&PROGRAM_COUNTER); }
	InitilzeSection (&Section, NULL, (uint32_t)-1, 0);
	memcpy(&Section.RegWorking, &RegSet, sizeof(REG_INFO));
	WriteBackRegisters(&Section);
	Call_Direct(TimerDone);
	//Call_Direct(RecompileTimerDone);
	//RecompileTimerDone();
	//RecompileTimerDone();

	Popad();

	//Interrupt
	CompConstToVariable(0,&CPU_Action->DoSomething);
	JeLabel32(0);
	Jump2 = RecompPos - 4;
	CompileExit(-1,&Section.RegWorking,DoCPU_Action,1,NULL);

	SetJump32(Jump2,RecompPos);
	Ret();

	SetJump32(Jump,RecompPos);

	//Interrupt 2
	CompConstToVariable(0,&CPU_Action->DoSomething);
	JeLabel32(0);
	Jump = RecompPos - 4;
	if (TargetPC != (uint32_t)-1) { MoveConstToVariable(TargetPC,&PROGRAM_COUNTER); }
	InitilzeSection (&Section, NULL, (uint32_t)-1, 0);
	memcpy(&Section.RegWorking, &RegSet, sizeof(REG_INFO));
	WriteBackRegisters(&Section);
	CompileExit(-1,&Section.RegWorking,DoCPU_Action,1,NULL);
	SetJump32(Jump,RecompPos);
	return;
}

void  CreateSectionLinkage (BLOCK_SECTION * Section) {
	BLOCK_SECTION ** TargetSection[2];
	uint32_t * TargetPC[2], count;

	InheritConstants(Section);
	//__try {
		FillSectionInfo(Section);
/*	} __except( r4300i_CPU_MemoryFilter( GetExceptionCode(), GetExceptionInformation()) ) {
		DisplayError(GS(MSG_UNKNOWN_MEM_ACTION));
		ExitThread(0);
	}*/

	if (Section->Jump.TargetPC < Section->Cont.TargetPC) {
		TargetSection[0] = (BLOCK_SECTION **)&Section->JumpSection;
		TargetSection[1] = (BLOCK_SECTION **)&Section->ContinueSection;
		TargetPC[0] = &Section->Jump.TargetPC;
		TargetPC[1] = &Section->Cont.TargetPC;
	} else {
		TargetSection[0] = (BLOCK_SECTION **)&Section->ContinueSection;
		TargetSection[1] = (BLOCK_SECTION **)&Section->JumpSection;
		TargetPC[0] = &Section->Cont.TargetPC;
		TargetPC[1] = &Section->Jump.TargetPC;
	}

	for (count = 0; count < 2; count ++) {
		if (*TargetPC[count] != (uint32_t)-1 && *TargetSection[count] == NULL) {
			*TargetSection[count] = ExistingSection(BlockInfo.BlockInfo.ContinueSection,*TargetPC[count],GetNewTestValue());
			if (*TargetSection[count] == NULL) {
				*TargetSection[count] = ExistingSection(BlockInfo.BlockInfo.JumpSection,*TargetPC[count],GetNewTestValue());
			}
			if (*TargetSection[count] == NULL) {
				BlockInfo.NoOfSections += 1;
				*TargetSection[count] = malloc(sizeof(BLOCK_SECTION));
				InitilzeSection (*TargetSection[count], Section, *TargetPC[count], BlockInfo.NoOfSections);
				CreateSectionLinkage(*TargetSection[count]);
			} else {
				AddParent(*TargetSection[count],Section);
			}
		}
	}
}

void  DetermineLoop(BLOCK_SECTION * Section, uint32_t Test, uint32_t Test2, uint32_t TestID) {
	if (Section == NULL) { return; }
	if (Section->SectionID != TestID) {
		if (Section->Test2 == Test2) {
			return;
		}
		Section->Test2 = Test2;
		DetermineLoop(Section->ContinueSection,Test,Test2,TestID);
		DetermineLoop(Section->JumpSection,Test,Test2,TestID);
		return;
	}
	if (Section->Test2 == Test2) {
		Section->InLoop = 1;
		return;
	}
	Section->Test2 = Test2;
	DetermineLoop(Section->ContinueSection,Test,Test2,TestID);
	DetermineLoop(Section->JumpSection,Test,Test2,TestID);
	if (Section->Test == Test) { return; }
	Section->Test = Test;
	if (Section->ContinueSection != NULL) {
		DetermineLoop(Section->ContinueSection,Test,GetNewTestValue(),((BLOCK_SECTION *)Section->ContinueSection)->SectionID);
	}
	if (Section->JumpSection != NULL) {
		DetermineLoop(Section->JumpSection,Test,GetNewTestValue(),((BLOCK_SECTION *)Section->JumpSection)->SectionID);
	}
}

uint32_t DisplaySectionInformation (BLOCK_SECTION * Section, uint32_t ID, uint32_t Test) {
	return 1;
}

BLOCK_SECTION * ExistingSection(BLOCK_SECTION * StartSection, uint32_t Addr, uint32_t Test) {
	BLOCK_SECTION * Section;

	if (StartSection == NULL) { return NULL; }
	if (StartSection->StartPC == Addr) { return StartSection; }
	if (StartSection->Test == Test) { return NULL; }
	StartSection->Test = Test;
	Section = ExistingSection(StartSection->JumpSection,Addr,Test);
	if (Section != NULL) { return Section; }
	Section = ExistingSection(StartSection->ContinueSection,Addr,Test);
	if (Section != NULL) { return Section; }
	return NULL;
}

void  FillSectionInfo(BLOCK_SECTION * Section) {
	OPCODE Command;

	if (Section->CompiledLocation != NULL) { return; }
	Section->CompilePC      = Section->StartPC;
	memcpy(&Section->RegWorking,&Section->RegStart,sizeof(REG_INFO));
	NextInstruction = NORMAL;
	do {
		if (!r4300i_LW_VAddr(Section->CompilePC, &Command.Hex)) {
			//DisplayError(GS(MSG_FAIL_LOAD_WORD));
			StopEmulation();
		}

		switch (Command.op) {
		case R4300i_SPECIAL:
			switch (Command.funct) {
			case R4300i_SPECIAL_SLL:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && Command.rt == Command.rd) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt)) {
					MipsRegState(Command.rd) = STATE_CONST_32;
					MipsRegLo(Command.rd) = MipsRegLo(Command.rt) << Command.sa;
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SRL:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && Command.rt == Command.rd) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt)) {
					MipsRegState(Command.rd) = STATE_CONST_32;
					MipsRegLo(Command.rd) = MipsRegLo(Command.rt) >> Command.sa;
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SRA:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && Command.rt == Command.rd) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt)) {
					MipsRegState(Command.rd) = STATE_CONST_32;
					MipsRegLo(Command.rd) = MipsRegLo_S(Command.rt) >> Command.sa;
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SLLV:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt) && IsConst(Command.rs)) {
					MipsRegState(Command.rd) = STATE_CONST_32;
					MipsRegLo(Command.rd) = MipsRegLo(Command.rt) << (MipsRegLo(Command.rs) & 0x1F);
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SRLV:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt) && IsConst(Command.rs)) {
					MipsRegState(Command.rd) = STATE_CONST_32;
					MipsRegLo(Command.rd) = MipsRegLo(Command.rt) >> (MipsRegLo(Command.rs) & 0x1F);
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SRAV:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt) && IsConst(Command.rs)) {
					MipsRegState(Command.rd) = STATE_CONST_32;
					MipsRegLo(Command.rd) = MipsRegLo_S(Command.rt) >> (MipsRegLo(Command.rs) & 0x1F);
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_JR:
				if (IsConst(Command.rs)) {
					Section->Jump.TargetPC = MipsRegLo(Command.rs);
				} else {
					Section->Jump.TargetPC = (uint32_t)-1;
				}
				NextInstruction = DELAY_SLOT;
				break;
			case R4300i_SPECIAL_JALR:
				MipsRegLo(Opcode.rd) = Section->CompilePC + 8;
				MipsRegState(Opcode.rd) = STATE_CONST_32;
				if (IsConst(Command.rs)) {
					Section->Jump.TargetPC = MipsRegLo(Command.rs);
				} else {
					Section->Jump.TargetPC = (uint32_t)-1;
				}
				NextInstruction = DELAY_SLOT;
				break;
			case R4300i_SPECIAL_BREAK: break;
			case R4300i_SPECIAL_SYSCALL:
			//case R4300i_SPECIAL_BREAK:
				NextInstruction = END_BLOCK;
				Section->CompilePC -= 4;
				break;
			case R4300i_SPECIAL_MFHI: MipsRegState(Command.rd) = STATE_UNKNOWN; break;
			case R4300i_SPECIAL_MTHI: break;
			case R4300i_SPECIAL_MFLO: MipsRegState(Command.rd) = STATE_UNKNOWN; break;
			case R4300i_SPECIAL_MTLO: break;
			case R4300i_SPECIAL_DSLLV:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt) && IsConst(Command.rs)) {
					MipsRegState(Command.rd) = STATE_CONST_64;
					MipsReg(Command.rd) = Is64Bit(Command.rt)?MipsReg(Command.rt):(QWORD)MipsRegLo_S(Command.rt) << (MipsRegLo(Command.rs) & 0x3F);
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSRLV:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt) && IsConst(Command.rs)) {
					MipsRegState(Command.rd) = STATE_CONST_64;
					MipsReg(Command.rd) = Is64Bit(Command.rt)?MipsReg(Command.rt):(QWORD)MipsRegLo_S(Command.rt) >> (MipsRegLo(Command.rs) & 0x3F);
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSRAV:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt) && IsConst(Command.rs)) {
					MipsRegState(Command.rd) = STATE_CONST_64;
					MipsReg(Command.rd) = Is64Bit(Command.rt)?MipsReg_S(Command.rt):(int64_t)MipsRegLo_S(Command.rt) >> (MipsRegLo(Command.rs) & 0x3F);
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_MULT: break;
			case R4300i_SPECIAL_MULTU: break;
			case R4300i_SPECIAL_DIV: break;
			case R4300i_SPECIAL_DIVU: break;
			case R4300i_SPECIAL_DMULT: break;
			case R4300i_SPECIAL_DMULTU: break;
			case R4300i_SPECIAL_DDIV: break;
			case R4300i_SPECIAL_DDIVU: break;
			case R4300i_SPECIAL_ADD:
			case R4300i_SPECIAL_ADDU:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt) && IsConst(Command.rs)) {
					MipsRegLo(Command.rd) = MipsRegLo(Command.rs) + MipsRegLo(Command.rt);
					MipsRegState(Command.rd) = STATE_CONST_32;
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SUB:
			case R4300i_SPECIAL_SUBU:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt) && IsConst(Command.rs)) {
					MipsRegLo(Command.rd) = MipsRegLo(Command.rs) - MipsRegLo(Command.rt);
					MipsRegState(Command.rd) = STATE_CONST_32;
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_AND:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt) && IsConst(Command.rs)) {
					if (Is64Bit(Command.rt) && Is64Bit(Command.rs)) {
						MipsReg(Command.rd) = MipsReg(Command.rt) & MipsReg(Command.rs);
						MipsRegState(Command.rd) = STATE_CONST_64;
					} else if (Is64Bit(Command.rt) || Is64Bit(Command.rs)) {
						if (Is64Bit(Command.rt)) {
							MipsReg(Command.rd) = MipsReg(Command.rt) & MipsRegLo(Command.rs);
						} else {
							MipsReg(Command.rd) = MipsRegLo(Command.rt) & MipsReg(Command.rs);
						}
						MipsRegState(Command.rd) = ConstantsType(MipsReg(Command.rd));
					} else {
						MipsRegLo(Command.rd) = MipsRegLo(Command.rt) & MipsRegLo(Command.rs);
						MipsRegState(Command.rd) = STATE_CONST_32;
					}
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_OR:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt) && IsConst(Command.rs)) {
					if (Is64Bit(Command.rt) && Is64Bit(Command.rs)) {
						MipsReg(Command.rd) = MipsReg(Command.rt) | MipsReg(Command.rs);
						MipsRegState(Command.rd) = STATE_CONST_64;
					} else if (Is64Bit(Command.rt) || Is64Bit(Command.rs)) {
						if (Is64Bit(Command.rt)) {
							MipsReg(Command.rd) = MipsReg(Command.rt) | MipsRegLo(Command.rs);
						} else {
							MipsReg(Command.rd) = MipsRegLo(Command.rt) | MipsReg(Command.rs);
						}
						MipsRegState(Command.rd) = STATE_CONST_64;
					} else {
						MipsRegLo(Command.rd) = MipsRegLo(Command.rt) | MipsRegLo(Command.rs);
						MipsRegState(Command.rd) = STATE_CONST_32;
					}
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_XOR:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt) && IsConst(Command.rs)) {
					if (Is64Bit(Command.rt) && Is64Bit(Command.rs)) {
						MipsReg(Command.rd) = MipsReg(Command.rt) ^ MipsReg(Command.rs);
						MipsRegState(Command.rd) = STATE_CONST_64;
					} else if (Is64Bit(Command.rt) || Is64Bit(Command.rs)) {
						if (Is64Bit(Command.rt)) {
							MipsReg(Command.rd) = MipsReg(Command.rt) ^ MipsRegLo(Command.rs);
						} else {
							MipsReg(Command.rd) = MipsRegLo(Command.rt) ^ MipsReg(Command.rs);
						}
						MipsRegState(Command.rd) = STATE_CONST_64;
					} else {
						MipsRegLo(Command.rd) = MipsRegLo(Command.rt) ^ MipsRegLo(Command.rs);
						MipsRegState(Command.rd) = STATE_CONST_32;
					}
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_NOR:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt) && IsConst(Command.rs)) {
					if (Is64Bit(Command.rt) && Is64Bit(Command.rs)) {
						MipsReg(Command.rd) = ~(MipsReg(Command.rt) | MipsReg(Command.rs));
						MipsRegState(Command.rd) = STATE_CONST_64;
					} else if (Is64Bit(Command.rt) || Is64Bit(Command.rs)) {
						if (Is64Bit(Command.rt)) {
							MipsReg(Command.rd) = ~(MipsReg(Command.rt) | MipsRegLo(Command.rs));
						} else {
							MipsReg(Command.rd) = ~(MipsRegLo(Command.rt) | MipsReg(Command.rs));
						}
						MipsRegState(Command.rd) = STATE_CONST_64;
					} else {
						MipsRegLo(Command.rd) = ~(MipsRegLo(Command.rt) | MipsRegLo(Command.rs));
						MipsRegState(Command.rd) = STATE_CONST_32;
					}
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SLT:
				if (Command.rd == 0) { break; }
				if (IsConst(Command.rt) && IsConst(Command.rs)) {
					if (Is64Bit(Command.rt) || Is64Bit(Command.rs)) {
						if (Is64Bit(Command.rt)) {
							MipsRegLo(Command.rd) = (MipsRegLo_S(Command.rs) < MipsReg_S(Command.rt))?1:0;
						} else {
							MipsRegLo(Command.rd) = (MipsReg_S(Command.rs) < MipsRegLo_S(Command.rt))?1:0;
						}
					} else {
						MipsRegLo(Command.rd) = (MipsRegLo_S(Command.rs) < MipsRegLo_S(Command.rt))?1:0;
					}
					MipsRegState(Command.rd) = STATE_CONST_32;
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SLTU:
				if (Command.rd == 0) { break; }
				if (IsConst(Command.rt) && IsConst(Command.rs)) {
					if (Is64Bit(Command.rt) || Is64Bit(Command.rs)) {
						if (Is64Bit(Command.rt)) {
							MipsRegLo(Command.rd) = (MipsRegLo(Command.rs) < MipsReg(Command.rt))?1:0;
						} else {
							MipsRegLo(Command.rd) = (MipsReg(Command.rs) < MipsRegLo(Command.rt))?1:0;
						}
					} else {
						MipsRegLo(Command.rd) = (MipsRegLo(Command.rs) < MipsRegLo(Command.rt))?1:0;
					}
					MipsRegState(Command.rd) = STATE_CONST_32;
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DADD:
			case R4300i_SPECIAL_DADDU:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt) && IsConst(Command.rs)) {
					MipsReg(Command.rd) =
						Is64Bit(Command.rs)?MipsReg(Command.rs):(int64_t)MipsRegLo_S(Command.rs) +
						Is64Bit(Command.rt)?MipsReg(Command.rt):(int64_t)MipsRegLo_S(Command.rt);
					MipsRegState(Command.rd) = STATE_CONST_64;
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSUB:
			case R4300i_SPECIAL_DSUBU:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && (Command.rt == Command.rd || Command.rs == Command.rd)) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt) && IsConst(Command.rs)) {
					MipsReg(Command.rd) =
						Is64Bit(Command.rs)?MipsReg(Command.rs):(int64_t)MipsRegLo_S(Command.rs) -
						Is64Bit(Command.rt)?MipsReg(Command.rt):(int64_t)MipsRegLo_S(Command.rt);
					MipsRegState(Command.rd) = STATE_CONST_64;
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSLL:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && Command.rt == Command.rd) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt)) {
					MipsRegState(Command.rd) = STATE_CONST_64;
					MipsReg(Command.rd) = Is64Bit(Command.rt)?MipsReg(Command.rt):(int64_t)MipsRegLo_S(Command.rt) << Command.sa;
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSRL:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && Command.rt == Command.rd) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt)) {
					MipsRegState(Command.rd) = STATE_CONST_64;
					MipsReg(Command.rd) = Is64Bit(Command.rt)?MipsReg(Command.rt):(QWORD)MipsRegLo_S(Command.rt) >> Command.sa;
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSRA:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && Command.rt == Command.rd) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt)) {
					MipsRegState(Command.rd) = STATE_CONST_64;
					MipsReg_S(Command.rd) = Is64Bit(Command.rt)?MipsReg_S(Command.rt):(int64_t)MipsRegLo_S(Command.rt) >> Command.sa;
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSLL32:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && Command.rt == Command.rd) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt)) {
					MipsRegState(Command.rd) = STATE_CONST_64;
					MipsReg(Command.rd) = MipsRegLo(Command.rt) << (Command.sa + 32);
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSRL32:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && Command.rt == Command.rd) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt)) {
					MipsRegState(Command.rd) = STATE_CONST_32;
					MipsRegLo(Command.rd) = (uint32_t)(MipsReg(Command.rt) >> (Command.sa + 32));
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSRA32:
				if (Command.rd == 0) { break; }
				if (Section->InLoop && Command.rt == Command.rd) {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				if (IsConst(Command.rt)) {
					MipsRegState(Command.rd) = STATE_CONST_32;
					MipsRegLo(Command.rd) = (uint32_t)(MipsReg_S(Command.rt) >> (Command.sa + 32));
				} else {
					MipsRegState(Command.rd) = STATE_UNKNOWN;
				}
				break;
			default:
				NextInstruction = END_BLOCK;
				Section->CompilePC -= 4;
			}
			break;
		case R4300i_REGIMM:
			switch (Command.rt) {
			case R4300i_REGIMM_BLTZ:
			case R4300i_REGIMM_BGEZ:
				NextInstruction = DELAY_SLOT;
				Section->Cont.TargetPC = Section->CompilePC + 8;
				Section->Jump.TargetPC = Section->CompilePC + ((short)Command.offset << 2) + 4;
				if (Section->CompilePC == Section->Jump.TargetPC) {
					if (!DelaySlotEffectsCompare(Section->CompilePC,Command.rs,0)) {
						Section->Jump.PermLoop = 1;
					}
				}
				break;
			case R4300i_REGIMM_BLTZL:
			case R4300i_REGIMM_BGEZL:
				NextInstruction = LIKELY_DELAY_SLOT;
				Section->Cont.TargetPC = Section->CompilePC + 8;
				Section->Jump.TargetPC = Section->CompilePC + ((short)Command.offset << 2) + 4;
				if (Section->CompilePC == Section->Jump.TargetPC) {
					if (!DelaySlotEffectsCompare(Section->CompilePC,Command.rs,0)) {
						Section->Jump.PermLoop = 1;
					}
				}
				break;
			case R4300i_REGIMM_BLTZAL:
			case R4300i_REGIMM_BGEZAL:
				NextInstruction = DELAY_SLOT;
				MipsRegLo(31) = Section->CompilePC + 8;
				MipsRegState(31) = STATE_CONST_32;
				Section->Cont.TargetPC = Section->CompilePC + 8;
				Section->Jump.TargetPC = Section->CompilePC + ((short)Command.offset << 2) + 4;
				if (Section->CompilePC == Section->Jump.TargetPC) {
					if (!DelaySlotEffectsCompare(Section->CompilePC,Command.rs,0)) {
						Section->Jump.PermLoop = 1;
					}
				}
				break;
			default:
				NextInstruction = END_BLOCK;
				Section->CompilePC -= 4;
			}
			break;
		case R4300i_JAL:
			NextInstruction = DELAY_SLOT;
			MipsRegLo(31) = Section->CompilePC + 8;
			MipsRegState(31) = STATE_CONST_32;
			Section->Jump.TargetPC = (Section->CompilePC & 0xF0000000) + (Command.target << 2);
			if (Section->CompilePC == Section->Jump.TargetPC) {
				if (!DelaySlotEffectsCompare(Section->CompilePC,31,0)) {
					Section->Jump.PermLoop = 1;
				}
			}
			break;
		case R4300i_J:
			NextInstruction = DELAY_SLOT;
			Section->Jump.TargetPC = (Section->CompilePC & 0xF0000000) + (Command.target << 2);
			if (Section->CompilePC == Section->Jump.TargetPC) { Section->Jump.PermLoop = 1; }
			break;
		case R4300i_BEQ:
		case R4300i_BNE:
		case R4300i_BLEZ:
		case R4300i_BGTZ:
			NextInstruction = DELAY_SLOT;
			Section->Cont.TargetPC = Section->CompilePC + 8;
			Section->Jump.TargetPC = Section->CompilePC + ((short)Command.offset << 2) + 4;
			if (Section->CompilePC == Section->Jump.TargetPC) {
				if (!DelaySlotEffectsCompare(Section->CompilePC,Command.rs,Command.rt)) {
					Section->Jump.PermLoop = 1;
				}
			}
			break;
		case R4300i_ADDI:
		case R4300i_ADDIU:
			if (Command.rt == 0) { break; }
			if (Section->InLoop && Command.rs == Command.rt) {
				MipsRegState(Command.rt) = STATE_UNKNOWN;
			}
			if (IsConst(Command.rs)) {
				MipsRegLo(Command.rt) = MipsRegLo(Command.rs) + (short)Command.immediate;
				MipsRegState(Command.rt) = STATE_CONST_32;
			} else {
				MipsRegState(Command.rt) = STATE_UNKNOWN;
			}
			break;
		case R4300i_SLTI:
			if (Command.rt == 0) { break; }
			if (IsConst(Command.rs)) {
				if (Is64Bit(Command.rs)) {
					MipsRegLo(Command.rt) = (MipsReg_S(Command.rs) < (int64_t)((short)Command.immediate))?1:0;
				} else {
					MipsRegLo(Command.rt) = (MipsRegLo_S(Command.rs) < (int)((short)Command.immediate))?1:0;
				}
				MipsRegState(Command.rt) = STATE_CONST_32;
			} else {
				MipsRegState(Command.rt) = STATE_UNKNOWN;
			}
			break;
		case R4300i_SLTIU:
			if (Command.rt == 0) { break; }
			if (IsConst(Command.rs)) {
				if (Is64Bit(Command.rs)) {
					MipsRegLo(Command.rt) = (MipsReg(Command.rs) < (int64_t)((short)Command.immediate))?1:0;
				} else {
					MipsRegLo(Command.rt) = (MipsRegLo(Command.rs) < (uint32_t)((short)Command.immediate))?1:0;
				}
				MipsRegState(Command.rt) = STATE_CONST_32;
			} else {
				MipsRegState(Command.rt) = STATE_UNKNOWN;
			}
			break;
		case R4300i_LUI:
			if (Command.rt == 0) { break; }
			MipsRegLo(Command.rt) = ((short)Command.offset << 16);
			MipsRegState(Command.rt) = STATE_CONST_32;
			break;
		case R4300i_ANDI:
			if (Command.rt == 0) { break; }
			if (Section->InLoop && Command.rs == Command.rt) {
				MipsRegState(Command.rt) = STATE_UNKNOWN;
			}
			if (IsConst(Command.rs)) {
				MipsRegState(Command.rt) = STATE_CONST_32;
				MipsRegLo(Command.rt) = MipsRegLo(Command.rs) & Command.immediate;
			} else {
				MipsRegState(Command.rt) = STATE_UNKNOWN;
			}
			break;
		case R4300i_ORI:
			if (Command.rt == 0) { break; }
			if (Section->InLoop && Command.rs == Command.rt) {
				MipsRegState(Command.rt) = STATE_UNKNOWN;
			}
			if (IsConst(Command.rs)) {
				MipsRegState(Command.rt) = STATE_CONST_32;
				MipsRegLo(Command.rt) = MipsRegLo(Command.rs) | Command.immediate;
			} else {
				MipsRegState(Command.rt) = STATE_UNKNOWN;
			}
			break;
		case R4300i_XORI:
			if (Command.rt == 0) { break; }
			if (Section->InLoop && Command.rs == Command.rt) {
				MipsRegState(Command.rt) = STATE_UNKNOWN;
			}
			if (IsConst(Command.rs)) {
				MipsRegState(Command.rt) = STATE_CONST_32;
				MipsRegLo(Command.rt) = MipsRegLo(Command.rs) ^ Command.immediate;
			} else {
				MipsRegState(Command.rt) = STATE_UNKNOWN;
			}
			break;
		case R4300i_CP0:
			switch (Command.rs) {
			case R4300i_COP0_MF:
				if (Command.rt == 0) { break; }
				MipsRegState(Command.rt) = STATE_UNKNOWN;
				break;
			case R4300i_COP0_MT: break;
			default:
				if ( (Command.rs & 0x10 ) != 0 ) {
					switch( Command.funct ) {
					case R4300i_COP0_CO_TLBR: break;
					case R4300i_COP0_CO_TLBWI: break;
					case R4300i_COP0_CO_TLBWR: break;
					case R4300i_COP0_CO_TLBP: break;
					case R4300i_COP0_CO_ERET: NextInstruction = END_BLOCK; break;
					default:
						NextInstruction = END_BLOCK;
						Section->CompilePC -= 4;
					}
				} else {
					NextInstruction = END_BLOCK;
					Section->CompilePC -= 4;
				}
			}
			break;
		case R4300i_CP1:
			switch (Command.fmt) {
			case R4300i_COP1_CF:
			case R4300i_COP1_MF:
			case R4300i_COP1_DMF:
				if (Command.rt == 0) { break; }
				MipsRegState(Command.rt) = STATE_UNKNOWN;
				break;
			case R4300i_COP1_BC:
				switch (Command.ft) {
				case R4300i_COP1_BC_BCF:
				case R4300i_COP1_BC_BCT:
				case R4300i_COP1_BC_BCFL:
				case R4300i_COP1_BC_BCTL:
					NextInstruction = DELAY_SLOT;
					Section->Cont.TargetPC = Section->CompilePC + 8;
					Section->Jump.TargetPC = Section->CompilePC + ((short)Command.offset << 2) + 4;
					if (Section->CompilePC == Section->Jump.TargetPC) {
						int EffectDelaySlot;
						OPCODE NewCommand;

						if (!r4300i_LW_VAddr(Section->CompilePC + 4, &NewCommand.Hex)) {
							//DisplayError(GS(MSG_FAIL_LOAD_WORD));
							//ExitThread(0);
							StopEmulation();
						}

						EffectDelaySlot = 0;
						if (NewCommand.op == R4300i_CP1) {
							if (NewCommand.fmt == R4300i_COP1_S && (NewCommand.funct & 0x30) == 0x30 ) {
								EffectDelaySlot = 1;
							}
							if (NewCommand.fmt == R4300i_COP1_D && (NewCommand.funct & 0x30) == 0x30 ) {
								EffectDelaySlot = 1;
							}
						}
						if (!EffectDelaySlot) {
							Section->Jump.PermLoop = 1;
						}
					}
					break;
				}
				break;
			case R4300i_COP1_MT: break;
			case R4300i_COP1_DMT: break;
			case R4300i_COP1_CT: break;
			case R4300i_COP1_S: break;
			case R4300i_COP1_D: break;
			case R4300i_COP1_W: break;
			case R4300i_COP1_L: break;
			default:
				NextInstruction = END_BLOCK;
				Section->CompilePC -= 4;
			}
			break;
		case R4300i_BEQL:
		case R4300i_BNEL:
		case R4300i_BLEZL:
		case R4300i_BGTZL:
			NextInstruction = LIKELY_DELAY_SLOT;
			Section->Cont.TargetPC = Section->CompilePC + 8;
			Section->Jump.TargetPC = Section->CompilePC + ((short)Command.offset << 2) + 4;
			if (Section->CompilePC == Section->Jump.TargetPC) {
				if (!DelaySlotEffectsCompare(Section->CompilePC,Command.rs,Command.rt)) {
					Section->Jump.PermLoop = 1;
				}
			}
			break;
		case R4300i_DADDI:
		case R4300i_DADDIU:
			if (Command.rt == 0) { break; }
			if (Section->InLoop && Command.rs == Command.rt) {
				MipsRegState(Command.rt) = STATE_UNKNOWN;
			}
			if (IsConst(Command.rs)) {
				if (Is64Bit(Command.rs)) {
					int imm32 = (short)Opcode.immediate;
					int64_t imm64 = imm32;
					MipsReg_S(Command.rt) = MipsRegLo_S(Command.rs) + imm64;
				} else {
					MipsReg_S(Command.rt) = MipsRegLo_S(Command.rs) + (short)Command.immediate;
				}
				MipsRegState(Command.rt) = STATE_CONST_64;
			} else {
				MipsRegState(Command.rt) = STATE_UNKNOWN;
			}
			break;
		case R4300i_LDR:
		case R4300i_LDL:
		case R4300i_LB:
		case R4300i_LH:
		case R4300i_LWL:
		case R4300i_LW:
		case R4300i_LWU:
		case R4300i_LL:
		case R4300i_LBU:
		case R4300i_LHU:
		case R4300i_LWR:
		case R4300i_SC:
			if (Command.rt == 0) { break; }
			MipsRegState(Command.rt) = STATE_UNKNOWN;
			break;
		case R4300i_SB: break;
		case R4300i_SH: break;
		case R4300i_SWL: break;
		case R4300i_SW: break;
		case R4300i_SWR: break;
		case R4300i_SDL: break;
		case R4300i_SDR: break;
		case R4300i_CACHE: break;
		case R4300i_LWC1: break;
		case R4300i_SWC1: break;
		case R4300i_LDC1: break;
		case R4300i_LD:
			if (Command.rt == 0) { break; }
			MipsRegState(Command.rt) = STATE_UNKNOWN;
			break;
		case R4300i_SDC1: break;
		case R4300i_SD: break;
		default:
			NextInstruction = END_BLOCK;
			Section->CompilePC -= 4;
			if (Command.Hex == 0x7C1C97C0) { break; }
			if (Command.Hex == 0x7FFFFFFF) { break; }
			if (Command.Hex == 0xF1F3F5F7) { break; }
			if (Command.Hex == 0xC1200000) { break; }
			if (Command.Hex == 0x4C5A5353) { break; }
		}

		switch (NextInstruction) {
		case NORMAL:
			Section->CompilePC += 4;
			break;
		case DELAY_SLOT:
			NextInstruction = DELAY_SLOT_DONE;
			Section->CompilePC += 4;
			break;
		case LIKELY_DELAY_SLOT:
			memcpy(&Section->Cont.RegSet,&Section->RegWorking,sizeof(REG_INFO));
			NextInstruction = LIKELY_DELAY_SLOT_DONE;
			Section->CompilePC += 4;
			break;
		case DELAY_SLOT_DONE:
			memcpy(&Section->Cont.RegSet,&Section->RegWorking,sizeof(REG_INFO));
			memcpy(&Section->Jump.RegSet,&Section->RegWorking,sizeof(REG_INFO));
			NextInstruction = END_BLOCK;
			break;
		case LIKELY_DELAY_SLOT_DONE:
			memcpy(&Section->Jump.RegSet,&Section->RegWorking,sizeof(REG_INFO));
			NextInstruction = END_BLOCK;
			break;
		}
		if ((Section->CompilePC & 0xFFFFF000) != (Section->StartPC & 0xFFFFF000)) {
			if (NextInstruction != END_BLOCK && NextInstruction != NORMAL) {
			//	DisplayError("Branch running over delay slot ???\nNextInstruction == %d",NextInstruction);
				Section->Cont.TargetPC = (uint32_t)-1;
				Section->Jump.TargetPC = (uint32_t)-1;
			}
			NextInstruction = END_BLOCK;
			Section->CompilePC -= 4;
		}
	} while (NextInstruction != END_BLOCK);

	if (Section->Cont.TargetPC != (uint32_t)-1) {
		if ((Section->Cont.TargetPC & 0xFFFFF000) != (Section->StartPC & 0xFFFFF000)) {
			Section->Cont.TargetPC = (uint32_t)-1;
		}
	}
	if (Section->Jump.TargetPC != (uint32_t)-1) {
		if ((Section->Jump.TargetPC & 0xFFFFF000) != (Section->StartPC & 0xFFFFF000)) {
			Section->Jump.TargetPC = (uint32_t)-1;
		}
	}
}

void  FixConstants (BLOCK_SECTION * Section, uint32_t Test, int * Changed) {
	BLOCK_SECTION * Parent;
	int count, NoOfParents;
	REG_INFO Original[2];

	if (Section == NULL) { return; }
	if (Section->Test == Test) { return; }
	Section->Test = Test;

	InheritConstants(Section);

	memcpy(&Original[0],&Section->Cont.RegSet,sizeof(REG_INFO));
	memcpy(&Original[1],&Section->Jump.RegSet,sizeof(REG_INFO));

	if (Section->ParentSection) {
		for (NoOfParents = 0;Section->ParentSection[NoOfParents] != NULL;NoOfParents++) {
			Parent = Section->ParentSection[NoOfParents];
			if (Parent->ContinueSection == Section) {
				for (count = 0; count < 32; count++) {
					if (Section->RegStart.MIPS_RegState[count] != Parent->Cont.RegSet.MIPS_RegState[count]) {
						Section->RegStart.MIPS_RegState[count] = STATE_UNKNOWN;
						//*Changed = 1;
					}
					Section->RegStart.MIPS_RegState[count] = STATE_UNKNOWN;
				}
			}
			if (Parent->JumpSection == Section) {
				for (count = 0; count < 32; count++) {
					if (Section->RegStart.MIPS_RegState[count] != Parent->Jump.RegSet.MIPS_RegState[count]) {
						Section->RegStart.MIPS_RegState[count] = STATE_UNKNOWN;
						//*Changed = 1;
					}
				}
			}
			memcpy(&Section->RegWorking,&Section->RegStart,sizeof(REG_INFO));
		}
	}
	FillSectionInfo(Section);
	if (memcmp(&Original[0],&Section->Cont.RegSet,sizeof(REG_INFO)) != 0) { *Changed = 1; }
	if (memcmp(&Original[1],&Section->Jump.RegSet,sizeof(REG_INFO)) != 0) { *Changed = 1; }

	if (Section->JumpSection) { FixConstants(Section->JumpSection,Test,Changed); }
	if (Section->ContinueSection) { FixConstants(Section->ContinueSection,Test,Changed); }
}

void FixRandomReg (void) {
//	while ((int)Registers.CP0[1] < (int)Registers.CP0[6]) {
//		Registers.CP0[1] += 32 - Registers.CP0[6];
//	}
}

void FreeSection (BLOCK_SECTION * Section, BLOCK_SECTION * Parent) {
	if (Section == NULL) { return; }

	if (Section->ParentSection) {
		int NoOfParents, count;

		for (NoOfParents = 0;Section->ParentSection[NoOfParents] != NULL;NoOfParents++);

		for (count = 0; count < NoOfParents; count++) {
			if (Section->ParentSection[count] == Parent) {
				if (NoOfParents == 1) {
					free(Section->ParentSection);
					Section->ParentSection = NULL;
				} else {
					memmove(&Section->ParentSection[count],&Section->ParentSection[count + 1],
						sizeof(void*) * (NoOfParents - count));
					Section->ParentSection = realloc(Section->ParentSection,NoOfParents*sizeof(void *));
				}
				NoOfParents -= 1;
			}
		}

		if (Parent->JumpSection == Section) { Parent->JumpSection = NULL; }
		if (Parent->ContinueSection == Section) { Parent->ContinueSection = NULL; }

		if (Section->ParentSection) {
			for (count = 0; count < NoOfParents; count++) {
				if (!IsAllParentLoops(Section,Section->ParentSection[count],0,GetNewTestValue())) { return; }
			}
			for (count = 0; count < NoOfParents; count++) {
				Parent = Section->ParentSection[count];
				if (Parent->JumpSection == Section) { Parent->JumpSection = NULL; }
				if (Parent->ContinueSection == Section) { Parent->ContinueSection = NULL; }
			}
			free(Section->ParentSection);
			Section->ParentSection = NULL;
		}
	}
	if (Section->ParentSection == NULL) {
		FreeSection(Section->JumpSection,Section);
		FreeSection(Section->ContinueSection,Section);
		Section->JumpSection = NULL;
		Section->ContinueSection = NULL;
		free(Section);
		Section = 0;
	}
}

void GenerateBasicSectionLinkage (BLOCK_SECTION * Section) {
	//_asm int 3
}

void GenerateSectionLinkage (BLOCK_SECTION * Section) {
	BLOCK_SECTION * TargetSection[2], *Parent;
	JUMP_INFO * JumpInfo[2];
	uint8_t * Jump;
	int count;

	TargetSection[0] = Section->ContinueSection;
	TargetSection[1] = Section->JumpSection;
	JumpInfo[0] = &Section->Cont;
	JumpInfo[1] = &Section->Jump;

	for (count = 0; count < 2; count ++) {
		if (JumpInfo[count]->LinkLocation == NULL && JumpInfo[count]->FallThrough == 0) {
			JumpInfo[count]->TargetPC = -1;
		}
	}
	if ((Section->CompilePC & 0xFFC) == 0xFFC) {
		//Handle Fall througth
		Jump = NULL;
		for (count = 0; count < 2; count ++) {
			if (!JumpInfo[count]->FallThrough) { continue; }
			JumpInfo[count]->FallThrough = 0;
			if (JumpInfo[count]->LinkLocation != NULL) {
				SetJump32(JumpInfo[count]->LinkLocation,RecompPos);
				JumpInfo[count]->LinkLocation = NULL;
				if (JumpInfo[count]->LinkLocation2 != NULL) {
					SetJump32(JumpInfo[count]->LinkLocation2,RecompPos);
					JumpInfo[count]->LinkLocation2 = NULL;
				}
			}
			MoveConstToVariable(JumpInfo[count]->TargetPC,&JumpToLocation);
			if (JumpInfo[(count + 1) & 1]->LinkLocation == NULL) { break; }
			JmpLabel8(0);
			Jump = RecompPos - 1;
		}
		for (count = 0; count < 2; count ++) {
			if (JumpInfo[count]->LinkLocation == NULL) { continue; }
			JumpInfo[count]->FallThrough = 0;
			if (JumpInfo[count]->LinkLocation != NULL) {
				SetJump32(JumpInfo[count]->LinkLocation,RecompPos);
				JumpInfo[count]->LinkLocation = NULL;
				if (JumpInfo[count]->LinkLocation2 != NULL) {
					SetJump32(JumpInfo[count]->LinkLocation2,RecompPos);
					JumpInfo[count]->LinkLocation2 = NULL;
				}
			}
			MoveConstToVariable(JumpInfo[count]->TargetPC,&JumpToLocation);
			if (JumpInfo[(count + 1) & 1]->LinkLocation == NULL) { break; }
			JmpLabel8(0);
			Jump = RecompPos - 1;
		}
		if (Jump != NULL) {
			SetJump8(Jump,RecompPos);
		}
		MoveConstToVariable(Section->CompilePC + 4,&PROGRAM_COUNTER);
		if (BlockCycleCount != 0) {
			AddConstToVariable(BlockCycleCount,&CP0[9]);
			SubConstFromVariable(BlockCycleCount,&Timers->Timer);
		}
		if (BlockRandomModifier != 0) { SubConstFromVariable(BlockRandomModifier,&CP0[1]); }
		WriteBackRegisters(Section);
		MoveConstToVariable(DELAY_SLOT,&NextInstruction);
		Ret();
		return;
	}

	if (TargetSection[0] != TargetSection[1] || TargetSection[0] == NULL) {
		for (count = 0; count < 2; count ++) {
			if (JumpInfo[count]->LinkLocation == NULL && JumpInfo[count]->FallThrough == 0) {
				FreeSection(TargetSection[count],Section);
			} else if (TargetSection[count] == NULL && JumpInfo[count]->FallThrough) {
				if (JumpInfo[count]->LinkLocation != NULL) {
					SetJump32(JumpInfo[count]->LinkLocation,RecompPos);
					JumpInfo[count]->LinkLocation = NULL;
					if (JumpInfo[count]->LinkLocation2 != NULL) {
						SetJump32(JumpInfo[count]->LinkLocation2,RecompPos);
						JumpInfo[count]->LinkLocation2 = NULL;
					}
				}
				if (JumpInfo[count]->TargetPC > (Section->CompilePC + 4)) {
					CompileExit (JumpInfo[count]->TargetPC,&JumpInfo[count]->RegSet,Normal,1,NULL);
				} else {
					CompileExit (JumpInfo[count]->TargetPC,&JumpInfo[count]->RegSet,Normal,1,NULL);
				}
				JumpInfo[count]->FallThrough = 0;
			} else if (TargetSection[count] != NULL && JumpInfo[count] != NULL) {
				if (!JumpInfo[count]->FallThrough) { continue; }
				if (JumpInfo[count]->TargetPC == TargetSection[count]->StartPC) { continue; }
				if (JumpInfo[count]->LinkLocation != NULL) {
					SetJump32(JumpInfo[count]->LinkLocation,RecompPos);
					JumpInfo[count]->LinkLocation = NULL;
					if (JumpInfo[count]->LinkLocation2 != NULL) {
						SetJump32(JumpInfo[count]->LinkLocation2,RecompPos);
						JumpInfo[count]->LinkLocation2 = NULL;
					}
				}
				CompileExit (JumpInfo[count]->TargetPC,&JumpInfo[count]->RegSet,Normal,1,NULL);
				FreeSection(TargetSection[count],Section);
			}
		}
	} else {
		if (Section->Cont.LinkLocation == NULL && Section->Cont.FallThrough == 0) { Section->ContinueSection = NULL; }
		if (Section->Jump.LinkLocation == NULL && Section->Jump.FallThrough == 0) { Section->JumpSection = NULL; }
		if (Section->JumpSection == NULL &&  Section->ContinueSection == NULL) {
			FreeSection(TargetSection[0],Section);
		}
	}

	TargetSection[0] = Section->ContinueSection;
	TargetSection[1] = Section->JumpSection;

	for (count = 0; count < 2; count ++) {
		if (TargetSection[count] == NULL) { continue; }
		if (!JumpInfo[count]->FallThrough) { continue; }

		if (TargetSection[count]->CompiledLocation != NULL) {
			//char Label[100];
			JumpInfo[count]->FallThrough = 0;
			if (JumpInfo[count]->LinkLocation != NULL) {
				SetJump32(JumpInfo[count]->LinkLocation,RecompPos);
				JumpInfo[count]->LinkLocation = NULL;
				if (JumpInfo[count]->LinkLocation2 != NULL) {
					SetJump32(JumpInfo[count]->LinkLocation2,RecompPos);
					JumpInfo[count]->LinkLocation2 = NULL;
				}
			}
			if (JumpInfo[count]->RegSet.RandomModifier != 0) {
				SubConstFromVariable(JumpInfo[count]->RegSet.RandomModifier,&CP0[1]);
				JumpInfo[count]->RegSet.RandomModifier = 0;
			}
			if (JumpInfo[count]->RegSet.CycleCount != 0) {
				AddConstToVariable(JumpInfo[count]->RegSet.CycleCount,&CP0[9]);
			}
			if (JumpInfo[count]->TargetPC <= Section->CompilePC) {
				uint32_t CycleCount = JumpInfo[count]->RegSet.CycleCount;
				JumpInfo[count]->RegSet.CycleCount = 0;

				if (JumpInfo[count]->PermLoop) {
					MoveConstToVariable(JumpInfo[count]->TargetPC,&PROGRAM_COUNTER);
					Pushad();
					Call_Direct(InPermLoop);
					Popad();
					CompileSystemCheck(0,-1,JumpInfo[count]->RegSet);
				} else {
					CompileSystemCheck(CycleCount,JumpInfo[count]->TargetPC,JumpInfo[count]->RegSet);
				}
			} else {
				if (JumpInfo[count]->RegSet.CycleCount != 0) {
					SubConstFromVariable(JumpInfo[count]->RegSet.CycleCount,&Timers->Timer);
					JumpInfo[count]->RegSet.CycleCount = 0;
				}
			}
			memcpy(&Section->RegWorking, &JumpInfo[count]->RegSet,sizeof(REG_INFO));
			SyncRegState(Section,&TargetSection[count]->RegStart);
			JmpLabel32(0);
			SetJump32((uint32_t *)RecompPos - 1,TargetSection[count]->CompiledLocation);
		}
	}
	//Section->CycleCount = 0;
	//Section->RandomModifier = 0;

	for (count = 0; count < 2; count ++) {
		int count2;

		if (TargetSection[count] == NULL) { continue; }
		if (TargetSection[count]->ParentSection == NULL) { continue; }

		for (count2 = 0;TargetSection[count]->ParentSection[count2] != NULL;count2++) {
			Parent = TargetSection[count]->ParentSection[count2];
			if (Parent->CompiledLocation != NULL) { continue; }
			if (JumpInfo[count]->FallThrough) {
				JumpInfo[count]->FallThrough = 0;
				//JmpLabel32(JumpInfo[count]->BranchLabel,0);
				JmpLabel32(0);
				JumpInfo[count]->LinkLocation = RecompPos - 4;
			}
		}
	}

	for (count = 0; count < 2; count ++) {
		if (JumpInfo[count]->FallThrough) {
			if (JumpInfo[count]->TargetPC < Section->CompilePC) {
				uint32_t CycleCount = JumpInfo[count]->RegSet.CycleCount;;

				if (JumpInfo[count]->RegSet.RandomModifier != 0) {
					SubConstFromVariable(JumpInfo[count]->RegSet.RandomModifier,&CP0[1]);
					JumpInfo[count]->RegSet.RandomModifier = 0;
				}
				if (JumpInfo[count]->RegSet.CycleCount != 0) {
					AddConstToVariable(JumpInfo[count]->RegSet.CycleCount,&CP0[9]);
				}
				JumpInfo[count]->RegSet.CycleCount = 0;

				CompileSystemCheck(CycleCount,JumpInfo[count]->TargetPC,JumpInfo[count]->RegSet);
			}
		}
	}

	for (count = 0; count < 2; count ++) {
		if (JumpInfo[count]->FallThrough) {
			GenerateX86Code(TargetSection[count],GetNewTestValue());
		}
	}

	for (count = 0; count < 2; count ++) {
		if (JumpInfo[count]->LinkLocation == NULL) { continue; }
		if (TargetSection[count] == NULL) {
			SetJump32(JumpInfo[count]->LinkLocation,RecompPos);
			JumpInfo[count]->LinkLocation = NULL;
			if (JumpInfo[count]->LinkLocation2 != NULL) {
				SetJump32(JumpInfo[count]->LinkLocation2,RecompPos);
				JumpInfo[count]->LinkLocation2 = NULL;
			}
			CompileExit (JumpInfo[count]->TargetPC,&JumpInfo[count]->RegSet,Normal,1,NULL);
			continue;
		}
		if (JumpInfo[count]->TargetPC != TargetSection[count]->StartPC) {
			//DisplayError("I need to add more code in GenerateSectionLinkage cause this is going to cause an exception");
			//_asm int 3
		}
		if (TargetSection[count]->CompiledLocation == NULL) {
			GenerateX86Code(TargetSection[count],GetNewTestValue());
		} else {
			//char Label[100];

			SetJump32(JumpInfo[count]->LinkLocation,RecompPos);
			JumpInfo[count]->LinkLocation = NULL;
			if (JumpInfo[count]->LinkLocation2 != NULL) {
				SetJump32(JumpInfo[count]->LinkLocation2,RecompPos);
				JumpInfo[count]->LinkLocation2 = NULL;
			}
			memcpy(&Section->RegWorking,&JumpInfo[count]->RegSet,sizeof(REG_INFO));
			if (JumpInfo[count]->RegSet.RandomModifier != 0) {
				SubConstFromVariable(JumpInfo[count]->RegSet.RandomModifier,&CP0[1]);
				JumpInfo[count]->RegSet.RandomModifier = 0;
			}
			if (JumpInfo[count]->RegSet.CycleCount != 0) {
				AddConstToVariable(JumpInfo[count]->RegSet.CycleCount,&CP0[9]);
			}
			if (JumpInfo[count]->TargetPC <= Section->CompilePC) {
				uint32_t CycleCount = JumpInfo[count]->RegSet.CycleCount;
				JumpInfo[count]->RegSet.CycleCount = 0;

				if (JumpInfo[count]->PermLoop) {
					MoveConstToVariable(JumpInfo[count]->TargetPC,&PROGRAM_COUNTER);
					Pushad();
					Call_Direct(InPermLoop);
					Popad();
					CompileSystemCheck(0,-1,JumpInfo[count]->RegSet);
				} else {
					CompileSystemCheck(CycleCount,JumpInfo[count]->TargetPC,JumpInfo[count]->RegSet);
				}
			} else {
				if (JumpInfo[count]->RegSet.CycleCount != 0) {
					SubConstFromVariable(JumpInfo[count]->RegSet.CycleCount,&Timers->Timer);
					JumpInfo[count]->RegSet.CycleCount = 0;
				}
			}
			memcpy(&Section->RegWorking, &JumpInfo[count]->RegSet,sizeof(REG_INFO));
			SyncRegState(Section,&TargetSection[count]->RegStart);
			//JmpLabel32(Label,0);
			JmpLabel32(0);
			SetJump32((uint32_t *)RecompPos - 1,TargetSection[count]->CompiledLocation);
		}
	}
}


uint32_t GenerateX86Code (BLOCK_SECTION * Section, uint32_t Test) {
	int count;

	if (Section == NULL) { return 0; }
	if (Section->CompiledLocation != NULL) {
		if (Section->Test == Test) { return 0; }
		Section->Test = Test;
		if (GenerateX86Code(Section->ContinueSection,Test)) { return 1; }
		if (GenerateX86Code(Section->JumpSection,Test)) { return 1; }
		return 0;
	}
	if (Section->ParentSection) {
		for (count = 0;Section->ParentSection[count] != NULL;count++) {
			BLOCK_SECTION * Parent;

			Parent = Section->ParentSection[count];
			if (Parent->CompiledLocation != NULL) { continue; }
			if (IsAllParentLoops(Section,Parent,1,GetNewTestValue())) { continue; }
			return 0;
		}
	}
	if (!InheritParentInfo(Section)) { return 0; }
	Section->CompiledLocation = RecompPos;
	Section->CompilePC = Section->StartPC;
	NextInstruction = NORMAL;

#ifdef USEX64
	MoveConstQwordToX86reg(TLB_Map, x86_R15);
#endif

	do {
		//__try {
			if (!r4300i_LW_VAddr(Section->CompilePC, &Opcode.Hex)) {
				//DisplayError(GS(MSG_FAIL_LOAD_WORD));
				//ExitThread(0);
				StopEmulation();
			}

		BlockCycleCount += 2;

		BlockRandomModifier += 1;

		for (count = 1; count < 64; count ++) { x86Protected(count) = 0; }

		switch (Opcode.op) {
		case R4300i_SPECIAL:
			switch (Opcode.funct) {
			case R4300i_SPECIAL_SLL: Compile_R4300i_SPECIAL_SLL(Section); break;
			case R4300i_SPECIAL_SRL: Compile_R4300i_SPECIAL_SRL(Section); break;
			case R4300i_SPECIAL_SRA: Compile_R4300i_SPECIAL_SRA(Section); break;
			case R4300i_SPECIAL_SLLV: Compile_R4300i_SPECIAL_SLLV(Section); break;
			case R4300i_SPECIAL_SRLV: Compile_R4300i_SPECIAL_SRLV(Section); break;
			case R4300i_SPECIAL_SRAV: Compile_R4300i_SPECIAL_SRAV(Section); break;
			case R4300i_SPECIAL_JR: Compile_R4300i_SPECIAL_JR(Section); break;
			case R4300i_SPECIAL_JALR: Compile_R4300i_SPECIAL_JALR(Section); break;
			case R4300i_SPECIAL_MFLO: Compile_R4300i_SPECIAL_MFLO(Section); break;
			case R4300i_SPECIAL_SYSCALL: Compile_R4300i_SPECIAL_SYSCALL(Section); break;
			case R4300i_SPECIAL_BREAK: Compile_R4300i_SPECIAL_BREAK(Section); break;
			case R4300i_SPECIAL_MTLO: Compile_R4300i_SPECIAL_MTLO(Section); break;
			case R4300i_SPECIAL_MFHI: Compile_R4300i_SPECIAL_MFHI(Section); break;
			case R4300i_SPECIAL_MTHI: Compile_R4300i_SPECIAL_MTHI(Section); break;
			case R4300i_SPECIAL_DSLLV: Compile_R4300i_SPECIAL_DSLLV(Section); break;
			case R4300i_SPECIAL_DSRLV: Compile_R4300i_SPECIAL_DSRLV(Section); break;
			case R4300i_SPECIAL_DSRAV: Compile_R4300i_SPECIAL_DSRAV(Section); break;
			case R4300i_SPECIAL_MULT: Compile_R4300i_SPECIAL_MULT(Section); break;
			case R4300i_SPECIAL_DIV: Compile_R4300i_SPECIAL_DIV(Section); break;
			case R4300i_SPECIAL_DIVU: Compile_R4300i_SPECIAL_DIVU(Section); break;
			case R4300i_SPECIAL_MULTU: Compile_R4300i_SPECIAL_MULTU(Section); break;
			case R4300i_SPECIAL_DMULT: Compile_R4300i_SPECIAL_DMULT(Section); break;
			case R4300i_SPECIAL_DMULTU: Compile_R4300i_SPECIAL_DMULTU(Section); break;
			case R4300i_SPECIAL_DDIV: Compile_R4300i_SPECIAL_DDIV(Section); break;
			case R4300i_SPECIAL_DDIVU: Compile_R4300i_SPECIAL_DDIVU(Section); break;
			case R4300i_SPECIAL_ADD: Compile_R4300i_SPECIAL_ADD(Section); break;
			case R4300i_SPECIAL_ADDU: Compile_R4300i_SPECIAL_ADDU(Section); break;
			case R4300i_SPECIAL_SUB: Compile_R4300i_SPECIAL_SUB(Section); break;
			case R4300i_SPECIAL_SUBU: Compile_R4300i_SPECIAL_SUBU(Section); break;
			case R4300i_SPECIAL_AND: Compile_R4300i_SPECIAL_AND(Section); break;
			case R4300i_SPECIAL_OR: Compile_R4300i_SPECIAL_OR(Section); break;
			case R4300i_SPECIAL_XOR: Compile_R4300i_SPECIAL_XOR(Section); break;
			case R4300i_SPECIAL_NOR: Compile_R4300i_SPECIAL_NOR(Section); break;
			case R4300i_SPECIAL_SLT: Compile_R4300i_SPECIAL_SLT(Section); break;
			case R4300i_SPECIAL_SLTU: Compile_R4300i_SPECIAL_SLTU(Section); break;
			case R4300i_SPECIAL_DADD: Compile_R4300i_SPECIAL_DADD(Section); break;
			case R4300i_SPECIAL_DADDU: Compile_R4300i_SPECIAL_DADDU(Section); break;
			case R4300i_SPECIAL_DSUB: Compile_R4300i_SPECIAL_DSUB(Section); break;
			case R4300i_SPECIAL_DSUBU: Compile_R4300i_SPECIAL_DSUBU(Section); break;
			case R4300i_SPECIAL_DSLL: Compile_R4300i_SPECIAL_DSLL(Section); break;
			case R4300i_SPECIAL_DSRL: Compile_R4300i_SPECIAL_DSRL(Section); break;
			case R4300i_SPECIAL_DSRA: Compile_R4300i_SPECIAL_DSRA(Section); break;
			case R4300i_SPECIAL_DSLL32: Compile_R4300i_SPECIAL_DSLL32(Section); break;
			case R4300i_SPECIAL_DSRL32: Compile_R4300i_SPECIAL_DSRL32(Section); break;
			case R4300i_SPECIAL_DSRA32: Compile_R4300i_SPECIAL_DSRA32(Section); break;
			default:
				Compile_R4300i_UnknownOpcode(Section); break;
			}
			break;
		case R4300i_REGIMM:
			switch (Opcode.rt) {
			case R4300i_REGIMM_BLTZ:Compile_R4300i_Branch(Section,BLTZ_Compare,BranchTypeRs, 0); break;
			case R4300i_REGIMM_BGEZ:Compile_R4300i_Branch(Section,BGEZ_Compare,BranchTypeRs, 0); break;
			case R4300i_REGIMM_BLTZL:Compile_R4300i_BranchLikely(Section,BLTZ_Compare, 0); break;
			case R4300i_REGIMM_BGEZL:Compile_R4300i_BranchLikely(Section,BGEZ_Compare, 0); break;
			case R4300i_REGIMM_BLTZAL:Compile_R4300i_Branch(Section,BLTZ_Compare,BranchTypeRs, 1); break;
			case R4300i_REGIMM_BGEZAL:Compile_R4300i_Branch(Section,BGEZ_Compare,BranchTypeRs, 1); break;
			default:
				Compile_R4300i_UnknownOpcode(Section); break;
			}
			break;
		case R4300i_BEQ: Compile_R4300i_Branch(Section,BEQ_Compare,BranchTypeRsRt,0); break;
		case R4300i_BNE: Compile_R4300i_Branch(Section,BNE_Compare,BranchTypeRsRt,0); break;
		case R4300i_BGTZ:Compile_R4300i_Branch(Section,BGTZ_Compare,BranchTypeRs,0); break;
		case R4300i_BLEZ:Compile_R4300i_Branch(Section,BLEZ_Compare,BranchTypeRs,0); break;
		case R4300i_J: Compile_R4300i_J(Section); break;
		case R4300i_JAL: Compile_R4300i_JAL(Section); break;
		case R4300i_ADDI: Compile_R4300i_ADDI(Section); break;
		case R4300i_ADDIU: Compile_R4300i_ADDIU(Section); break;
		case R4300i_SLTI: Compile_R4300i_SLTI(Section); break;
		case R4300i_SLTIU: Compile_R4300i_SLTIU(Section); break;
		case R4300i_ANDI: Compile_R4300i_ANDI(Section); break;
		case R4300i_ORI: Compile_R4300i_ORI(Section); break;
		case R4300i_XORI: Compile_R4300i_XORI(Section); break;
		case R4300i_LUI: Compile_R4300i_LUI(Section); break;
		case R4300i_CP0:
			switch (Opcode.rs) {
			case R4300i_COP0_MF: Compile_R4300i_COP0_MF(Section); break;
			case R4300i_COP0_MT: Compile_R4300i_COP0_MT(Section); break;
			default:
				if ( (Opcode.rs & 0x10 ) != 0 ) {
					switch( Opcode.funct ) {
					case R4300i_COP0_CO_TLBR: Compile_R4300i_COP0_CO_TLBR(Section); break;
					case R4300i_COP0_CO_TLBWI: Compile_R4300i_COP0_CO_TLBWI(Section); break;
					case R4300i_COP0_CO_TLBWR: Compile_R4300i_COP0_CO_TLBWR(Section); break;
					case R4300i_COP0_CO_TLBP: Compile_R4300i_COP0_CO_TLBP(Section); break;
					case R4300i_COP0_CO_ERET: Compile_R4300i_COP0_CO_ERET(Section); break;
					default: Compile_R4300i_UnknownOpcode(Section); break;
					}
				} else {
					Compile_R4300i_UnknownOpcode(Section);
				}
			}
			break;
		case R4300i_CP1:
			switch (Opcode.rs) {
			case R4300i_COP1_MF: Compile_R4300i_COP1_MF(Section); break;
			case R4300i_COP1_DMF: Compile_R4300i_COP1_DMF(Section); break;
			case R4300i_COP1_CF: Compile_R4300i_COP1_CF(Section); break;
			case R4300i_COP1_MT: Compile_R4300i_COP1_MT(Section); break;
			case R4300i_COP1_DMT: Compile_R4300i_COP1_DMT(Section); break;
			case R4300i_COP1_CT: Compile_R4300i_COP1_CT(Section); break;
			case R4300i_COP1_BC:
				switch (Opcode.ft) {
				case R4300i_COP1_BC_BCF: Compile_R4300i_Branch(Section,COP1_BCF_Compare,BranchTypeCop1,0); break;
				case R4300i_COP1_BC_BCT: Compile_R4300i_Branch(Section,COP1_BCT_Compare,BranchTypeCop1,0); break;
				case R4300i_COP1_BC_BCFL: Compile_R4300i_BranchLikely(Section,COP1_BCF_Compare,0); break;
				case R4300i_COP1_BC_BCTL: Compile_R4300i_BranchLikely(Section,COP1_BCT_Compare,0); break;
				default:
					Compile_R4300i_UnknownOpcode(Section); break;
				}
				break;
			case R4300i_COP1_S:
				switch (Opcode.funct) {
				case R4300i_COP1_FUNCT_ADD: Compile_R4300i_COP1_S_ADD(Section); break;
				case R4300i_COP1_FUNCT_SUB: Compile_R4300i_COP1_S_SUB(Section); break;
				case R4300i_COP1_FUNCT_MUL: Compile_R4300i_COP1_S_MUL(Section); break;
				case R4300i_COP1_FUNCT_DIV: Compile_R4300i_COP1_S_DIV(Section); break;
				case R4300i_COP1_FUNCT_ABS: Compile_R4300i_COP1_S_ABS(Section); break;
				case R4300i_COP1_FUNCT_NEG: Compile_R4300i_COP1_S_NEG(Section); break;
				case R4300i_COP1_FUNCT_SQRT: Compile_R4300i_COP1_S_SQRT(Section); break;
				case R4300i_COP1_FUNCT_MOV: Compile_R4300i_COP1_S_MOV(Section); break;
				case R4300i_COP1_FUNCT_TRUNC_L: Compile_R4300i_COP1_S_TRUNC_L(Section); break;
				case R4300i_COP1_FUNCT_CEIL_L: Compile_R4300i_COP1_S_CEIL_L(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_FLOOR_L: Compile_R4300i_COP1_S_FLOOR_L(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_ROUND_W: Compile_R4300i_COP1_S_ROUND_W(Section); break;
				case R4300i_COP1_FUNCT_TRUNC_W: Compile_R4300i_COP1_S_TRUNC_W(Section); break;
				case R4300i_COP1_FUNCT_CEIL_W: Compile_R4300i_COP1_S_CEIL_W(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_FLOOR_W: Compile_R4300i_COP1_S_FLOOR_W(Section); break;
				case R4300i_COP1_FUNCT_CVT_D: Compile_R4300i_COP1_S_CVT_D(Section); break;
				case R4300i_COP1_FUNCT_CVT_W: Compile_R4300i_COP1_S_CVT_W(Section); break;
				case R4300i_COP1_FUNCT_CVT_L: Compile_R4300i_COP1_S_CVT_L(Section); break;
				case R4300i_COP1_FUNCT_C_F:   case R4300i_COP1_FUNCT_C_UN:
				case R4300i_COP1_FUNCT_C_EQ:  case R4300i_COP1_FUNCT_C_UEQ:
				case R4300i_COP1_FUNCT_C_OLT: case R4300i_COP1_FUNCT_C_ULT:
				case R4300i_COP1_FUNCT_C_OLE: case R4300i_COP1_FUNCT_C_ULE:
				case R4300i_COP1_FUNCT_C_SF:  case R4300i_COP1_FUNCT_C_NGLE:
				case R4300i_COP1_FUNCT_C_SEQ: case R4300i_COP1_FUNCT_C_NGL:
				case R4300i_COP1_FUNCT_C_LT:  case R4300i_COP1_FUNCT_C_NGE:
				case R4300i_COP1_FUNCT_C_LE:  case R4300i_COP1_FUNCT_C_NGT:
					Compile_R4300i_COP1_S_CMP(Section); break;
				default:
					Compile_R4300i_UnknownOpcode(Section); break;
				}
				break;
			case R4300i_COP1_D:
				switch (Opcode.funct) {
				case R4300i_COP1_FUNCT_ADD: Compile_R4300i_COP1_D_ADD(Section); break;
				case R4300i_COP1_FUNCT_SUB: Compile_R4300i_COP1_D_SUB(Section); break;
				case R4300i_COP1_FUNCT_MUL: Compile_R4300i_COP1_D_MUL(Section); break;
				case R4300i_COP1_FUNCT_DIV: Compile_R4300i_COP1_D_DIV(Section); break;
				case R4300i_COP1_FUNCT_ABS: Compile_R4300i_COP1_D_ABS(Section); break;
				case R4300i_COP1_FUNCT_NEG: Compile_R4300i_COP1_D_NEG(Section); break;
				case R4300i_COP1_FUNCT_SQRT: Compile_R4300i_COP1_D_SQRT(Section); break;
				case R4300i_COP1_FUNCT_MOV: Compile_R4300i_COP1_D_MOV(Section); break;
				case R4300i_COP1_FUNCT_TRUNC_L: Compile_R4300i_COP1_D_TRUNC_L(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_CEIL_L: Compile_R4300i_COP1_D_CEIL_L(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_FLOOR_L: Compile_R4300i_COP1_D_FLOOR_L(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_ROUND_W: Compile_R4300i_COP1_D_ROUND_W(Section); break;
				case R4300i_COP1_FUNCT_TRUNC_W: Compile_R4300i_COP1_D_TRUNC_W(Section); break;
				case R4300i_COP1_FUNCT_CEIL_W: Compile_R4300i_COP1_D_CEIL_W(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_FLOOR_W: Compile_R4300i_COP1_D_FLOOR_W(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_CVT_S: Compile_R4300i_COP1_D_CVT_S(Section); break;
				case R4300i_COP1_FUNCT_CVT_W: Compile_R4300i_COP1_D_CVT_W(Section); break;
				case R4300i_COP1_FUNCT_CVT_L: Compile_R4300i_COP1_D_CVT_L(Section); break;
				case R4300i_COP1_FUNCT_C_F:   case R4300i_COP1_FUNCT_C_UN:
				case R4300i_COP1_FUNCT_C_EQ:  case R4300i_COP1_FUNCT_C_UEQ:
				case R4300i_COP1_FUNCT_C_OLT: case R4300i_COP1_FUNCT_C_ULT:
				case R4300i_COP1_FUNCT_C_OLE: case R4300i_COP1_FUNCT_C_ULE:
				case R4300i_COP1_FUNCT_C_SF:  case R4300i_COP1_FUNCT_C_NGLE:
				case R4300i_COP1_FUNCT_C_SEQ: case R4300i_COP1_FUNCT_C_NGL:
				case R4300i_COP1_FUNCT_C_LT:  case R4300i_COP1_FUNCT_C_NGE:
				case R4300i_COP1_FUNCT_C_LE:  case R4300i_COP1_FUNCT_C_NGT:
					Compile_R4300i_COP1_D_CMP(Section); break;
				default:
					Compile_R4300i_UnknownOpcode(Section); break;
				}
				break;
			case R4300i_COP1_W:
				switch (Opcode.funct) {
				case R4300i_COP1_FUNCT_CVT_S: Compile_R4300i_COP1_W_CVT_S(Section); break;
				case R4300i_COP1_FUNCT_CVT_D: Compile_R4300i_COP1_W_CVT_D(Section); break;
				default:
					Compile_R4300i_UnknownOpcode(Section); break;
				}
				break;
			case R4300i_COP1_L:
				switch (Opcode.funct) {
				case R4300i_COP1_FUNCT_CVT_S: Compile_R4300i_COP1_L_CVT_S(Section); break;
				case R4300i_COP1_FUNCT_CVT_D: Compile_R4300i_COP1_L_CVT_D(Section); break;
				default:
					Compile_R4300i_UnknownOpcode(Section); break;
				}
				break;
			default:
				Compile_R4300i_UnknownOpcode(Section); break;
			}
			break;
		case R4300i_BEQL: Compile_R4300i_BranchLikely(Section,BEQ_Compare,0); break;
		case R4300i_BNEL: Compile_R4300i_BranchLikely(Section,BNE_Compare,0); break;
		case R4300i_BGTZL:Compile_R4300i_BranchLikely(Section,BGTZ_Compare,0); break;
		case R4300i_BLEZL:Compile_R4300i_BranchLikely(Section,BLEZ_Compare,0); break;
		case R4300i_DADDIU: Compile_R4300i_DADDIU(Section); break;
		case R4300i_LDL: Compile_R4300i_LDL(Section); break;
		case R4300i_LDR: Compile_R4300i_LDR(Section); break;
		case R4300i_LB: Compile_R4300i_LB(Section); break;
		case R4300i_LH: Compile_R4300i_LH(Section); break;
		case R4300i_LWL: Compile_R4300i_LWL(Section); break;
		case R4300i_LW: Compile_R4300i_LW(Section); break;
		case R4300i_LBU: Compile_R4300i_LBU(Section); break;
		case R4300i_LHU: Compile_R4300i_LHU(Section); break;
		case R4300i_LWR: Compile_R4300i_LWR(Section); break;
		case R4300i_LWU: Compile_R4300i_LWU(Section); break;	//added by Witten
		case R4300i_SB: Compile_R4300i_SB(Section); break;
		case R4300i_SH: Compile_R4300i_SH(Section); break;
		case R4300i_SWL: Compile_R4300i_SWL(Section); break;
		case R4300i_SW: Compile_R4300i_SW(Section); break;
		case R4300i_SWR: Compile_R4300i_SWR(Section); break;
		case R4300i_SDL: Compile_R4300i_SDL(Section); break;
		case R4300i_SDR: Compile_R4300i_SDR(Section); break;
		case R4300i_CACHE: Compile_R4300i_CACHE(Section); break;
		case R4300i_LL: Compile_R4300i_LL(Section); break;
		case R4300i_LWC1: Compile_R4300i_LWC1(Section); break;
		case R4300i_LDC1: Compile_R4300i_LDC1(Section); break;
		case R4300i_SC: Compile_R4300i_SC(Section); break;
		case R4300i_LD: Compile_R4300i_LD(Section); break;
		case R4300i_SWC1: Compile_R4300i_SWC1(Section); break;
		case R4300i_SDC1: Compile_R4300i_SDC1(Section); break;
		case R4300i_SD: Compile_R4300i_SD(Section); break;
		default:
			Compile_R4300i_UnknownOpcode(Section); break;
		}

		for (count = 1; count < 64; count ++) { x86Protected(count) = 0; }

		UnMap_AllFPRs(Section);

		if ((Section->CompilePC &0xFFC) == 0xFFC) {
			if (NextInstruction == DO_DELAY_SLOT) {
			}
			if (NextInstruction == NORMAL) {
				CompileExit (Section->CompilePC + 4,&Section->RegWorking,Normal,1,NULL);
				NextInstruction = END_BLOCK;
			}
		}

		switch (NextInstruction) {
		case NORMAL:
			Section->CompilePC += 4;
			break;
		case DO_DELAY_SLOT:
			NextInstruction = DELAY_SLOT;
			Section->CompilePC += 4;
			break;
		case DELAY_SLOT:
			NextInstruction = DELAY_SLOT_DONE;
			BlockCycleCount -= 2;
			BlockRandomModifier -= 1;
			Section->CompilePC -= 4;
			break;
		}

	} while (NextInstruction != END_BLOCK);

	return 1;
}

uint32_t GetNewTestValue(void) {
	static uint32_t LastTest = 0;
	if (LastTest == 0xFFFFFFFF) { LastTest = 0; }
	LastTest += 1;
	return LastTest;
}

void InitilizeRegSet(REG_INFO * RegSet) {
	int count;

	RegSet->MIPS_RegState[0]  = STATE_CONST_32;
	RegSet->MIPS_RegVal[0].DW = 0;
	for (count = 1; count < 32; count ++ ) {
		RegSet->MIPS_RegState[count]   = STATE_UNKNOWN;
		RegSet->MIPS_RegVal[count].DW = 0;

	}
	for (count = 0; count < 64; count ++ ) {
		RegSet->x86reg_MappedTo[count]  = NotMapped;
		RegSet->x86reg_Protected[count] = 0;
		RegSet->x86reg_MapOrder[count]  = 0;
	}
	RegSet->CycleCount = 0;
	RegSet->RandomModifier = 0;

	RegSet->Stack_TopPos = 0;
	for (count = 0; count < 8; count ++ ) {
		RegSet->x86fpu_MappedTo[count] = -1;
		RegSet->x86fpu_State[count] = FPU_Unkown;
		RegSet->x86fpu_RoundingModel[count] = RoundDefault;
	}
	RegSet->Fpu_Used = 0;
	RegSet->RoundingModel = RoundUnknown;
}

void  InheritConstants(BLOCK_SECTION * Section) {
	int NoOfParents, count;
	BLOCK_SECTION * Parent;
	REG_INFO * RegSet;


	if (Section->ParentSection == NULL) {
		InitilizeRegSet(&Section->RegStart);
		memcpy(&Section->RegWorking,&Section->RegStart,sizeof(REG_INFO));
		return;
	}

	Parent = Section->ParentSection[0];
	RegSet = Section == Parent->ContinueSection?&Parent->Cont.RegSet:&Parent->Jump.RegSet;
	memcpy(&Section->RegStart,RegSet,sizeof(REG_INFO));
	memcpy(&Section->RegWorking,&Section->RegStart,sizeof(REG_INFO));

	for (NoOfParents = 1;Section->ParentSection[NoOfParents] != NULL;NoOfParents++) {
		Parent = Section->ParentSection[NoOfParents];
		RegSet = Section == Parent->ContinueSection?&Parent->Cont.RegSet:&Parent->Jump.RegSet;

		for (count = 0; count < 32; count++) {
			if (IsConst(count)) {
				if (MipsRegState(count) != RegSet->MIPS_RegState[count]) {
					MipsRegState(count) = STATE_UNKNOWN;
				} else if (Is32Bit(count) && MipsRegLo(count) != RegSet->MIPS_RegVal[count].UW[0]) {
					MipsRegState(count) = STATE_UNKNOWN;
				} else if (Is64Bit(count) && MipsReg(count) != RegSet->MIPS_RegVal[count].UDW) {
					MipsRegState(count) = STATE_UNKNOWN;
				}
			}
		}
	}
	memcpy(&Section->RegStart,&Section->RegWorking,sizeof(REG_INFO));
}

uint32_t InheritParentInfo (BLOCK_SECTION * Section) {
	int count, start, NoOfParents, NoOfCompiledParents, FirstParent,CurrentParent;
	BLOCK_PARENT * SectionParents;
	BLOCK_SECTION * Parent;
	JUMP_INFO * JumpInfo;
	//char Label[100];
	uint32_t NeedSync;

	DisplaySectionInformation(Section,Section->SectionID,GetNewTestValue());

	if (Section->ParentSection == NULL) {
		InitilizeRegSet(&Section->RegStart);
		memcpy(&Section->RegWorking,&Section->RegStart,sizeof(REG_INFO));
		return 1;
	}

	NoOfParents = 0;
	for (count = 0;Section->ParentSection[count] != NULL;count++) {
		Parent = Section->ParentSection[count];
		NoOfParents += Parent->JumpSection != Parent->ContinueSection?1:2;
	}

	if (NoOfParents == 0) {
		return 0;
	} else if (NoOfParents == 1) {
		Parent = Section->ParentSection[0];
		if (Section == Parent->ContinueSection) { JumpInfo = &Parent->Cont; }
		else if (Section == Parent->JumpSection) { JumpInfo = &Parent->Jump; }
		else {
		}

		memcpy(&Section->RegStart,&JumpInfo->RegSet,sizeof(REG_INFO));
		if (JumpInfo->LinkLocation != NULL) {
			SetJump32(JumpInfo->LinkLocation,RecompPos);
			if (JumpInfo->LinkLocation2 != NULL) {
				SetJump32(JumpInfo->LinkLocation2,RecompPos);
			}
		}
		memcpy(&Section->RegWorking,&Section->RegStart,sizeof(REG_INFO));
		return 1;
	}

	//Multiple Parents
	for (count = 0, NoOfCompiledParents = 0;Section->ParentSection[count] != NULL;count++) {
		Parent = Section->ParentSection[count];
		if (Parent->CompiledLocation != NULL) {
			NoOfCompiledParents += Parent->JumpSection != Parent->ContinueSection?1:2;
		}
	}
	if (NoOfCompiledParents == 0){ return 0; }
	SectionParents = (BLOCK_PARENT *)malloc(NoOfParents * sizeof(BLOCK_PARENT));

	for (count = 0, NoOfCompiledParents = 0;Section->ParentSection[count] != NULL;count++) {
		Parent = Section->ParentSection[count];
		if (Parent->CompiledLocation == NULL) { continue; }
		if (Parent->JumpSection != Parent->ContinueSection) {
			SectionParents[NoOfCompiledParents].Parent = Parent;
			SectionParents[NoOfCompiledParents].JumpInfo =
				Section == Parent->ContinueSection?&Parent->Cont:&Parent->Jump;
			NoOfCompiledParents += 1;
		} else {
			SectionParents[NoOfCompiledParents].Parent = Parent;
			SectionParents[NoOfCompiledParents].JumpInfo = &Parent->Cont;
			NoOfCompiledParents += 1;
			SectionParents[NoOfCompiledParents].Parent = Parent;
			SectionParents[NoOfCompiledParents].JumpInfo = &Parent->Jump;
			NoOfCompiledParents += 1;
		}
	}

	start = NoOfCompiledParents;
	for (count = 0;Section->ParentSection[count] != NULL;count++) {
		Parent = Section->ParentSection[count];
		if (Parent->CompiledLocation != NULL) { continue; }
		if (Parent->JumpSection != Parent->ContinueSection) {
			SectionParents[start].Parent = Parent;
			SectionParents[start].JumpInfo =
				Section == Parent->ContinueSection?&Parent->Cont:&Parent->Jump;
			start += 1;
		} else {
			SectionParents[start].Parent = Parent;
			SectionParents[start].JumpInfo = &Parent->Cont;
			start += 1;
			SectionParents[start].Parent = Parent;
			SectionParents[start].JumpInfo = &Parent->Jump;
			start += 1;
		}
	}
	FirstParent = 0;
	for (count = 1;count < NoOfCompiledParents;count++) {
		if (SectionParents[count].JumpInfo->FallThrough) {
			FirstParent = count; break;
		}
	}

	//Link First Parent to start
	Parent = SectionParents[FirstParent].Parent;
	JumpInfo = SectionParents[FirstParent].JumpInfo;

	memcpy(&Section->RegWorking,&JumpInfo->RegSet,sizeof(REG_INFO));
	if (JumpInfo->LinkLocation != NULL) {
		SetJump32(JumpInfo->LinkLocation,RecompPos);
		JumpInfo->LinkLocation  = NULL;
		if (JumpInfo->LinkLocation2 != NULL) {
			SetJump32(JumpInfo->LinkLocation2,RecompPos);
			JumpInfo->LinkLocation2  = NULL;
		}
	}
	if (BlockRandomModifier != 0) { SubConstFromVariable(BlockRandomModifier,&CP0[1]); }
	if (BlockCycleCount != 0) {
		AddConstToVariable(BlockCycleCount,&CP0[9]);
		SubConstFromVariable(BlockCycleCount,&Timers->Timer);
	}
	JumpInfo->FallThrough   = 0;

	//Fix up initial state
	UnMap_AllFPRs(Section);
	for (count = 0;count < NoOfParents;count++) {
		int count2, count3;
		REG_INFO * RegSet;

		if (count == FirstParent) { continue; }
		Parent = SectionParents[count].Parent;
		RegSet = &SectionParents[count].JumpInfo->RegSet;

		if (CurrentRoundingModel != RegSet->RoundingModel) { CurrentRoundingModel = RoundUnknown; }
		if (NoOfParents != NoOfCompiledParents) { CurrentRoundingModel = RoundUnknown; }

		for (count2 = 1; count2 < 32; count2++) {
			if (Is32BitMapped(count2)) {
				switch (RegSet->MIPS_RegState[count2]) {
				case STATE_MAPPED_64: Map_GPR_64bit(Section,count2,count2); break;
				case STATE_MAPPED_32_ZERO: break;
				case STATE_MAPPED_32_SIGN:
					if (IsUnsigned(count2)) {
						MipsRegState(count2) = STATE_MAPPED_32_SIGN;
					}
					break;
				case STATE_CONST_64: Map_GPR_64bit(Section,count2,count2); break;
				case STATE_CONST_32:
					if ((RegSet->MIPS_RegVal[count2].W[0] < 0) && IsUnsigned(count2)) {
						MipsRegState(count2) = STATE_MAPPED_32_SIGN;
					}
					break;
				case STATE_UNKNOWN:
					//Map_GPR_32bit(Section,count2,1,count2);
					Map_GPR_64bit(Section,count2,count2); //??
					//UnMap_GPR(Section,count2,1); ??
					break;
				}
			}
			if (IsConst(count2)) {
				if (MipsRegState(count2) != RegSet->MIPS_RegState[count2]) {
					if (Is32Bit(count2)) {
						Map_GPR_32bit(Section,count2,1,count2);
					} else {
						Map_GPR_32bit(Section,count2,1,count2);
					}
				} else if (Is32Bit(count2) && MipsRegLo(count2) != RegSet->MIPS_RegVal[count2].UW[0]) {
					Map_GPR_32bit(Section,count2,1,count2);
				} else if (Is64Bit(count2) && MipsReg(count2) != RegSet->MIPS_RegVal[count2].UDW) {
					Map_GPR_32bit(Section,count2,1,count2);
				}
			}
			for (count3 = 1; count3 < 64; count3 ++) { x86Protected(count3) = 0; }
		}
	}
	memcpy(&Section->RegStart,&Section->RegWorking,sizeof(REG_INFO));

	//Sync registers for different blocks
//	sprintf(Label,"Section_%d",Section->SectionID);
	CurrentParent = FirstParent;
	NeedSync = 0;
	for (count = 0;count < NoOfCompiledParents;count++) {
		REG_INFO * RegSet;
		int count2;

		if (count == FirstParent) { continue; }
		Parent    = SectionParents[count].Parent;
		JumpInfo = SectionParents[count].JumpInfo;
		RegSet   = &SectionParents[count].JumpInfo->RegSet;

		if (JumpInfo->RegSet.CycleCount != 0) { NeedSync = 1; }
		if (JumpInfo->RegSet.RandomModifier  != 0) { NeedSync = 1; }

		for (count2 = 0; count2 < 8; count2++) {
			if (FpuMappedTo(count2) == (uint32_t)-1) {
				NeedSync = 1;
			}
		}

		for (count2 = 0; count2 < 32; count2++) {
			if (NeedSync == 1)  { break; }
			if (MipsRegState(count2) != RegSet->MIPS_RegState[count2]) {
				NeedSync = 1;
				continue;
			}
			switch (MipsRegState(count2)) {
			case STATE_UNKNOWN: break;
			case STATE_MAPPED_64:
				if (MipsReg(count2) != RegSet->MIPS_RegVal[count2].UDW) {
					NeedSync = 1;
				}
				break;
			case STATE_MAPPED_32_ZERO:
			case STATE_MAPPED_32_SIGN:
				if (MipsRegLo(count2) != RegSet->MIPS_RegVal[count2].UW[0]) {
					//DisplayError("Parent: %d",Parent->SectionID);
					NeedSync = 1;
				}
				break;
			case STATE_CONST_32:
				if (MipsRegLo(count2) != RegSet->MIPS_RegVal[count2].UW[0]) {
					NeedSync = 1;
				}
				break;
			}
		}
		if (NeedSync == 0) { continue; }
		Parent   = SectionParents[CurrentParent].Parent;
		JumpInfo = SectionParents[CurrentParent].JumpInfo;
		//JmpLabel32(Label,0);
		JmpLabel32(0);
		JumpInfo->LinkLocation  = RecompPos - 4;
		JumpInfo->LinkLocation2 = NULL;

		CurrentParent = count;
		Parent   = SectionParents[CurrentParent].Parent;
		JumpInfo = SectionParents[CurrentParent].JumpInfo;
		if (JumpInfo->LinkLocation != NULL) {
			SetJump32(JumpInfo->LinkLocation,RecompPos);
			JumpInfo->LinkLocation = NULL;
			if (JumpInfo->LinkLocation2 != NULL) {
				SetJump32(JumpInfo->LinkLocation2,RecompPos);
				JumpInfo->LinkLocation2 = NULL;
			}
		}
		memcpy(&Section->RegWorking,&JumpInfo->RegSet,sizeof(REG_INFO));
		if (BlockRandomModifier != 0) { SubConstFromVariable(BlockRandomModifier,&CP0[1]); }
		if (BlockCycleCount != 0) {
			AddConstToVariable(BlockCycleCount,&CP0[9]);
			SubConstFromVariable(BlockCycleCount,&Timers->Timer);
		}
		SyncRegState(Section,&Section->RegStart); 		//Sync
		memcpy(&Section->RegStart,&Section->RegWorking,sizeof(REG_INFO));

	}

	for (count = 0;count < NoOfCompiledParents;count++) {
		Parent   = SectionParents[count].Parent;
		JumpInfo = SectionParents[count].JumpInfo;

		if (JumpInfo->LinkLocation != NULL) {
			SetJump32(JumpInfo->LinkLocation,RecompPos);
			JumpInfo->LinkLocation = NULL;
			if (JumpInfo->LinkLocation2 != NULL) {
				SetJump32(JumpInfo->LinkLocation2,RecompPos);
				JumpInfo->LinkLocation2 = NULL;
			}
		}
	}

	BlockCycleCount = 0;
	BlockRandomModifier = 0;
	free(SectionParents);
	SectionParents = NULL;
	return 1;
}

uint32_t IsAllParentLoops(BLOCK_SECTION * Section, BLOCK_SECTION * Parent, uint32_t IgnoreIfCompiled, uint32_t Test) {
	int count;

	if (IgnoreIfCompiled && Parent->CompiledLocation != NULL) { return 1; }
	if (!Section->InLoop) { return 0; }
	if (!Parent->InLoop) { return 0; }
	if (Parent->ParentSection == NULL) { return 0; }
	if (Section == Parent) { return 1; }
	if (Parent->Test == Test) { return 1; }
	Parent->Test = Test;

	for (count = 0;Parent->ParentSection[count] != NULL;count++) {
		if (!IsAllParentLoops(Section,Parent->ParentSection[count],IgnoreIfCompiled,Test)) { return 0; }
	}
	return 1;
}

void  InitilzeSection (BLOCK_SECTION * Section, BLOCK_SECTION * Parent, uint32_t StartAddr, uint32_t ID) {
	Section->ParentSection      = NULL;
	Section->JumpSection        = NULL;
	Section->ContinueSection    = NULL;
	Section->CompiledLocation   = NULL;

	Section->SectionID          = ID;
	Section->Test               = 0;
	Section->Test2              = 0;
	Section->InLoop             = 0;

	Section->StartPC            = StartAddr;
	Section->CompilePC          = Section->StartPC;

	Section->Jump.LinkLocation  = NULL;
	Section->Jump.LinkLocation2 = NULL;
	Section->Jump.FallThrough   = 0;
	Section->Jump.PermLoop      = 0;
	Section->Jump.TargetPC      = (uint32_t)-1;
	Section->Cont.LinkLocation  = NULL;
	Section->Cont.LinkLocation2 = NULL;
	Section->Cont.FallThrough   = 0;
	Section->Cont.PermLoop      = 0;
	Section->Cont.TargetPC      = (uint32_t)-1;

	AddParent(Section,Parent);
}

void MarkCodeBlock (uint32_t PAddr) {
	if (PAddr < RdramSize) {
		N64_Blocks.NoOfRDRamBlocks[PAddr >> 12] += 1;
	} else if (PAddr >= 0x04000000 && PAddr <= 0x04000FFC) {
		N64_Blocks.NoOfDMEMBlocks += 1;
	} else if (PAddr >= 0x04001000 && PAddr <= 0x04001FFC) {
		N64_Blocks.NoOfIMEMBlocks += 1;
	} else if (PAddr >= 0x1FC00000 && PAddr <= 0x1FC00800) {
		N64_Blocks.NoOfPifRomBlocks += 1;
	} else {
	}
}

extern uint8_t *  MemChunk;

void CallBlock(void (*block)(void)) {
			
#ifdef USEX64 	
	// Make sure the Memory block pointer is in register R15
	__asm__ __volatile__("mov %%rax, %%r15" : : "a"(MemChunk));
	__asm__ __volatile__("pushq %rbx");
	__asm__ __volatile__("pushq %rcx");
	__asm__ __volatile__("pushq %rdx");
	__asm__ __volatile__("pushq %r9");
	__asm__ __volatile__("pushq %r10");
	__asm__ __volatile__("pushq %r11");
	__asm__ __volatile__("pushq %r12");
	__asm__ __volatile__("pushq %r13");
	__asm__ __volatile__("pushq %r14");
	__asm__ __volatile__("pushq %r15");

	block();

	__asm__ __volatile__("popq %r15");
	__asm__ __volatile__("popq %r14");
	__asm__ __volatile__("popq %r13");
	__asm__ __volatile__("popq %r12");
	__asm__ __volatile__("popq %r11");
	__asm__ __volatile__("popq %r10");
	__asm__ __volatile__("popq %r9");
	__asm__ __volatile__("popq %rdx");
	__asm__ __volatile__("popq %rcx");
	__asm__ __volatile__("popq %rbx");
#else
	__asm__ __volatile__("pusha");
	block();
	__asm__ __volatile__("popa");
#endif

}
//#include <windows.h>
//int r4300i_CPU_MemoryFilter64( DWORD dwExptCode, LPEXCEPTION_POINTERS lpEP);

uint32_t lastgood = 0;


void StartRecompilerCPU (void ) {
	uintptr_t Addr;
	void (*Block)(void) = 0;

	InitExceptionHandler();

	ResetRecompCode();
	memset(&N64_Blocks,0,sizeof(N64_Blocks));
	NextInstruction = NORMAL;

	//__try {

		//for (;;) {
		while(cpu_running == 1) {

			if(PROGRAM_COUNTER < 0x90000000)
				lastgood = PROGRAM_COUNTER;
			Addr = PROGRAM_COUNTER;
				if (!TranslateVaddr(&Addr)) {
					DoTLBMiss(NextInstruction == DELAY_SLOT,PROGRAM_COUNTER);
					NextInstruction = NORMAL;
					Addr = PROGRAM_COUNTER;
					if (!TranslateVaddr(&Addr)) {
						StopEmulation();
					}
				}
			if (NextInstruction == DELAY_SLOT) {
					Block = *(DelaySlotTable + (Addr >> 12));

				if (Block == NULL) {
					Block = CompileDelaySlot();

					*(DelaySlotTable + (Addr >> 12)) = Block;

					NextInstruction = NORMAL;
				}

				CallBlock(Block);

				continue;
			}


				if (Addr > 0x10000000)
				{
					if (PROGRAM_COUNTER >= 0xB0000000 && PROGRAM_COUNTER < (RomFileSize | 0xB0000000)) {
						while (PROGRAM_COUNTER >= 0xB0000000 && PROGRAM_COUNTER < (RomFileSize | 0xB0000000)) {
							ExecuteInterpreterOpCode();
						}
						continue;
					} else {

						StopEmulation();
					}
				}

			Block = *(JumpTable + (Addr >> 2));


			if (Block == NULL) {
				Block = Compiler4300iBlock();

				*(JumpTable + (Addr >> 2)) = Block;

				NextInstruction = NORMAL;
			}


			CallBlock(Block);
		}
		
		cpu_stopped = 1;
}


void SyncRegState (BLOCK_SECTION * Section, REG_INFO * SyncTo) {
	int count, x86Reg,x86RegHi, changed;

	changed = 0;
	UnMap_AllFPRs(Section);
	if (CurrentRoundingModel != SyncTo->RoundingModel) { CurrentRoundingModel = RoundUnknown; }

	for (count = 1; count < 32; count ++) {
		if (MipsRegState(count) == SyncTo->MIPS_RegState[count]) {
			switch (MipsRegState(count)) {
			case STATE_UNKNOWN: continue;
			case STATE_MAPPED_64:
				if (MipsReg(count) == SyncTo->MIPS_RegVal[count].UDW) {
					continue;
				}
				break;
			case STATE_MAPPED_32_ZERO:
			case STATE_MAPPED_32_SIGN:
				if (MipsRegLo(count) == SyncTo->MIPS_RegVal[count].UW[0]) {
					continue;
				}
				break;
			case STATE_CONST_64:
				if (MipsReg(count) != SyncTo->MIPS_RegVal[count].UDW) {
				}
				continue;
			case STATE_CONST_32:
				if (MipsRegLo(count) != SyncTo->MIPS_RegVal[count].UW[0]) {
				}
				continue;
			}
		}
		changed = 1;

		switch (SyncTo->MIPS_RegState[count]) {
		case STATE_UNKNOWN: UnMap_GPR(Section,count,1);  break;
		case STATE_MAPPED_64:
			x86Reg = SyncTo->MIPS_RegVal[count].UW[0];
			x86RegHi = SyncTo->MIPS_RegVal[count].UW[1];
			UnMap_X86reg(Section,x86Reg);
			UnMap_X86reg(Section,x86RegHi);
			switch (MipsRegState(count)) {
			case STATE_UNKNOWN:
				MoveVariableToX86reg(&GPR[count].UW[0],x86Reg);
				MoveVariableToX86reg(&GPR[count].UW[1],x86RegHi);
				break;
			case STATE_MAPPED_64:
				MoveX86RegToX86Reg(MipsRegLo(count),x86Reg);
				x86Mapped(MipsRegLo(count)) = NotMapped;
				MoveX86RegToX86Reg(MipsRegHi(count),x86RegHi);
				x86Mapped(MipsRegHi(count)) = NotMapped;
				break;
			case STATE_MAPPED_32_SIGN:
				MoveX86RegToX86Reg(MipsRegLo(count),x86RegHi);
				ShiftRightSignImmed(x86RegHi,31);
				MoveX86RegToX86Reg(MipsRegLo(count),x86Reg);
				x86Mapped(MipsRegLo(count)) = NotMapped;
				break;
			case STATE_MAPPED_32_ZERO:
				XorX86RegToX86Reg(x86RegHi,x86RegHi);
				MoveX86RegToX86Reg(MipsRegLo(count),x86Reg);
				x86Mapped(MipsRegLo(count)) = NotMapped;
				break;
			case STATE_CONST_64:
				MoveConstToX86reg(MipsRegHi(count),x86RegHi);
				MoveConstToX86reg(MipsRegLo(count),x86Reg);
				break;
			case STATE_CONST_32:
				MoveConstToX86reg(MipsRegLo_S(count) >> 31,x86RegHi);
				MoveConstToX86reg(MipsRegLo(count),x86Reg);
				break;
			default:
				continue;
			}
			MipsRegLo(count) = x86Reg;
			MipsRegHi(count) = x86RegHi;
			MipsRegState(count) = STATE_MAPPED_64;
			x86Mapped(x86Reg) = GPR_Mapped;
			x86Mapped(x86RegHi) = GPR_Mapped;
			x86MapOrder(x86Reg) = 1;
			x86MapOrder(x86RegHi) = 1;
			break;
		case STATE_MAPPED_32_SIGN:
			x86Reg = SyncTo->MIPS_RegVal[count].UW[0];
			UnMap_X86reg(Section,x86Reg);
			switch (MipsRegState(count)) {
			case STATE_UNKNOWN: MoveVariableToX86reg(&GPR[count].UW[0],x86Reg); break;
			case STATE_CONST_32: MoveConstToX86reg(MipsRegLo(count),x86Reg); break;
			case STATE_MAPPED_32_SIGN:
				MoveX86RegToX86Reg(MipsRegLo(count),x86Reg);
				x86Mapped(MipsRegLo(count)) = NotMapped;
				break;
			case STATE_MAPPED_32_ZERO:
				if (MipsRegLo(count) != (uint32_t)x86Reg) {
					MoveX86RegToX86Reg(MipsRegLo(count),x86Reg);
					x86Mapped(MipsRegLo(count)) = NotMapped;
				}
				break;
			case STATE_MAPPED_64:
				MoveX86RegToX86Reg(MipsRegLo(count),x86Reg);
				x86Mapped(MipsRegLo(count)) = NotMapped;
				x86Mapped(MipsRegHi(count)) = NotMapped;
				break;
			}
			MipsRegLo(count) = x86Reg;
			MipsRegState(count) = STATE_MAPPED_32_SIGN;
			x86Mapped(x86Reg) = GPR_Mapped;
			x86MapOrder(x86Reg) = 1;
			break;
		case STATE_MAPPED_32_ZERO:
			x86Reg = SyncTo->MIPS_RegVal[count].UW[0];
			UnMap_X86reg(Section,x86Reg);
			switch (MipsRegState(count)) {
			case STATE_MAPPED_64:
			case STATE_UNKNOWN:
				MoveVariableToX86reg(&GPR[count].UW[0],x86Reg);
				break;
			case STATE_MAPPED_32_ZERO:
				MoveX86RegToX86Reg(MipsRegLo(count),x86Reg);
				x86Mapped(MipsRegLo(count)) = NotMapped;
				break;
			case STATE_CONST_32:
				if (MipsRegLo_S(count) < 0) {
				}
				MoveConstToX86reg(MipsRegLo(count),x86Reg);
				break;
			}
			MipsRegLo(count) = x86Reg;
			MipsRegState(count) = SyncTo->MIPS_RegState[count];
			x86Mapped(x86Reg) = GPR_Mapped;
			x86MapOrder(x86Reg) = 1;
			break;
		default:
			changed = 0;
		}
	}
}
