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
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <fpu_control.h>
#include "main.h"
#include "cpu.h"

int32_t RoundingModel = _FPU_RC_NEAREST;

#define ADDRESS_ERROR_EXCEPTION(Address,FromRead) \
	DoAddressError(NextInstruction == JUMP,Address,FromRead);\
	NextInstruction = JUMP;\
	JumpToLocation = PROGRAM_COUNTER;\
	return;

//#define TEST_COP1_USABLE_EXCEPTION
#define TEST_COP1_USABLE_EXCEPTION \
	if ((STATUS_REGISTER & STATUS_CU1) == 0) {\
		DoCopUnusableException(NextInstruction == JUMP,1);\
		NextInstruction = JUMP;\
		JumpToLocation = PROGRAM_COUNTER;\
		return;\
	}

#define TLB_READ_EXCEPTION(Address) \
	DoTLBMiss(NextInstruction == JUMP,Address);\
	NextInstruction = JUMP;\
	JumpToLocation = PROGRAM_COUNTER;\
	return;

/************************* OpCode functions *************************/
void r4300i_J (void) {
	NextInstruction = DELAY_SLOT;
	JumpToLocation = (PROGRAM_COUNTER & 0xF0000000) + (Opcode.target << 2);
	TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,0,0);
}

void r4300i_JAL (void) {
	NextInstruction = DELAY_SLOT;
	JumpToLocation = (PROGRAM_COUNTER & 0xF0000000) + (Opcode.target << 2);
	TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,0,0);
	GPR[31].DW= (int32_t)(PROGRAM_COUNTER + 8);
}

void r4300i_BEQ (void) {
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.rs].DW == GPR[Opcode.rt].DW) {
		JumpToLocation = PROGRAM_COUNTER + ((int16_t)Opcode.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.rs,Opcode.rt);
	} else {
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void r4300i_BNE (void) {
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.rs].DW != GPR[Opcode.rt].DW) {
		JumpToLocation = PROGRAM_COUNTER + ((int16_t)Opcode.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.rs,Opcode.rt);
	} else {
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void r4300i_BLEZ (void) {
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.rs].DW <= 0) {
		JumpToLocation = PROGRAM_COUNTER + ((int16_t)Opcode.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.rs,0);
	} else {
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void r4300i_BGTZ (void) {
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.rs].DW > 0) {
		JumpToLocation = PROGRAM_COUNTER + ((int16_t)Opcode.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.rs,0);
	} else {
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void r4300i_ADDI (void) {
	if (Opcode.rt == 0) { return; }
	GPR[Opcode.rt].DW = (GPR[Opcode.rs].W[0] + ((int16_t)Opcode.immediate));
}

void r4300i_ADDIU (void) {
	GPR[Opcode.rt].DW = (GPR[Opcode.rs].W[0] + ((int16_t)Opcode.immediate));
}

void r4300i_SLTI (void) {
	if (GPR[Opcode.rs].DW < (int64_t)((int16_t)Opcode.immediate)) {
		GPR[Opcode.rt].DW = 1;
	} else {
		GPR[Opcode.rt].DW = 0;
	}
}

void r4300i_SLTIU (void) {
	int32_t imm32 = (int16_t)Opcode.immediate;
	int64_t imm64;

	imm64 = imm32;
	GPR[Opcode.rt].DW = GPR[Opcode.rs].UDW < (uint64_t)imm64?1:0;
}

void r4300i_ANDI (void) {
	GPR[Opcode.rt].DW = GPR[Opcode.rs].DW & Opcode.immediate;
}

void r4300i_ORI (void) {
	GPR[Opcode.rt].DW = GPR[Opcode.rs].DW | Opcode.immediate;
}

void r4300i_XORI (void) {
	GPR[Opcode.rt].DW = GPR[Opcode.rs].DW ^ Opcode.immediate;
}

void r4300i_LUI (void) {
	if (Opcode.rt == 0) { return; }
	GPR[Opcode.rt].DW = (int32_t)((int16_t)Opcode.offset << 16);
}

void r4300i_BEQL (void) {
	if (GPR[Opcode.rs].DW == GPR[Opcode.rt].DW) {
		NextInstruction = DELAY_SLOT;
		JumpToLocation = PROGRAM_COUNTER + ((int16_t)Opcode.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.rs,Opcode.rt);
	} else {
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void r4300i_BNEL (void) {
	if (GPR[Opcode.rs].DW != GPR[Opcode.rt].DW) {
		NextInstruction = DELAY_SLOT;
		JumpToLocation = PROGRAM_COUNTER + ((int16_t)Opcode.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.rs,Opcode.rt);
	} else {
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void r4300i_BLEZL (void) {
	if (GPR[Opcode.rs].DW <= 0) {
		NextInstruction = DELAY_SLOT;
		JumpToLocation = PROGRAM_COUNTER + ((int16_t)Opcode.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.rs,0);
	} else {
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void r4300i_BGTZL (void) {
	if (GPR[Opcode.rs].DW > 0) {
		NextInstruction = DELAY_SLOT;
		JumpToLocation = PROGRAM_COUNTER + ((int16_t)Opcode.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.rs,0);
	} else {
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void r4300i_DADDIU (void) {
	GPR[Opcode.rt].DW = GPR[Opcode.rs].DW + (int64_t)((int16_t)Opcode.immediate);
}

uint64_t LDL_MASK[8] = { 0ULL,0xFFULL,0xFFFFULL,0xFFFFFFULL,0xFFFFFFFFULL,0xFFFFFFFFFFULL, 0xFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFULL };
int32_t LDL_SHIFT[8] = { 0, 8, 16, 24, 32, 40, 48, 56 };

void r4300i_LDL (void) {
	uint32_t Offset, Address;
	uint64_t Value;

	Address = GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;
	Offset  = Address & 7;

	if (!r4300i_LD_VAddr((Address & ~7),&Value)) {
		return;
	}
	GPR[Opcode.rt].DW = GPR[Opcode.rt].DW & LDL_MASK[Offset];
	GPR[Opcode.rt].DW += Value << LDL_SHIFT[Offset];
}

uint64_t LDR_MASK[8] = { 0xFFFFFFFFFFFFFF00ULL, 0xFFFFFFFFFFFF0000ULL,
                      0xFFFFFFFFFF000000ULL, 0xFFFFFFFF00000000ULL,
                      0xFFFFFF0000000000ULL, 0xFFFF000000000000ULL,
                      0xFF00000000000000ULL, 0 };
int32_t LDR_SHIFT[8] = { 56, 48, 40, 32, 24, 16, 8, 0 };

void r4300i_LDR (void) {
	uint32_t Offset, Address;
	uint64_t Value;

	Address = GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;
	Offset  = Address & 7;

	if (!r4300i_LD_VAddr((Address & ~7),&Value)) {
		return;
	}

	GPR[Opcode.rt].DW = GPR[Opcode.rt].DW & LDR_MASK[Offset];
	GPR[Opcode.rt].DW += Value >> LDR_SHIFT[Offset];

}

void r4300i_LB (void) {
	uint32_t Address =  GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;
	if (Opcode.rt == 0) { return; }
	if (!r4300i_LB_VAddr(Address,&GPR[Opcode.rt].UB[0])) {
		TLB_READ_EXCEPTION(Address);
	} else {
		GPR[Opcode.rt].DW = GPR[Opcode.rt].B[0];
	}
}

void r4300i_LH (void) {
	uint32_t Address =  GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;
	if ((Address & 1) != 0) { ADDRESS_ERROR_EXCEPTION(Address,1); }
	if (!r4300i_LH_VAddr(Address,&GPR[Opcode.rt].UHW[0])) {
		//if (ShowTLBMisses) {
			DisplayError("LH TLB: %X",Address);
		//}
		TLB_READ_EXCEPTION(Address);
	} else {
		GPR[Opcode.rt].DW = GPR[Opcode.rt].HW[0];
	}
}

uint32_t LWL_MASK[4] = { 0,0xFF,0xFFFF,0xFFFFFF };
int32_t LWL_SHIFT[4] = { 0, 8, 16, 24};

void r4300i_LWL (void) {
	uint32_t Offset, Address, Value;

	Address = GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;
	Offset  = Address & 3;

	if (!r4300i_LW_VAddr((Address & ~3),&Value)) {
		return;
	}

	GPR[Opcode.rt].DW = (int32_t)(GPR[Opcode.rt].W[0] & LWL_MASK[Offset]);
	GPR[Opcode.rt].DW += (int32_t)(Value << LWL_SHIFT[Offset]);
}

void r4300i_LW (void) {
	uint32_t Address =  GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;


//	if ((Address & 3) != 0) { ADDRESS_ERROR_EXCEPTION(Address,1); }

	if (Opcode.rt == 0) { return; }


	if (!r4300i_LW_VAddr(Address,&GPR[Opcode.rt].UW[0])) {
		//if (ShowTLBMisses) {
			printf("LW TLB: %X",Address);
		//}
		TLB_READ_EXCEPTION(Address);
	} else {
		GPR[Opcode.rt].DW = GPR[Opcode.rt].W[0];
	}
}

void r4300i_LBU (void) {
	uint32_t Address =  GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;
	if (!r4300i_LB_VAddr(Address,&GPR[Opcode.rt].UB[0])) {
		//if (ShowTLBMisses) {
			DisplayError("LBU TLB: %X",Address);
		//}
		TLB_READ_EXCEPTION(Address);
	} else {
		GPR[Opcode.rt].UDW = GPR[Opcode.rt].UB[0];
	}
}

void r4300i_LHU (void) {
	uint32_t Address =  GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;
	if ((Address & 1) != 0) { ADDRESS_ERROR_EXCEPTION(Address,1); }
	if (!r4300i_LH_VAddr(Address,&GPR[Opcode.rt].UHW[0])) {
		//if (ShowTLBMisses) {
			DisplayError("LHU TLB: %X",Address);
		//}
		TLB_READ_EXCEPTION(Address);
	} else {
		GPR[Opcode.rt].UDW = GPR[Opcode.rt].UHW[0];
	}
}

uint32_t LWR_MASK[4] = { 0xFFFFFF00, 0xFFFF0000, 0xFF000000, 0 };
int32_t LWR_SHIFT[4] = { 24, 16 ,8, 0 };

void r4300i_LWR (void) {
	uint32_t Offset, Address, Value;

	Address = GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;
	Offset  = Address & 3;

	if (!r4300i_LW_VAddr((Address & ~3),&Value)) {
		return;
	}

	GPR[Opcode.rt].DW = (int32_t)(GPR[Opcode.rt].W[0] & LWR_MASK[Offset]);
	GPR[Opcode.rt].DW += (int32_t)(Value >> LWR_SHIFT[Offset]);
}

void r4300i_LWU (void) {
	uint32_t Address =  GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;
	if ((Address & 3) != 0) { ADDRESS_ERROR_EXCEPTION(Address,1); }
	if (Opcode.rt == 0) { return; }

	if (!r4300i_LW_VAddr(Address,&GPR[Opcode.rt].UW[0])) {
		//if (ShowTLBMisses) {
			DisplayError("LWU TLB: %X",Address);
		//}
		TLB_READ_EXCEPTION(Address);
	} else {
		GPR[Opcode.rt].UDW = GPR[Opcode.rt].UW[0];
	}
}

void r4300i_SB (void) {
	uint32_t Address =  GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;
	if (!r4300i_SB_VAddr(Address,GPR[Opcode.rt].UB[0])) {
	}
}

void r4300i_SH (void) {
	uint32_t Address =  GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;
	if ((Address & 1) != 0) { ADDRESS_ERROR_EXCEPTION(Address,0); }
	if (!r4300i_SH_VAddr(Address,GPR[Opcode.rt].UHW[0])) {
	}
}

uint32_t SWL_MASK[4] = { 0,0xFF000000,0xFFFF0000,0xFFFFFF00 };
int32_t SWL_SHIFT[4] = { 0, 8, 16, 24 };

void r4300i_SWL (void) {
	uint32_t Offset, Address, Value;

	Address = GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;
	Offset  = Address & 3;

	if (!r4300i_LW_VAddr((Address & ~3),&Value)) {
		return;
	}

	Value &= SWL_MASK[Offset];
	Value += GPR[Opcode.rt].UW[0] >> SWL_SHIFT[Offset];

	if (!r4300i_SW_VAddr((Address & ~0x03),Value)) {
	}
}


void r4300i_SW (void) {
	uint32_t Address =  GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;
	if ((Address & 3) != 0) { ADDRESS_ERROR_EXCEPTION(Address,0); }
	if (!r4300i_SW_VAddr(Address,GPR[Opcode.rt].UW[0])) {
	}
	//TranslateVaddr(&Address);
	//if (Address == 0x00090AA0) {
	//	LogMessage("%X: Write %X to %X",PROGRAM_COUNTER,GPR[Opcode.rt].UW[0],GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset);
	//}
}

uint64_t SDL_MASK[8] = { 0,0xFF00000000000000ULL,
						0xFFFF000000000000ULL,
						0xFFFFFF0000000000ULL,
						0xFFFFFFFF00000000ULL,
					    0xFFFFFFFFFF000000ULL,
						0xFFFFFFFFFFFF0000ULL,
						0xFFFFFFFFFFFFFF00ULL
					};
int32_t SDL_SHIFT[8] = { 0, 8, 16, 24, 32, 40, 48, 56 };

void r4300i_SDL (void) {
	uint32_t Offset, Address;
	uint64_t Value;

	Address = GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;
	Offset  = Address & 7;

	if (!r4300i_LD_VAddr((Address & ~7),&Value)) {
		return;
	}

	Value &= SDL_MASK[Offset];
	Value += GPR[Opcode.rt].UDW >> SDL_SHIFT[Offset];

	if (!r4300i_SD_VAddr((Address & ~7),Value)) {
	}
}

uint64_t SDR_MASK[8] = { 0x00FFFFFFFFFFFFFFULL,
					  0x0000FFFFFFFFFFFFULL,
					  0x000000FFFFFFFFFFULL,
					  0x00000000FFFFFFFFULL,
					  0x0000000000FFFFFFULL,
					  0x000000000000FFFFULL,
					  0x00000000000000FFULL,
					  0x0000000000000000ULL
					};
int32_t SDR_SHIFT[8] = { 56,48,40,32,24,16,8,0 };

void r4300i_SDR (void) {
	uint32_t Offset, Address;
	uint64_t Value;

	Address = GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;
	Offset  = Address & 7;

	if (!r4300i_LD_VAddr((Address & ~7),&Value)) {
		return;
	}

	Value &= SDR_MASK[Offset];
	Value += GPR[Opcode.rt].UDW << SDR_SHIFT[Offset];

	if (!r4300i_SD_VAddr((Address & ~7),Value)) {
	}
}

uint32_t SWR_MASK[4] = { 0x00FFFFFF,0x0000FFFF,0x000000FF,0x00000000 };
int32_t SWR_SHIFT[4] = { 24, 16 , 8, 0  };

void r4300i_SWR (void) {
	uint32_t Offset, Address, Value;

	Address = GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;
	Offset  = Address & 3;

	if (!r4300i_LW_VAddr((Address & ~3),&Value)) {
		return;
	}

	Value &= SWR_MASK[Offset];
	Value += GPR[Opcode.rt].UW[0] << SWR_SHIFT[Offset];

	if (!r4300i_SW_VAddr((Address & ~0x03),Value)) {
	}
}

void r4300i_CACHE (void) {
}

void r4300i_LL (void) {
	uint32_t Address =  GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;
	uintptr_t ll = 0;
	if ((Address & 3) != 0) { ADDRESS_ERROR_EXCEPTION(Address,1); }

	if (Opcode.rt == 0) { return; }

	if (!r4300i_LW_VAddr(Address,&GPR[Opcode.rt].UW[0])) {
		//if (ShowTLBMisses) {
			DisplayError("LW TLB: %X",Address);
		//}
		TLB_READ_EXCEPTION(Address);
	} else {
		GPR[Opcode.rt].DW = GPR[Opcode.rt].W[0];
	}
	LLBit = 1;
	LLAddr = Address;
	ll = LLAddr;
	TranslateVaddr(&ll);
	LLAddr = ll;

}

void r4300i_LWC1 (void) {
	uint32_t Address =  GPR[Opcode.base].UW[0] + (uint32_t)((int16_t)Opcode.offset);
	TEST_COP1_USABLE_EXCEPTION
	if ((Address & 3) != 0) { ADDRESS_ERROR_EXCEPTION(Address,1); }
	if (!r4300i_LW_VAddr(Address,&*(uint32_t *)FPRFloatLocation[Opcode.ft])) {
		//if (ShowTLBMisses) {
			DisplayError("LWC1 TLB: %X",Address);
		//}
		TLB_READ_EXCEPTION(Address);
	}
}

void r4300i_SC (void) {
	uint32_t Address =  GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;
	if ((Address & 3) != 0) { ADDRESS_ERROR_EXCEPTION(Address,0); }
	if (LLBit == 1) {
		if (!r4300i_SW_VAddr(Address,GPR[Opcode.rt].UW[0])) {
			DisplayError("SW TLB: %X",Address);
		}
	}
	GPR[Opcode.rt].UW[0] = LLBit;
}

void r4300i_LD (void) {
	uint32_t Address =  GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;
	if ((Address & 7) != 0) { ADDRESS_ERROR_EXCEPTION(Address,1); }
	if (!r4300i_LD_VAddr(Address,&GPR[Opcode.rt].UDW)) {
	}
}


void r4300i_LDC1 (void) {
	uint32_t Address =  GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;

	TEST_COP1_USABLE_EXCEPTION
	if ((Address & 7) != 0) { ADDRESS_ERROR_EXCEPTION(Address,1); }
	if (!r4300i_LD_VAddr(Address,&*(uint64_t *)FPRDoubleLocation[Opcode.ft])) {
	}
}

void r4300i_SWC1 (void) {
	uint32_t Address =  GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;
	TEST_COP1_USABLE_EXCEPTION
	if ((Address & 3) != 0) { ADDRESS_ERROR_EXCEPTION(Address,0); }

	if (!r4300i_SW_VAddr(Address,*(uint32_t *)FPRFloatLocation[Opcode.ft])) {
	}
}

void r4300i_SDC1 (void) {
	uint32_t Address =  GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;

	TEST_COP1_USABLE_EXCEPTION
	if ((Address & 7) != 0) { ADDRESS_ERROR_EXCEPTION(Address,0); }
	if (!r4300i_SD_VAddr(Address,*(int64_t *)FPRDoubleLocation[Opcode.ft])) {
	}
}

void r4300i_SD (void) {
	uint32_t Address =  GPR[Opcode.base].UW[0] + (int16_t)Opcode.offset;
	if ((Address & 7) != 0) { ADDRESS_ERROR_EXCEPTION(Address,0); }
	if (!r4300i_SD_VAddr(Address,GPR[Opcode.rt].UDW)) {
	}
}
/********************** R4300i OpCodes: Special **********************/
void r4300i_SPECIAL_SLL (void) {
	GPR[Opcode.rd].DW = (GPR[Opcode.rt].W[0] << Opcode.sa);
}

void r4300i_SPECIAL_SRL (void) {
	GPR[Opcode.rd].DW = (int32_t)(GPR[Opcode.rt].UW[0] >> Opcode.sa);
}

void r4300i_SPECIAL_SRA (void) {
	GPR[Opcode.rd].DW = (GPR[Opcode.rt].W[0] >> Opcode.sa);
}

void r4300i_SPECIAL_SLLV (void) {
	if (Opcode.rd == 0) { return; }
	GPR[Opcode.rd].DW = (GPR[Opcode.rt].W[0] << (GPR[Opcode.rs].UW[0] & 0x1F));
}

void r4300i_SPECIAL_SRLV (void) {
	GPR[Opcode.rd].DW = (int32_t)(GPR[Opcode.rt].UW[0] >> (GPR[Opcode.rs].UW[0] & 0x1F));
}

void r4300i_SPECIAL_SRAV (void) {
	GPR[Opcode.rd].DW = (GPR[Opcode.rt].W[0] >> (GPR[Opcode.rs].UW[0] & 0x1F));
}

void r4300i_SPECIAL_JR (void) {
	NextInstruction = DELAY_SLOT;
	JumpToLocation = GPR[Opcode.rs].UW[0];
}

void r4300i_SPECIAL_JALR (void) {
	NextInstruction = DELAY_SLOT;
	JumpToLocation = GPR[Opcode.rs].UW[0];
	GPR[Opcode.rd].DW = (int32_t)(PROGRAM_COUNTER + 8);
}

void r4300i_SPECIAL_SYSCALL (void) {
	DoSysCallException(NextInstruction == JUMP);
	NextInstruction = JUMP;
	JumpToLocation = PROGRAM_COUNTER;
}

void r4300i_SPECIAL_BREAK (void) {
	*WaitMode=1;
}

void r4300i_SPECIAL_SYNC (void) {
}

void r4300i_SPECIAL_MFHI (void) {
	GPR[Opcode.rd].DW = HI.DW;
}

void r4300i_SPECIAL_MTHI (void) {
	HI.DW = GPR[Opcode.rs].DW;
}

void r4300i_SPECIAL_MFLO (void) {
	GPR[Opcode.rd].DW = LO.DW;
}

void r4300i_SPECIAL_MTLO (void) {
	LO.DW = GPR[Opcode.rs].DW;
}

void r4300i_SPECIAL_DSLLV (void) {
	GPR[Opcode.rd].DW = GPR[Opcode.rt].DW << (GPR[Opcode.rs].UW[0] & 0x3F);
}

void r4300i_SPECIAL_DSRLV (void) {
	GPR[Opcode.rd].UDW = GPR[Opcode.rt].UDW >> (GPR[Opcode.rs].UW[0] & 0x3F);
}

void r4300i_SPECIAL_DSRAV (void) {
	GPR[Opcode.rd].DW = GPR[Opcode.rt].DW >> (GPR[Opcode.rs].UW[0] & 0x3F);
}

void r4300i_SPECIAL_MULT (void) {
	HI.DW = (int64_t)(GPR[Opcode.rs].W[0]) * (int64_t)(GPR[Opcode.rt].W[0]);
	LO.DW = HI.W[0];
	HI.DW = HI.W[1];
}

void r4300i_SPECIAL_MULTU (void) {
	HI.DW = (uint64_t)(GPR[Opcode.rs].UW[0]) * (uint64_t)(GPR[Opcode.rt].UW[0]);
	LO.DW = HI.W[0];
	HI.DW = HI.W[1];
}

void r4300i_SPECIAL_DIV (void) {
	if ( GPR[Opcode.rt].UDW != 0 ) {
		LO.DW = GPR[Opcode.rs].W[0] / GPR[Opcode.rt].W[0];
		HI.DW = GPR[Opcode.rs].W[0] % GPR[Opcode.rt].W[0];
	} else {
	}
}

void r4300i_SPECIAL_DIVU (void) {
	if ( GPR[Opcode.rt].UDW != 0 ) {
		LO.DW = (int32_t)(GPR[Opcode.rs].UW[0] / GPR[Opcode.rt].UW[0]);
		HI.DW = (int32_t)(GPR[Opcode.rs].UW[0] % GPR[Opcode.rt].UW[0]);
	} else {
	}
}

void r4300i_SPECIAL_DMULT (void) {
	MIPS_DWORD Tmp[3];

	LO.UDW = (uint64_t)GPR[Opcode.rs].UW[0] * (uint64_t)GPR[Opcode.rt].UW[0];
	Tmp[0].UDW = (int64_t)GPR[Opcode.rs].W[1] * (int64_t)(uint64_t)GPR[Opcode.rt].UW[0];
	Tmp[1].UDW = (int64_t)(uint64_t)GPR[Opcode.rs].UW[0] * (int64_t)GPR[Opcode.rt].W[1];
	HI.UDW = (int64_t)GPR[Opcode.rs].W[1] * (int64_t)GPR[Opcode.rt].W[1];

	Tmp[2].UDW = (uint64_t)LO.UW[1] + (uint64_t)Tmp[0].UW[0] + (uint64_t)Tmp[1].UW[0];
	LO.UDW += ((uint64_t)Tmp[0].UW[0] + (uint64_t)Tmp[1].UW[0]) << 32;
	HI.UDW += (uint64_t)Tmp[0].W[1] + (uint64_t)Tmp[1].W[1] + Tmp[2].UW[1];
}

void r4300i_SPECIAL_DMULTU (void) {
	MIPS_DWORD Tmp[3];

	LO.UDW = (uint64_t)GPR[Opcode.rs].UW[0] * (uint64_t)GPR[Opcode.rt].UW[0];
	Tmp[0].UDW = (uint64_t)GPR[Opcode.rs].UW[1] * (uint64_t)GPR[Opcode.rt].UW[0];
	Tmp[1].UDW = (uint64_t)GPR[Opcode.rs].UW[0] * (uint64_t)GPR[Opcode.rt].UW[1];
	HI.UDW = (uint64_t)GPR[Opcode.rs].UW[1] * (uint64_t)GPR[Opcode.rt].UW[1];

	Tmp[2].UDW = (uint64_t)LO.UW[1] + (uint64_t)Tmp[0].UW[0] + (uint64_t)Tmp[1].UW[0];
	LO.UDW += ((uint64_t)Tmp[0].UW[0] + (uint64_t)Tmp[1].UW[0]) << 32;
	HI.UDW += (uint64_t)Tmp[0].UW[1] + (uint64_t)Tmp[1].UW[1] + Tmp[2].UW[1];
}

void r4300i_SPECIAL_DDIV (void) {
	if ( GPR[Opcode.rt].UDW != 0 ) {
		LO.DW = GPR[Opcode.rs].DW / GPR[Opcode.rt].DW;
		HI.DW = GPR[Opcode.rs].DW % GPR[Opcode.rt].DW;
	} else {
	}
}

void r4300i_SPECIAL_DDIVU (void) {
	if ( GPR[Opcode.rt].UDW != 0 ) {
		LO.UDW = GPR[Opcode.rs].UDW / GPR[Opcode.rt].UDW;
		HI.UDW = GPR[Opcode.rs].UDW % GPR[Opcode.rt].UDW;
	} else {
	}
}

void r4300i_SPECIAL_ADD (void) {
	GPR[Opcode.rd].DW = GPR[Opcode.rs].W[0] + GPR[Opcode.rt].W[0];
}

void r4300i_SPECIAL_ADDU (void) {
	GPR[Opcode.rd].DW = GPR[Opcode.rs].W[0] + GPR[Opcode.rt].W[0];
}

void r4300i_SPECIAL_SUB (void) {
	GPR[Opcode.rd].DW = GPR[Opcode.rs].W[0] - GPR[Opcode.rt].W[0];
}

void r4300i_SPECIAL_SUBU (void) {
	GPR[Opcode.rd].DW = GPR[Opcode.rs].W[0] - GPR[Opcode.rt].W[0];
}

void r4300i_SPECIAL_AND (void) {
	GPR[Opcode.rd].DW = GPR[Opcode.rs].DW & GPR[Opcode.rt].DW;
}

void r4300i_SPECIAL_OR (void) {
	GPR[Opcode.rd].DW = GPR[Opcode.rs].DW | GPR[Opcode.rt].DW;
}

void r4300i_SPECIAL_XOR (void) {
	GPR[Opcode.rd].DW = GPR[Opcode.rs].DW ^ GPR[Opcode.rt].DW;
}

void r4300i_SPECIAL_NOR (void) {
	GPR[Opcode.rd].DW = ~(GPR[Opcode.rs].DW | GPR[Opcode.rt].DW);
}

void r4300i_SPECIAL_SLT (void) {
	if (GPR[Opcode.rs].DW < GPR[Opcode.rt].DW) {
		GPR[Opcode.rd].DW = 1;
	} else {
		GPR[Opcode.rd].DW = 0;
	}
}

void r4300i_SPECIAL_SLTU (void) {
	if (GPR[Opcode.rs].UDW < GPR[Opcode.rt].UDW) {
		GPR[Opcode.rd].DW = 1;
	} else {
		GPR[Opcode.rd].DW = 0;
	}
}

void r4300i_SPECIAL_DADD (void) {
	GPR[Opcode.rd].DW = GPR[Opcode.rs].DW + GPR[Opcode.rt].DW;
}

void r4300i_SPECIAL_DADDU (void) {
	GPR[Opcode.rd].DW = GPR[Opcode.rs].DW + GPR[Opcode.rt].DW;
}

void r4300i_SPECIAL_DSUB (void) {
	GPR[Opcode.rd].DW = GPR[Opcode.rs].DW - GPR[Opcode.rt].DW;
}

void r4300i_SPECIAL_DSUBU (void) {
	GPR[Opcode.rd].DW = GPR[Opcode.rs].DW - GPR[Opcode.rt].DW;
}

void r4300i_SPECIAL_TEQ (void) {
	if (GPR[Opcode.rs].DW == GPR[Opcode.rt].DW) {
	}
}

void r4300i_SPECIAL_DSLL (void) {
	GPR[Opcode.rd].DW = (GPR[Opcode.rt].DW << Opcode.sa);
}

void r4300i_SPECIAL_DSRL (void) {
	GPR[Opcode.rd].UDW = (GPR[Opcode.rt].UDW >> Opcode.sa);
}

void r4300i_SPECIAL_DSRA (void) {
	GPR[Opcode.rd].DW = (GPR[Opcode.rt].DW >> Opcode.sa);
}

void r4300i_SPECIAL_DSLL32 (void) {
	GPR[Opcode.rd].DW = (GPR[Opcode.rt].DW << (Opcode.sa + 32));
}

void r4300i_SPECIAL_DSRL32 (void) {
   GPR[Opcode.rd].UDW = (GPR[Opcode.rt].UDW >> (Opcode.sa + 32));
}

void r4300i_SPECIAL_DSRA32 (void) {
	GPR[Opcode.rd].DW = (GPR[Opcode.rt].DW >> (Opcode.sa + 32));
}

/********************** R4300i OpCodes: RegImm **********************/
void r4300i_REGIMM_BLTZ (void) {
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.rs].DW < 0) {
		JumpToLocation = PROGRAM_COUNTER + ((int16_t)Opcode.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.rs,0);
	} else {
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void r4300i_REGIMM_BGEZ (void) {
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.rs].DW >= 0) {
		JumpToLocation = PROGRAM_COUNTER + ((int16_t)Opcode.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.rs,0);
	} else {
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void r4300i_REGIMM_BLTZL (void) {
	if (GPR[Opcode.rs].DW < 0) {
		NextInstruction = DELAY_SLOT;
		JumpToLocation = PROGRAM_COUNTER + ((int16_t)Opcode.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.rs,0);
	} else {
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void r4300i_REGIMM_BGEZL (void) {
	if (GPR[Opcode.rs].DW >= 0) {
		NextInstruction = DELAY_SLOT;
		JumpToLocation = PROGRAM_COUNTER + ((int16_t)Opcode.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.rs,0);
	} else {
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void r4300i_REGIMM_BLTZAL (void) {
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.rs].DW < 0) {
		JumpToLocation = PROGRAM_COUNTER + ((int16_t)Opcode.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.rs,0);
	} else {
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
	GPR[31].DW= (int32_t)(PROGRAM_COUNTER + 8);
}

void r4300i_REGIMM_BGEZAL (void) {
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.rs].DW >= 0) {
		JumpToLocation = PROGRAM_COUNTER + ((int16_t)Opcode.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.rs,0);
	} else {
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
	GPR[31].DW = (int32_t)(PROGRAM_COUNTER + 8);
}
/************************** COP0 functions **************************/
void r4300i_COP0_MF (void) {
	GPR[Opcode.rt].DW = (int32_t)CP0[Opcode.rd];
}

void r4300i_COP0_MT (void) {
	switch (Opcode.rd) {
	case 0: //Index
	case 2: //EntryLo0
	case 3: //EntryLo1
	case 5: //PageMask
	case 6: //Wired
	case 10: //Entry Hi
	case 14: //EPC
	case 16: //Config
	case 18: //WatchLo
	case 19: //WatchHi
	case 28: //Tag lo
	case 29: //Tag Hi
	case 30: //ErrEPC
		CP0[Opcode.rd] = GPR[Opcode.rt].UW[0];
		break;
	case 4: //Context
		CP0[Opcode.rd] = GPR[Opcode.rt].UW[0] & 0xFF800000;
		break;
	case 9: //Count
		CP0[Opcode.rd]= GPR[Opcode.rt].UW[0];
		ChangeCompareTimer();
		break;
	case 11: //Compare
		CP0[Opcode.rd] = GPR[Opcode.rt].UW[0];
		FAKE_CAUSE_REGISTER &= ~CAUSE_IP7;
		ChangeCompareTimer();
		break;
	case 12: //Status
		if ((CP0[Opcode.rd] ^ GPR[Opcode.rt].UW[0]) != 0) {
			CP0[Opcode.rd] = GPR[Opcode.rt].UW[0];
			SetFpuLocations();
		} else {
			CP0[Opcode.rd] = GPR[Opcode.rt].UW[0];
		}
		if ((CP0[Opcode.rd] & 0x18) != 0) {
		}
		CheckInterrupts();
		break;
	case 13: //cause
		CP0[Opcode.rd] &= 0xFFFFCFF;
		break;
	default:
		R4300i_UnknownOpcode();
	}

}

/************************** COP0 CO functions ***********************/
void r4300i_COP0_CO_TLBR (void) {
	TLB_Read();
}

void r4300i_COP0_CO_TLBWI (void) {
	WriteTLBEntry(INDEX_REGISTER & 0x1F);
}

void r4300i_COP0_CO_TLBWR (void) {
	WriteTLBEntry(RANDOM_REGISTER & 0x1F);
}

void r4300i_COP0_CO_TLBP (void) {
	TLB_Probe();
}

void r4300i_COP0_CO_ERET (void) {
	NextInstruction = JUMP;
	if ((STATUS_REGISTER & STATUS_ERL) != 0) {
		JumpToLocation = ERROREPC_REGISTER;
		STATUS_REGISTER &= ~STATUS_ERL;
	} else {
		JumpToLocation = EPC_REGISTER;
		STATUS_REGISTER &= ~STATUS_EXL;
	}
	LLBit = 0;
	CheckInterrupts();
}

/************************** COP1 functions **************************/
void r4300i_COP1_MF (void) {
	TEST_COP1_USABLE_EXCEPTION
	GPR[Opcode.rt].DW = *(int32_t *)FPRFloatLocation[Opcode.fs];
}

void r4300i_COP1_DMF (void) {
	TEST_COP1_USABLE_EXCEPTION
	GPR[Opcode.rt].DW = *(int64_t *)FPRDoubleLocation[Opcode.fs];
}

void r4300i_COP1_CF (void) {
	TEST_COP1_USABLE_EXCEPTION
	if (Opcode.fs != 31 && Opcode.fs != 0) {
		return;
	}
	GPR[Opcode.rt].DW = (int32_t)FPCR[Opcode.fs];
}

void r4300i_COP1_MT (void) {
	TEST_COP1_USABLE_EXCEPTION
	*(int32_t *)FPRFloatLocation[Opcode.fs] = GPR[Opcode.rt].W[0];
}

void r4300i_COP1_DMT (void) {
	TEST_COP1_USABLE_EXCEPTION
	*(int64_t *)FPRDoubleLocation[Opcode.fs] = GPR[Opcode.rt].DW;
}

void r4300i_COP1_CT (void) {
	TEST_COP1_USABLE_EXCEPTION
	if (Opcode.fs == 31) {
		FPCR[Opcode.fs] = GPR[Opcode.rt].W[0];
		switch((FPCR[Opcode.fs] & 3)) {
		case 0: RoundingModel = _FPU_RC_NEAREST; break;
		case 1: RoundingModel = _FPU_RC_ZERO; break;
		case 2: RoundingModel = _FPU_RC_UP; break;
		case 3: RoundingModel = _FPU_RC_DOWN; break;
		}
		return;
	}
}

/************************* COP1: BC1 functions ***********************/
void r4300i_COP1_BCF (void) {
	TEST_COP1_USABLE_EXCEPTION
	NextInstruction = DELAY_SLOT;
	if ((FPCR[31] & FPCSR_C) == 0) {
		JumpToLocation = PROGRAM_COUNTER + ((int16_t)Opcode.offset << 2) + 4;
	} else {
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void r4300i_COP1_BCT (void) {
	TEST_COP1_USABLE_EXCEPTION
	NextInstruction = DELAY_SLOT;
	if ((FPCR[31] & FPCSR_C) != 0) {
		JumpToLocation = PROGRAM_COUNTER + ((int16_t)Opcode.offset << 2) + 4;
	} else {
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void r4300i_COP1_BCFL (void) {
	TEST_COP1_USABLE_EXCEPTION
	if ((FPCR[31] & FPCSR_C) == 0) {
		NextInstruction = DELAY_SLOT;
		JumpToLocation = PROGRAM_COUNTER + ((int16_t)Opcode.offset << 2) + 4;
	} else {
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void r4300i_COP1_BCTL (void) {
	TEST_COP1_USABLE_EXCEPTION
	if ((FPCR[31] & FPCSR_C) != 0) {
		NextInstruction = DELAY_SLOT;
		JumpToLocation = PROGRAM_COUNTER + ((int16_t)Opcode.offset << 2) + 4;
	} else {
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}
/************************** COP1: S functions ************************/
__inline void Float_RoundToInteger32( int32_t * Dest, float * Source ) {
	*Dest = (int32_t)*Source;
}

__inline void Float_RoundToInteger64( int64_t * Dest, float * Source ) {
	*Dest = (int64_t)*Source;
}

void r4300i_COP1_S_ADD (void) {
	TEST_COP1_USABLE_EXCEPTION
	controlfp(RoundingModel);
	*(float *)FPRFloatLocation[Opcode.fd] = (*(float *)FPRFloatLocation[Opcode.fs] + *(float *)FPRFloatLocation[Opcode.ft]);
}

void r4300i_COP1_S_SUB (void) {
	TEST_COP1_USABLE_EXCEPTION
	controlfp(RoundingModel);
	*(float *)FPRFloatLocation[Opcode.fd] = (*(float *)FPRFloatLocation[Opcode.fs] - *(float *)FPRFloatLocation[Opcode.ft]);
}

void r4300i_COP1_S_MUL (void) {
	TEST_COP1_USABLE_EXCEPTION
	controlfp(RoundingModel);
	*(float *)FPRFloatLocation[Opcode.fd] = (*(float *)FPRFloatLocation[Opcode.fs] * *(float *)FPRFloatLocation[Opcode.ft]);
}

void r4300i_COP1_S_DIV (void) {
	TEST_COP1_USABLE_EXCEPTION
	controlfp(RoundingModel);
	*(float *)FPRFloatLocation[Opcode.fd] = (*(float *)FPRFloatLocation[Opcode.fs] / *(float *)FPRFloatLocation[Opcode.ft]);
}

void r4300i_COP1_S_SQRT (void) {
	TEST_COP1_USABLE_EXCEPTION
	controlfp(RoundingModel);
	*(float *)FPRFloatLocation[Opcode.fd] = (float)sqrt(*(float *)FPRFloatLocation[Opcode.fs]);
}

void r4300i_COP1_S_ABS (void) {
	TEST_COP1_USABLE_EXCEPTION
	controlfp(RoundingModel);
	*(float *)FPRFloatLocation[Opcode.fd] = (float)fabs(*(float *)FPRFloatLocation[Opcode.fs]);
}

void r4300i_COP1_S_MOV (void) {
	TEST_COP1_USABLE_EXCEPTION
	controlfp(RoundingModel);
	*(float *)FPRFloatLocation[Opcode.fd] = *(float *)FPRFloatLocation[Opcode.fs];
}

void r4300i_COP1_S_NEG (void) {
	TEST_COP1_USABLE_EXCEPTION
	controlfp(RoundingModel);
	*(float *)FPRFloatLocation[Opcode.fd] = (*(float *)FPRFloatLocation[Opcode.fs] * -1.0f);
}

void r4300i_COP1_S_TRUNC_L (void) {
	TEST_COP1_USABLE_EXCEPTION
	//_controlfp(_RC_CHOP,_MCW_RC);
	Float_RoundToInteger64(&*(int64_t *)FPRDoubleLocation[Opcode.fd],&*(float *)FPRFloatLocation[Opcode.fs]);
}

void r4300i_COP1_S_CEIL_L (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION
	//_controlfp(_RC_UP,_MCW_RC);
	Float_RoundToInteger64(&*(int64_t *)FPRDoubleLocation[Opcode.fd],&*(float *)FPRFloatLocation[Opcode.fs]);
}

void r4300i_COP1_S_FLOOR_L (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION
	//_controlfp(_FPU_RC_DOWN,_MCW_RC);
	Float_RoundToInteger64(&*(int64_t *)FPRDoubleLocation[Opcode.fd],&*(float *)FPRFloatLocation[Opcode.fs]);
}

void r4300i_COP1_S_ROUND_W (void) {
	TEST_COP1_USABLE_EXCEPTION
	//_controlfp(_FPU_RC_NEAREST,_MCW_RC);
	Float_RoundToInteger32(&*(int32_t *)FPRFloatLocation[Opcode.fd],&*(float *)FPRFloatLocation[Opcode.fs]);
}

void r4300i_COP1_S_TRUNC_W (void) {
	TEST_COP1_USABLE_EXCEPTION
	//_controlfp(_RC_CHOP,_MCW_RC);
	Float_RoundToInteger32(&*(int32_t *)FPRFloatLocation[Opcode.fd],&*(float *)FPRFloatLocation[Opcode.fs]);
}

void r4300i_COP1_S_CEIL_W (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION
	//_controlfp(_RC_UP,_MCW_RC);
	Float_RoundToInteger32(&*(int32_t *)FPRFloatLocation[Opcode.fd],&*(float *)FPRFloatLocation[Opcode.fs]);
}

void r4300i_COP1_S_FLOOR_W (void) {
	TEST_COP1_USABLE_EXCEPTION
	//_controlfp(_FPU_RC_DOWN,_MCW_RC);
	Float_RoundToInteger32(&*(int32_t *)FPRFloatLocation[Opcode.fd],&*(float *)FPRFloatLocation[Opcode.fs]);
}

void r4300i_COP1_S_CVT_D (void) {
	TEST_COP1_USABLE_EXCEPTION
	controlfp(RoundingModel);
	*(double *)FPRDoubleLocation[Opcode.fd] = (double)(*(float *)FPRFloatLocation[Opcode.fs]);
}

void r4300i_COP1_S_CVT_W (void) {
	TEST_COP1_USABLE_EXCEPTION
	controlfp(RoundingModel);
	Float_RoundToInteger32(&*(int32_t *)FPRFloatLocation[Opcode.fd],&*(float *)FPRFloatLocation[Opcode.fs]);
}

void r4300i_COP1_S_CVT_L (void) {
	TEST_COP1_USABLE_EXCEPTION
	controlfp(RoundingModel);
	Float_RoundToInteger64(&*(int64_t *)FPRDoubleLocation[Opcode.fd],&*(float *)FPRFloatLocation[Opcode.fs]);
}

void r4300i_COP1_S_CMP (void) {
	int32_t less, equal, unorded, condition;
	float Temp0, Temp1;

	TEST_COP1_USABLE_EXCEPTION

	Temp0 = *(float *)FPRFloatLocation[Opcode.fs];
	Temp1 = *(float *)FPRFloatLocation[Opcode.ft];

	if(0) {
	//if (_isnan(Temp0) || _isnan(Temp1)) {
		less = 0;
		equal = 0;
		unorded = 1;
		if ((Opcode.funct & 8) != 0) {
		}
	} else {
		less = Temp0 < Temp1;
		equal = Temp0 == Temp1;
		unorded = 0;
	}

	condition = ((Opcode.funct & 4) && less) | ((Opcode.funct & 2) && equal) |
		((Opcode.funct & 1) && unorded);

	if (condition) {
		FPCR[31] |= FPCSR_C;
	} else {
		FPCR[31] &= ~FPCSR_C;
	}

}

/************************** COP1: D functions ************************/
__inline void Double_RoundToInteger32( int32_t * Dest, double * Source ) {
	*Dest = (int32_t)*Source;
}

__inline void Double_RoundToInteger64( int64_t * Dest, double * Source ) {
	*Dest = (int64_t)*Source;
}

void r4300i_COP1_D_ADD (void) {
	TEST_COP1_USABLE_EXCEPTION
	*(double *)FPRDoubleLocation[Opcode.fd] = *(double *)FPRDoubleLocation[Opcode.fs] + *(double *)FPRDoubleLocation[Opcode.ft];
}

void r4300i_COP1_D_SUB (void) {
	TEST_COP1_USABLE_EXCEPTION
	*(double *)FPRDoubleLocation[Opcode.fd] = *(double *)FPRDoubleLocation[Opcode.fs] - *(double *)FPRDoubleLocation[Opcode.ft];
}

void r4300i_COP1_D_MUL (void) {
	TEST_COP1_USABLE_EXCEPTION
	*(double *)FPRDoubleLocation[Opcode.fd] = *(double *)FPRDoubleLocation[Opcode.fs] * *(double *)FPRDoubleLocation[Opcode.ft];
}

void r4300i_COP1_D_DIV (void) {
	TEST_COP1_USABLE_EXCEPTION
	*(double *)FPRDoubleLocation[Opcode.fd] = *(double *)FPRDoubleLocation[Opcode.fs] / *(double *)FPRDoubleLocation[Opcode.ft];
}

void r4300i_COP1_D_SQRT (void) {
	TEST_COP1_USABLE_EXCEPTION
	*(double *)FPRDoubleLocation[Opcode.fd] = (double)sqrt(*(double *)FPRDoubleLocation[Opcode.fs]);
}

void r4300i_COP1_D_ABS (void) {
	TEST_COP1_USABLE_EXCEPTION
	*(double *)FPRDoubleLocation[Opcode.fd] = fabs(*(double *)FPRDoubleLocation[Opcode.fs]);
}

void r4300i_COP1_D_MOV (void) {
	TEST_COP1_USABLE_EXCEPTION
	*(int64_t *)FPRDoubleLocation[Opcode.fd] = *(int64_t *)FPRDoubleLocation[Opcode.fs];
}

void r4300i_COP1_D_NEG (void) {
	TEST_COP1_USABLE_EXCEPTION
	*(double *)FPRDoubleLocation[Opcode.fd] = (*(double *)FPRDoubleLocation[Opcode.fs] * -1.0);
}

void r4300i_COP1_D_TRUNC_L (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION
	controlfp(_FPU_RC_ZERO);
	Double_RoundToInteger64(&*(int64_t *)FPRFloatLocation[Opcode.fd],&*(double *)FPRDoubleLocation[Opcode.fs] );
}

void r4300i_COP1_D_CEIL_L (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION
	controlfp(_FPU_RC_UP);
	Double_RoundToInteger64(&*(int64_t *)FPRFloatLocation[Opcode.fd],&*(double *)FPRDoubleLocation[Opcode.fs] );
}

void r4300i_COP1_D_FLOOR_L (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION
	controlfp(_FPU_RC_DOWN);
	Double_RoundToInteger64(&*(int64_t *)FPRDoubleLocation[Opcode.fd],&*(double *)FPRFloatLocation[Opcode.fs]);
}

void r4300i_COP1_D_ROUND_W (void) {
	TEST_COP1_USABLE_EXCEPTION
	controlfp(_FPU_RC_NEAREST);
	Double_RoundToInteger32(&*(int32_t *)FPRFloatLocation[Opcode.fd],&*(double *)FPRDoubleLocation[Opcode.fs] );
}

void r4300i_COP1_D_TRUNC_W (void) {
	TEST_COP1_USABLE_EXCEPTION
	controlfp(_FPU_RC_ZERO);
	Double_RoundToInteger32(&*(int32_t *)FPRFloatLocation[Opcode.fd],&*(double *)FPRDoubleLocation[Opcode.fs] );
}

void r4300i_COP1_D_CEIL_W (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION
	controlfp(_FPU_RC_UP);
	Double_RoundToInteger32(&*(int32_t *)FPRFloatLocation[Opcode.fd],&*(double *)FPRDoubleLocation[Opcode.fs] );
}

void r4300i_COP1_D_FLOOR_W (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION
	controlfp(_FPU_RC_DOWN);
	Double_RoundToInteger32(&*(int32_t *)FPRDoubleLocation[Opcode.fd],&*(double *)FPRFloatLocation[Opcode.fs]);
}

void r4300i_COP1_D_CVT_S (void) {
	TEST_COP1_USABLE_EXCEPTION
	controlfp(RoundingModel);
	*(float *)FPRFloatLocation[Opcode.fd] = (float)*(double *)FPRDoubleLocation[Opcode.fs];
}

void r4300i_COP1_D_CVT_W (void) {
	TEST_COP1_USABLE_EXCEPTION
	controlfp(RoundingModel);
	Double_RoundToInteger32(&*(int32_t *)FPRFloatLocation[Opcode.fd],&*(double *)FPRDoubleLocation[Opcode.fs] );
}

void r4300i_COP1_D_CVT_L (void) {
	TEST_COP1_USABLE_EXCEPTION
	controlfp(RoundingModel);
	Double_RoundToInteger64(&*(int64_t *)FPRDoubleLocation[Opcode.fd],&*(double *)FPRDoubleLocation[Opcode.fs]);
}

void r4300i_COP1_D_CMP (void) {
	int32_t less, equal, unorded, condition;
	MIPS_DWORD Temp0, Temp1;

	TEST_COP1_USABLE_EXCEPTION

	Temp0.DW = *(int64_t *)FPRDoubleLocation[Opcode.fs];
	Temp1.DW = *(int64_t *)FPRDoubleLocation[Opcode.ft];

	if(0) {
	//if (_isnan(Temp0.D) || _isnan(Temp1.D)) {
		less = 0;
		equal = 0;
		unorded = 1;
		if ((Opcode.funct & 8) != 0) {
		}
	} else {
		less = Temp0.D < Temp1.D;
		equal = Temp0.D == Temp1.D;
		unorded = 0;
	}

	condition = ((Opcode.funct & 4) && less) | ((Opcode.funct & 2) && equal) |
		((Opcode.funct & 1) && unorded);

	if (condition) {
		FPCR[31] |= FPCSR_C;
	} else {
		FPCR[31] &= ~FPCSR_C;
	}
}

/************************** COP1: W functions ************************/
void r4300i_COP1_W_CVT_S (void) {
	TEST_COP1_USABLE_EXCEPTION
	controlfp(RoundingModel);
	*(float *)FPRFloatLocation[Opcode.fd] = (float)*(int32_t *)FPRFloatLocation[Opcode.fs];
}

void r4300i_COP1_W_CVT_D (void) {
	TEST_COP1_USABLE_EXCEPTION
	controlfp(RoundingModel);
	*(double *)FPRDoubleLocation[Opcode.fd] = (double)*(int32_t *)FPRFloatLocation[Opcode.fs];
}

/************************** COP1: L functions ************************/
void r4300i_COP1_L_CVT_S (void) {
	TEST_COP1_USABLE_EXCEPTION
	controlfp(RoundingModel);
	*(float *)FPRFloatLocation[Opcode.fd] = (float)*(int64_t *)FPRDoubleLocation[Opcode.fs];
}

void r4300i_COP1_L_CVT_D (void) {
	TEST_COP1_USABLE_EXCEPTION
	controlfp(RoundingModel);
	*(double *)FPRDoubleLocation[Opcode.fd] = (double)*(int64_t *)FPRDoubleLocation[Opcode.fs];
}

/************************** Other functions **************************/
void R4300i_UnknownOpcode (void) {
	DisplayError("Unkniown X86 Opcode.\tPC:%08x\tOp:%08x\n", PROGRAM_COUNTER,Opcode.Hex);
	StopEmulation();
}
