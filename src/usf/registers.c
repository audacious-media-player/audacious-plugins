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

// #ifdef EXT_REGS

#include "main.h"
#include "cpu.h"
#include "x86.h"
#include "types.h"

uint32_t PROGRAM_COUNTER, * CP0,*FPCR,*RegRDRAM,*RegSP,*RegDPC,*RegMI,*RegVI,*RegAI,*RegPI,
	*RegRI,*RegSI, HalfLine, RegModValue, ViFieldNumber, LLBit, LLAddr;
void * FPRDoubleLocation[32], * FPRFloatLocation[32];
MIPS_DWORD *GPR, *FPR, HI, LO;
int32_t fpuControl;
N64_REGISTERS * Registers = 0;

void SetupRegisters(N64_REGISTERS * n64_Registers) {
	PROGRAM_COUNTER = n64_Registers->PROGRAM_COUNTER;
	HI.DW    = n64_Registers->HI.DW;
	LO.DW    = n64_Registers->LO.DW;
	CP0      = n64_Registers->CP0;
	GPR      = n64_Registers->GPR;
	FPR      = n64_Registers->FPR;
	FPCR     = n64_Registers->FPCR;
	RegRDRAM = n64_Registers->RDRAM;
	RegSP    = n64_Registers->SP;
	RegDPC   = n64_Registers->DPC;
	RegMI    = n64_Registers->MI;
	RegVI    = n64_Registers->VI;
	RegAI    = n64_Registers->AI;
	RegPI    = n64_Registers->PI;
	RegRI    = n64_Registers->RI;
	RegSI    = n64_Registers->SI;
	PIF_Ram  = n64_Registers->PIF_Ram;
}

int  fUnMap_8BitTempReg (BLOCK_SECTION * Section);
int  UnMap_TempReg     (BLOCK_SECTION * Section);
uint32_t UnMap_X86reg      (BLOCK_SECTION * Section, uint32_t x86Reg);

void ChangeFPURegFormat (BLOCK_SECTION * Section, int32_t Reg, int32_t OldFormat, int32_t NewFormat, int32_t RoundingModel) {
	int32_t i;

	for (i = 0; i < 8; i++) {
		if (FpuMappedTo(i) == (uint32_t)Reg) {
			if (FpuState(i) != (uint32_t)OldFormat) {
				UnMap_FPR(Section,Reg,1);
				Load_FPR_ToTop(Section,Reg,Reg,OldFormat);
				ChangeFPURegFormat(Section,Reg,OldFormat,NewFormat,RoundingModel);
				return;
			}
			FpuRoundingModel(i) = RoundingModel;
			FpuState(i)         = NewFormat;
			return;
		}
	}

}

void ChangeMiIntrMask (void) {
	if ( ( RegModValue & MI_INTR_MASK_CLR_SP ) != 0 ) { MI_INTR_MASK_REG &= ~MI_INTR_MASK_SP; }
	if ( ( RegModValue & MI_INTR_MASK_SET_SP ) != 0 ) { MI_INTR_MASK_REG |= MI_INTR_MASK_SP; }
	if ( ( RegModValue & MI_INTR_MASK_CLR_SI ) != 0 ) { MI_INTR_MASK_REG &= ~MI_INTR_MASK_SI; }
	if ( ( RegModValue & MI_INTR_MASK_SET_SI ) != 0 ) { MI_INTR_MASK_REG |= MI_INTR_MASK_SI; }
	if ( ( RegModValue & MI_INTR_MASK_CLR_AI ) != 0 ) { MI_INTR_MASK_REG &= ~MI_INTR_MASK_AI; }
	if ( ( RegModValue & MI_INTR_MASK_SET_AI ) != 0 ) { MI_INTR_MASK_REG |= MI_INTR_MASK_AI; }
	if ( ( RegModValue & MI_INTR_MASK_CLR_VI ) != 0 ) { MI_INTR_MASK_REG &= ~MI_INTR_MASK_VI; }
	if ( ( RegModValue & MI_INTR_MASK_SET_VI ) != 0 ) { MI_INTR_MASK_REG |= MI_INTR_MASK_VI; }
	if ( ( RegModValue & MI_INTR_MASK_CLR_PI ) != 0 ) { MI_INTR_MASK_REG &= ~MI_INTR_MASK_PI; }
	if ( ( RegModValue & MI_INTR_MASK_SET_PI ) != 0 ) { MI_INTR_MASK_REG |= MI_INTR_MASK_PI; }
	if ( ( RegModValue & MI_INTR_MASK_CLR_DP ) != 0 ) { MI_INTR_MASK_REG &= ~MI_INTR_MASK_DP; }
	if ( ( RegModValue & MI_INTR_MASK_SET_DP ) != 0 ) { MI_INTR_MASK_REG |= MI_INTR_MASK_DP; }
}

void ChangeMiModeReg (void) {
	MI_MODE_REG &= ~0x7F;
	MI_MODE_REG |= (RegModValue & 0x7F);
	if ( ( RegModValue & MI_CLR_INIT ) != 0 ) { MI_MODE_REG &= ~MI_MODE_INIT; }
	if ( ( RegModValue & MI_SET_INIT ) != 0 ) { MI_MODE_REG |= MI_MODE_INIT; }
	if ( ( RegModValue & MI_CLR_EBUS ) != 0 ) { MI_MODE_REG &= ~MI_MODE_EBUS; }
	if ( ( RegModValue & MI_SET_EBUS ) != 0 ) { MI_MODE_REG |= MI_MODE_EBUS; }
	if ( ( RegModValue & MI_CLR_DP_INTR ) != 0 ) { MI_INTR_REG &= ~MI_INTR_DP; }
	if ( ( RegModValue & MI_CLR_RDRAM ) != 0 ) { MI_MODE_REG &= ~MI_MODE_RDRAM; }
	if ( ( RegModValue & MI_SET_RDRAM ) != 0 ) { MI_MODE_REG |= MI_MODE_RDRAM; }
}

void ChangeSpStatus (void) {
	if ( ( RegModValue & SP_CLR_HALT ) != 0) { SP_STATUS_REG &= ~SP_STATUS_HALT; }
	if ( ( RegModValue & SP_SET_HALT ) != 0) { SP_STATUS_REG |= SP_STATUS_HALT;  }
	if ( ( RegModValue & SP_CLR_BROKE ) != 0) { SP_STATUS_REG &= ~SP_STATUS_BROKE; }
	if ( ( RegModValue & SP_CLR_INTR ) != 0) {
		MI_INTR_REG &= ~MI_INTR_SP;
		CheckInterrupts();
	}

	if ( ( RegModValue & SP_CLR_SSTEP ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SSTEP; }
	if ( ( RegModValue & SP_SET_SSTEP ) != 0) { SP_STATUS_REG |= SP_STATUS_SSTEP;  }
	if ( ( RegModValue & SP_CLR_INTR_BREAK ) != 0) { SP_STATUS_REG &= ~SP_STATUS_INTR_BREAK; }
	if ( ( RegModValue & SP_SET_INTR_BREAK ) != 0) { SP_STATUS_REG |= SP_STATUS_INTR_BREAK;  }
	if ( ( RegModValue & SP_CLR_SIG0 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG0; }
	if ( ( RegModValue & SP_SET_SIG0 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG0;  }
	if ( ( RegModValue & SP_CLR_SIG1 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG1; }
	if ( ( RegModValue & SP_SET_SIG1 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG1;  }
	if ( ( RegModValue & SP_CLR_SIG2 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG2; }
	if ( ( RegModValue & SP_SET_SIG2 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG2;  }
	if ( ( RegModValue & SP_CLR_SIG3 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG3; }
	if ( ( RegModValue & SP_SET_SIG3 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG3;  }
	if ( ( RegModValue & SP_CLR_SIG4 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG4; }
	if ( ( RegModValue & SP_SET_SIG4 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG4;  }
	if ( ( RegModValue & SP_CLR_SIG5 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG5; }
	if ( ( RegModValue & SP_SET_SIG5 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG5;  }
	if ( ( RegModValue & SP_CLR_SIG6 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG6; }
	if ( ( RegModValue & SP_SET_SIG6 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG6;  }
	if ( ( RegModValue & SP_CLR_SIG7 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG7; }
	if ( ( RegModValue & SP_SET_SIG7 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG7;  }

	RunRsp();

}

int32_t Free8BitX86Reg (BLOCK_SECTION * Section) {
	int32_t x86Reg, count, MapCount[64], MapReg[64];

	//if (x86Mapped(x86_EBX) == NotMapped && !x86Protected(x86_EBX)) {return x86_EBX; }
	//if (x86Mapped(x86_EAX) == NotMapped && !x86Protected(x86_EAX)) {return x86_EAX; }
	//if (x86Mapped(x86_EDX) == NotMapped && !x86Protected(x86_EDX)) {return x86_EDX; }
	//if (x86Mapped(x86_ECX) == NotMapped && !x86Protected(x86_ECX)) {return x86_ECX; }
	if (x86Mapped(x86_EBX&0x2f) == NotMapped && !x86Protected(x86_EBX&0x2f)) {return x86_EBX; }
	if (x86Mapped(x86_EDX&0x2f) == NotMapped && !x86Protected(x86_EDX&0x2f)) {return x86_EDX; }
	if (x86Mapped(x86_ECX&0x2f) == NotMapped && !x86Protected(x86_ECX&0x2f)) {return x86_ECX; }
	if (x86Mapped(x86_EAX&0x2f) == NotMapped && !x86Protected(x86_EAX&0x2f)) {return x86_EAX; }
#ifdef EXT_REGS
	if (x86Mapped(x86_R9D&0x2f) == NotMapped && !x86Protected(x86_R9D&0x2f)) {return x86_R9D; }
	if (x86Mapped(x86_R10D&0x2f) == NotMapped && !x86Protected(x86_R10D&0x2f)) {return x86_R10D; }
	if (x86Mapped(x86_R11D&0x2f) == NotMapped && !x86Protected(x86_R11D&0x2f)) {return x86_R11D; }
	if (x86Mapped(x86_R14D&0x2f) == NotMapped && !x86Protected(x86_R14D&0x2f)) {return x86_R14D; }
#endif

	x86Reg = UnMap_8BitTempReg(Section);
	if (x86Reg >= 0) { return x86Reg; }

	for (count = 0; count < 64; count ++) {
		MapCount[count] = x86MapOrder(count);
		MapReg[count] = count;
	}
	for (count = 0; count < 64; count ++) {
		int i;

		for (i = 0; i < 63; i ++) {
			int temp;

			if (MapCount[i] < MapCount[i+1]) {
				temp = MapCount[i];
				MapCount[i] = MapCount[i+1];
				MapCount[i+1] = temp;
				temp = MapReg[i];
				MapReg[i] = MapReg[i+1];
				MapReg[i+1] = temp;
			}
		}

	}
	for (count = 0; count < 64; count ++) {
		if (MapCount[count] > 0) {
			if (!Is8BitReg(count)) {  continue; }
			if (UnMap_X86reg(Section,count)) {
				return count;
			}
		}
	}

	return -1;
}


int32_t FreeX86Reg (BLOCK_SECTION * Section) {
	int32_t x86Reg, count, MapCount[64], MapReg[64], StackReg;

	if (x86Mapped(x86_ESI&0x2f) == NotMapped && !x86Protected(x86_ESI&0x2f)) {return x86_ESI; }
	if (x86Mapped(x86_EDI&0x2f) == NotMapped && !x86Protected(x86_EDI&0x2f)) {return x86_EDI; }
	if (x86Mapped(x86_EBX&0x2f) == NotMapped && !x86Protected(x86_EBX&0x2f)) {return x86_EBX; }
	if (x86Mapped(x86_EDX&0x2f) == NotMapped && !x86Protected(x86_EDX&0x2f)) {return x86_EDX; }
	if (x86Mapped(x86_ECX&0x2f) == NotMapped && !x86Protected(x86_ECX&0x2f)) {return x86_ECX; }
	if (x86Mapped(x86_EAX&0x2f) == NotMapped && !x86Protected(x86_EAX&0x2f)) {return x86_EAX; }
#ifdef EXT_REGS
	if (x86Mapped(x86_R9D&0x2f) == NotMapped && !x86Protected(x86_R9D&0x2f)) {return x86_R9D; }
	if (x86Mapped(x86_R10D&0x2f) == NotMapped && !x86Protected(x86_R10D&0x2f)) {return x86_R10D; }
	if (x86Mapped(x86_R11D&0x2f) == NotMapped && !x86Protected(x86_R11D&0x2f)) {return x86_R11D; }
	if (x86Mapped(x86_R12D&0x2f) == NotMapped && !x86Protected(x86_R12D&0x2f)) {return x86_R12D; }
	if (x86Mapped(x86_R13D&0x2f) == NotMapped && !x86Protected(x86_R13D&0x2f)) {return x86_R13D; }
	if (x86Mapped(x86_R14D&0x2f) == NotMapped && !x86Protected(x86_R14D&0x2f)) {return x86_R14D; }
#endif

	x86Reg = UnMap_TempReg(Section);
	if (x86Reg > 0) { return x86Reg; }

	for (count = 0; count < 64; count ++) {
		MapCount[count] = x86MapOrder(count);
		MapReg[count] = count;
	}
	for (count = 0; count < 64; count ++) {
		int i;

		for (i = 0; i < 63; i ++) {
			int temp;

			if (MapCount[i] < MapCount[i+1]) {
				temp = MapCount[i];
				MapCount[i] = MapCount[i+1];
				MapCount[i+1] = temp;
				temp = MapReg[i];
				MapReg[i] = MapReg[i+1];
				MapReg[i+1] = temp;
			}
		}

	}
	StackReg = -1;
	for (count = 0; count < 64; count ++) {
		if (MapCount[count] > 0 && x86Mapped(MapReg[count]) != Stack_Mapped) {
			if (UnMap_X86reg(Section,MapReg[count])) {
				return MapReg[count];
			}
		}
		if (x86Mapped(MapReg[count]) == Stack_Mapped) { StackReg = MapReg[count]; }
	}
	if (StackReg >= 0) {
		UnMap_X86reg(Section,StackReg);
		return StackReg;
	}
	return -1;
}


uint32_t Is8BitReg (int32_t x86Reg) {
	if (x86Reg == x86_EAX) { return 1; }
	if (x86Reg == x86_EBX) { return 1; }
	if (x86Reg == x86_ECX) { return 1; }
	if (x86Reg == x86_EDX) { return 1; }
#ifdef EXT_REGS
	if (x86Reg == x86_R9D) { return 1; }
	if (x86Reg == x86_R10D) { return 1; }
	if (x86Reg == x86_R11D) { return 1; }
	if (x86Reg == x86_R14D) { return 1; }
#endif
	return 0;
}

void Load_FPR_ToTop (BLOCK_SECTION * Section, int32_t Reg, int32_t RegToLoad, int32_t Format) {
	int32_t i;

	if (RegToLoad < 0) { return; }
	if (Reg < 0) { return; }

	if (Format == FPU_Double || Format == FPU_Qword) {
		UnMap_FPR(Section,Reg + 1,1);
		UnMap_FPR(Section,RegToLoad + 1,1);
	} else {
		if ((Reg & 1) != 0) {
			for (i = 0; i < 8; i++) {
				if (FpuMappedTo(i) == (uint32_t)(Reg - 1)) {
					if (FpuState(i) == FPU_Double || FpuState(i) == FPU_Qword) {
						UnMap_FPR(Section,Reg,1);
					}
					i = 8;
				}
			}
		}
		if ((RegToLoad & 1) != 0) {
			for (i = 0; i < 8; i++) {
				if (FpuMappedTo(i) == (uint32_t)(RegToLoad - 1)) {
					if (FpuState(i) == FPU_Double || FpuState(i) == FPU_Qword) {
						UnMap_FPR(Section,RegToLoad,1);
					}
					i = 8;
				}
			}
		}
	}

	if (Reg == RegToLoad) {
		//if different format then unmap original reg from stack
		for (i = 0; i < 8; i++) {
			if (FpuMappedTo(i) == (uint32_t)Reg) {
				if (FpuState(i) != (uint32_t)Format) {
					UnMap_FPR(Section,Reg,1);
				}
				i = 8;
			}
		}
	} else {
		UnMap_FPR(Section,Reg,0);
	}

	if (RegInStack(Section,RegToLoad,Format)) {
		if (Reg != RegToLoad) {
			if (FpuMappedTo((StackTopPos - 1) & 7) != (uint32_t)RegToLoad) {
				UnMap_FPR(Section,FpuMappedTo((StackTopPos - 1) & 7),1);
				fpuLoadReg(&StackTopPos,StackPosition(Section,RegToLoad));
				FpuRoundingModel(StackTopPos) = RoundDefault;
				FpuMappedTo(StackTopPos)      = Reg;
				FpuState(StackTopPos)         = Format;
			} else {
				UnMap_FPR(Section,FpuMappedTo((StackTopPos - 1) & 7),1);
				Load_FPR_ToTop (Section,Reg, RegToLoad, Format);
			}
		} else {
			uint32_t RegPos, StackPos, i;

			for (i = 0; i < 8; i++) {
				if (FpuMappedTo(i) == (uint32_t)Reg) {
					RegPos = i;
					i = 8;
				}
			}

			if (RegPos == StackTopPos) {
				return;
			}
			StackPos = StackPosition(Section,Reg);

			FpuRoundingModel(RegPos) = FpuRoundingModel(StackTopPos);
			FpuMappedTo(RegPos)      = FpuMappedTo(StackTopPos);
			FpuState(RegPos)         = FpuState(StackTopPos);

			fpuExchange(StackPos);

			FpuRoundingModel(StackTopPos) = RoundDefault;
			FpuMappedTo(StackTopPos)      = Reg;
			FpuState(StackTopPos)         = Format;
		}
	} else {
		int TempReg;

		UnMap_FPR(Section,FpuMappedTo((StackTopPos - 1) & 7),1);
		for (i = 0; i < 8; i++) {
			if (FpuMappedTo(i) == (uint32_t)RegToLoad) {
				UnMap_FPR(Section,RegToLoad,1);
				i = 8;
			}
		}
		TempReg = Map_TempReg(Section,x86_Any,-1,0);
		switch (Format) {
		case FPU_Dword:
			MovePointerToX86reg(&FPRFloatLocation[RegToLoad],TempReg);
			MovePointerToX86reg(&FPRFloatLocation[RegToLoad],TempReg);

			fpuLoadIntegerDwordFromX86Reg(&StackTopPos,TempReg);
			break;
		case FPU_Qword:
			MovePointerToX86reg(&FPRDoubleLocation[RegToLoad],TempReg);
			fpuLoadIntegerQwordFromX86Reg(&StackTopPos,TempReg);
			break;
		case FPU_Float:
			MovePointerToX86reg(&FPRFloatLocation[RegToLoad],TempReg);
			fpuLoadDwordFromX86Reg(&StackTopPos,TempReg);
			break;
		case FPU_Double:
			MovePointerToX86reg(&FPRDoubleLocation[RegToLoad],TempReg);
			fpuLoadQwordFromX86Reg(&StackTopPos,TempReg);
			break;
		}
		x86Protected(TempReg) = 0;
		FpuRoundingModel(StackTopPos) = RoundDefault;
		FpuMappedTo(StackTopPos)      = Reg;
		FpuState(StackTopPos)         = Format;
	}

	//CurrentRoundingModel,FpuRoundingModel(StackTopPos));
}

void Map_GPR_32bit (BLOCK_SECTION * Section, int32_t Reg, uint32_t SignValue, int32_t MipsRegToLoad) {
	int x86Reg,count;

	if (Reg == 0) {
		return;
	}

	if (IsUnknown(Reg) || IsConst(Reg)) {
		x86Reg = FreeX86Reg(Section);
		if (x86Reg < 0) {
			printf("out of registers\n");
			return;
		}
	} else {
		if (Is64Bit(Reg)) {
			x86MapOrder(MipsRegHi(Reg)) = 0;
			x86Mapped(MipsRegHi(Reg)) = NotMapped;
			x86Protected(MipsRegHi(Reg)) = 0;
			MipsRegHi(Reg) = 0;
		}
		x86Reg = MipsRegLo(Reg);
	}
	for (count = 0; count < 64; count ++) {
		if (x86MapOrder(count) > 0) {
			x86MapOrder(count) += 1;
		}
	}
	x86MapOrder(x86Reg) = 1;

	if (MipsRegToLoad > 0) {
		if (IsUnknown(MipsRegToLoad)) {
			MoveVariableToX86reg(&GPR[MipsRegToLoad].UW[0],x86Reg);
		} else if (IsMapped(MipsRegToLoad)) {
			if (Reg != MipsRegToLoad) {
				MoveX86RegToX86Reg(MipsRegLo(MipsRegToLoad),x86Reg);
			}
		} else {
			if (MipsRegLo(MipsRegToLoad) != 0) {
				MoveConstToX86reg(MipsRegLo(MipsRegToLoad),x86Reg);
			} else {
				XorX86RegToX86Reg(x86Reg,x86Reg);
			}
		}
	} else if (MipsRegToLoad == 0) {
		XorX86RegToX86Reg(x86Reg,x86Reg);
	}
	x86Mapped(x86Reg) = GPR_Mapped;
	x86Protected(x86Reg) = 1;
	MipsRegLo(Reg) = x86Reg;
	MipsRegState(Reg) = SignValue ? STATE_MAPPED_32_SIGN : STATE_MAPPED_32_ZERO;
}

void Map_GPR_64bit (BLOCK_SECTION * Section, int32_t Reg, int32_t MipsRegToLoad) {
	int x86Hi, x86lo, count;

	if (Reg == 0) {

		return;
	}

	ProtectGPR(Section,Reg);
	if (IsUnknown(Reg) || IsConst(Reg)) {
		x86Hi = FreeX86Reg(Section);
		if (x86Hi < 0) { printf("out of registers\n"); return; }
		x86Protected(x86Hi) = 1;

		x86lo = FreeX86Reg(Section);
		if (x86lo < 0) { printf("out of registers\n"); return; }
		x86Protected(x86lo) = 1;

	} else {
		x86lo = MipsRegLo(Reg);
		if (Is32Bit(Reg)) {
			x86Protected(x86lo) = 1;
			x86Hi = FreeX86Reg(Section);
			if (x86Hi < 0) { printf("out of registers\n"); return; }
			x86Protected(x86Hi) = 1;
		} else {
			x86Hi = MipsRegHi(Reg);
		}
	}

	for (count = 0; count < 64; count ++) {
		if (x86MapOrder(count) > 0) { x86MapOrder(count) += 1; }
	}

	x86MapOrder(x86Hi) = 1;
	x86MapOrder(x86lo) = 1;
	if (MipsRegToLoad > 0) {
		if (IsUnknown(MipsRegToLoad)) {
			MoveVariableToX86reg(&GPR[MipsRegToLoad].UW[1],x86Hi);
			MoveVariableToX86reg(&GPR[MipsRegToLoad].UW[0],x86lo);
		} else if (IsMapped(MipsRegToLoad)) {
			if (Is32Bit(MipsRegToLoad)) {
				if (IsSigned(MipsRegToLoad)) {

					MoveX86RegToX86Reg(MipsRegLo(MipsRegToLoad),x86Hi);
					ShiftRightSignImmed(x86Hi,31);
				} else {
					XorX86RegToX86Reg(x86Hi,x86Hi);
				}
				if (Reg != MipsRegToLoad) {
					MoveX86RegToX86Reg(MipsRegLo(MipsRegToLoad),x86lo);
				}
			} else {
				if (Reg != MipsRegToLoad) {
					MoveX86RegToX86Reg(MipsRegHi(MipsRegToLoad),x86Hi);
					MoveX86RegToX86Reg(MipsRegLo(MipsRegToLoad),x86lo);
				}
			}
		} else {
			if (Is32Bit(MipsRegToLoad)) {
				if (IsSigned(MipsRegToLoad)) {
					if (MipsRegLo((int)MipsRegLo(MipsRegToLoad) >> 31) != 0) {
						MoveConstToX86reg((int)MipsRegLo(MipsRegToLoad) >> 31,x86Hi);
					} else {
						XorX86RegToX86Reg(x86Hi,x86Hi);
					}
				} else {
					XorX86RegToX86Reg(x86Hi,x86Hi);
				}
			} else {
				if (MipsRegHi(MipsRegToLoad) != 0) {
					MoveConstToX86reg(MipsRegHi(MipsRegToLoad),x86Hi);
				} else {
					XorX86RegToX86Reg(x86Hi,x86Hi);
				}
			}
			if (MipsRegLo(MipsRegToLoad) != 0) {
				MoveConstToX86reg(MipsRegLo(MipsRegToLoad),x86lo);
			} else {
				XorX86RegToX86Reg(x86lo,x86lo);
			}
		}
	} else if (MipsRegToLoad == 0) {
		XorX86RegToX86Reg(x86Hi,x86Hi);
		XorX86RegToX86Reg(x86lo,x86lo);
	}
	x86Mapped(x86Hi) = GPR_Mapped;
	x86Mapped(x86lo) = GPR_Mapped;
	MipsRegHi(Reg) = x86Hi;
	MipsRegLo(Reg) = x86lo;
	MipsRegState(Reg) = STATE_MAPPED_64;
}

int32_t Map_TempReg (BLOCK_SECTION * Section, int32_t x86Reg, int32_t MipsReg, uint32_t LoadHiWord) {
	int32_t count, x64reg = 0;

	//if((x86Reg & x64_Reg) || (x86Reg & x64_Any))
		//Int3();
	x64reg = (x86Reg & x64_Reg) || (x86Reg & x64_Any);
	x86Reg &= ~x64_Reg;

 	if ((x86Reg == x86_Any) || (x86Reg == x64_Any)) {
		for (count = 0; count < 64; count ++ ) {
			if (x86Mapped(count) == Temp_Mapped) {
				if (x86Protected(count) == 0) { x86Reg = count; }
			}
		}

		if ((x86Reg == x86_Any) || (x86Reg == x64_Any)) {
			x86Reg = FreeX86Reg(Section);
			if (x86Reg < 0) {
				x86Reg = FreeX86Reg(Section);
				return -1;
			}
		}
	} else if (x86Reg == x86_Any8Bit) {
		if (x86Mapped(x86_EBX&0x2f) == Temp_Mapped && !x86Protected(x86_EBX&0x2f)) { x86Reg = x86_EBX; }
		if (x86Mapped(x86_EAX&0x2f) == Temp_Mapped && !x86Protected(x86_EAX&0x2f)) { x86Reg = x86_EAX; }
		if (x86Mapped(x86_EDX&0x2f) == Temp_Mapped && !x86Protected(x86_EDX&0x2f)) { x86Reg = x86_EDX; }
		if (x86Mapped(x86_ECX&0x2f) == Temp_Mapped && !x86Protected(x86_ECX&0x2f)) { x86Reg = x86_ECX; }

#ifdef EXT_REGS
		if (x86Mapped(x86_R9D&0x2f) == Temp_Mapped && !x86Protected(x86_R9D&0x2f)) { x86Reg = x86_R9D; }
		if (x86Mapped(x86_R10D&0x2f) == Temp_Mapped && !x86Protected(x86_R10D&0x2f)) { x86Reg = x86_R10D; }
		if (x86Mapped(x86_R11D&0x2f) == Temp_Mapped && !x86Protected(x86_R11D&0x2f)) { x86Reg = x86_R11D; }
		if (x86Mapped(x86_R14D&0x2f) == Temp_Mapped && !x86Protected(x86_R14D&0x2f)) { x86Reg = x86_R14D; }
#endif

		if (x86Reg == x86_Any8Bit) {
			x86Reg = Free8BitX86Reg(Section);
			if (x86Reg < 0) {
				return -1;
			}
		}
	} else {
		int NewReg;

		if (x86Mapped(x86Reg) == GPR_Mapped) {
			if (x86Protected(x86Reg) == 1) {
				return -1;
			}
			x86Protected(x86Reg) = 1;
			NewReg = FreeX86Reg(Section);
			for (count = 1; count < 32; count ++) {
				if (IsMapped(count)) {
					if (MipsRegLo(count) == (uint32_t)x86Reg) {
						if (NewReg < 0) {
							UnMap_GPR(Section,count,1);
							count = 32;
							continue;
						}

						x86Mapped(NewReg) = GPR_Mapped;
						x86MapOrder(NewReg) = x86MapOrder(x86Reg);
						MipsRegLo(count) = NewReg;
						MoveX86RegToX86Reg(x86Reg,NewReg);
						if (MipsReg == count && LoadHiWord == 0) { MipsReg = -1; }
						count = 32;
					}
					if (Is64Bit(count) && MipsRegHi(count) == (uint32_t)x86Reg) {
						if (NewReg < 0) {
							UnMap_GPR(Section,count,1);
							count = 32;
							continue;
						}
						x86Mapped(NewReg) = GPR_Mapped;
						x86MapOrder(NewReg) = x86MapOrder(x86Reg);
						MipsRegHi(count) = NewReg;
						MoveX86RegToX86Reg(x86Reg,NewReg);
						if (MipsReg == count && LoadHiWord == 1) { MipsReg = -1; }
						count = 32;
					}
				}
			}
		}
		if (x86Mapped(x86Reg) == Stack_Mapped) {
			UnMap_X86reg(Section,x86Reg);
		}
	}
	if (MipsReg >= 0) {
		if (LoadHiWord) {
			if (IsUnknown(MipsReg)) {
				MoveVariableToX86reg(&GPR[MipsReg].UW[1],x86Reg);
			} else if (IsMapped(MipsReg)) {
				if (Is64Bit(MipsReg)) {
					MoveX86RegToX86Reg(MipsRegHi(MipsReg),x86Reg);
				} else if (IsSigned(MipsReg)){
					MoveX86RegToX86Reg(MipsRegLo(MipsReg),x86Reg);

					ShiftRightSignImmed(x86Reg,31);
				} else {
					MoveConstToX86reg(0,x86Reg);
				}
			} else {
				if (Is64Bit(MipsReg)) {
					if (MipsRegHi(MipsReg) != 0) {
						MoveConstToX86reg(MipsRegHi(MipsReg),x86Reg);
					} else {
						XorX86RegToX86Reg(x86Reg,x86Reg);
					}
				} else {
					if ((int)MipsRegLo(MipsReg) >> 31 != 0) {
						MoveConstToX86reg((int)MipsRegLo(MipsReg) >> 31,x86Reg);
					} else {
						XorX86RegToX86Reg(x86Reg,x86Reg);
					}
				}
			}
		} else {
			if (IsUnknown(MipsReg)) {
				MoveVariableToX86reg(&GPR[MipsReg].UW[0],x86Reg);
			} else if (IsMapped(MipsReg)) {
				MoveX86RegToX86Reg(MipsRegLo(MipsReg),x86Reg);
			} else {
				if (MipsRegLo(MipsReg) != 0) {
					MoveConstToX86reg(MipsRegLo(MipsReg),x86Reg);
				} else {
					XorX86RegToX86Reg(x86Reg,x86Reg);
				}
			}
		}
	}
	x86Mapped(x86Reg) = Temp_Mapped;
	x86Protected(x86Reg) = 1;
	for (count = 0; count < 64; count ++) {
		if (x86MapOrder(count) > 0) {
			x86MapOrder(count) += 1;
		}
	}
	x86MapOrder(x86Reg) = 1;
	if(x64reg) x86Reg |= x64_Reg;
	return x86Reg;
}

void ProtectGPR(BLOCK_SECTION * Section, uint32_t Reg) {
	if (IsUnknown(Reg)) { return; }
	if (IsConst(Reg)) { return; }
	if (Is64Bit(Reg)) {
		x86Protected(MipsRegHi(Reg)) = 1;
	}
	x86Protected(MipsRegLo(Reg)) = 1;
}

uint32_t RegInStack(BLOCK_SECTION * Section,int32_t Reg, int32_t Format) {
	int i;

	for (i = 0; i < 8; i++) {
		if (FpuMappedTo(i) == (uint32_t)Reg) {
			if (FpuState(i) == (uint32_t)Format) { return 1; }
			else if (Format == -1) { return 1; }
			return 0;
		}
	}
	return 0;
}

void SetFpuLocations (void) {
	int count;

	if ((STATUS_REGISTER & STATUS_FR) == 0) {
		for (count = 0; count < 32; count ++) {
			FPRFloatLocation[count] = (void *)(&FPR[count >> 1].W[count & 1]);
			//FPRDoubleLocation[count] = FPRFloatLocation[count];
			FPRDoubleLocation[count] = (void *)(&FPR[count >> 1].DW);
		}
	} else {
		for (count = 0; count < 32; count ++) {
			FPRFloatLocation[count] = (void *)(&FPR[count].W[1]);
			//FPRFloatLocation[count] = (void *)(&FPR[count].W[1]);
			//FPRDoubleLocation[count] = FPRFloatLocation[count];
			FPRDoubleLocation[count] = (void *)(&FPR[count].DW);
		}
	}
}

int32_t StackPosition (BLOCK_SECTION * Section, int32_t Reg) {
	int32_t i;

	for (i = 0; i < 8; i++) {
		if (FpuMappedTo(i) == (uint32_t)Reg) {
			return ((i - StackTopPos) & 7);
		}
	}
	return -1;
}

int UnMap_8BitTempReg (BLOCK_SECTION * Section) {
	int32_t count;

	for (count = 0; count < 64; count ++) {
		if (!Is8BitReg(count)) { continue; }
		if (MipsRegState(count) == Temp_Mapped) {
			if (x86Protected(count) == 0) {
				x86Mapped(count) = NotMapped;
				return count;
			}
		}
	}
	return -1;
}

void UnMap_AllFPRs ( BLOCK_SECTION * Section ) {
	int32_t StackPos;

	for (;;) {
		int i, StartPos;
		StackPos = StackTopPos;
		if (FpuMappedTo(StackTopPos) != -1 ) {
			UnMap_FPR(Section,FpuMappedTo(StackTopPos),1);
			continue;
		}
		//see if any more registers mapped
		StartPos = StackTopPos;
		for (i = 0; i < 8; i++) {
			if (FpuMappedTo((StartPos + i) & 7) != -1 ) { fpuIncStack(&StackTopPos); }
		}
		if (StackPos != StackTopPos) { continue; }
		return;
	}
}

void UnMap_FPR (BLOCK_SECTION * Section, int32_t Reg, int32_t WriteBackValue ) {
	int32_t TempReg;
	int32_t i;

	if (Reg < 0) { return; }
	for (i = 0; i < 8; i++) {
		if (FpuMappedTo(i) != (uint32_t)Reg) { continue; }
		if (WriteBackValue) {
			int RegPos;

			if (((i - StackTopPos + 8) & 7) != 0) {
				uint32_t RoundingModel, MappedTo, RegState;

				RoundingModel = FpuRoundingModel(StackTopPos);
				MappedTo      = FpuMappedTo(StackTopPos);
				RegState      = FpuState(StackTopPos);
				FpuRoundingModel(StackTopPos) = FpuRoundingModel(i);
				FpuMappedTo(StackTopPos)      = FpuMappedTo(i);
				FpuState(StackTopPos)         = FpuState(i);
				FpuRoundingModel(i) = RoundingModel;
				FpuMappedTo(i)      = MappedTo;
				FpuState(i)         = RegState;
				fpuExchange((i - StackTopPos) & 7);
			}

			if (CurrentRoundingModel != FpuRoundingModel(i)) {
				int x86reg;

				fpuControl = 0;
				fpuStoreControl(&fpuControl);
				x86reg = Map_TempReg(Section,x86_Any,-1,0);
				MoveVariableToX86reg(&fpuControl,  x86reg);
				AndConstToX86Reg(x86reg, 0xF3FF);

				switch (FpuRoundingModel(i)) {
				case RoundDefault: OrVariableToX86Reg(&FPU_RoundingMode,x86reg); break;
				case RoundTruncate: OrConstToX86Reg(0x0C00, x86reg); break;
				case RoundNearest: /*OrConstToX86Reg(0x0000, x86reg);*/ break;
				case RoundDown: OrConstToX86Reg(0x0400, x86reg); break;
				case RoundUp: OrConstToX86Reg(0x0800, x86reg); break;
				}
				MoveX86regToVariable(x86reg, &fpuControl);
				fpuLoadControl(&fpuControl);
				CurrentRoundingModel = FpuRoundingModel(i);
			}

			RegPos = StackTopPos;
			TempReg = Map_TempReg(Section,x86_Any,-1,0);
			switch (FpuState(StackTopPos)) {
			case FPU_Dword:
				MovePointerToX86reg(&FPRFloatLocation[FpuMappedTo(StackTopPos)],TempReg);
				fpuStoreIntegerDwordFromX86Reg(&StackTopPos,TempReg, 1);
				break;
			case FPU_Qword:
				MovePointerToX86reg(&FPRDoubleLocation[FpuMappedTo(StackTopPos)],TempReg);
				fpuStoreIntegerQwordFromX86Reg(&StackTopPos,TempReg, 1);
				break;
			case FPU_Float:
				MovePointerToX86reg(&FPRFloatLocation[FpuMappedTo(StackTopPos)],TempReg);
				fpuStoreDwordFromX86Reg(&StackTopPos,TempReg, 1);
				break;
			case FPU_Double:
				MovePointerToX86reg(&FPRDoubleLocation[FpuMappedTo(StackTopPos)],TempReg);
				fpuStoreQwordFromX86Reg(&StackTopPos,TempReg, 1);
				break;
			}
			x86Protected(TempReg) = 0;
			FpuRoundingModel(RegPos) = RoundDefault;
			FpuMappedTo(RegPos)      = -1;
			FpuState(RegPos)         = FPU_Unkown;
		} else {
			fpuFree((i - StackTopPos) & 7);
			FpuRoundingModel(i) = RoundDefault;
			FpuMappedTo(i)      = -1;
			FpuState(i)         = FPU_Unkown;
		}
		return;
	}
}

void UnMap_GPR (BLOCK_SECTION * Section, uint32_t Reg, int32_t WriteBackValue) {
	if (Reg == 0) {
		return;
	}
	if (IsUnknown(Reg)) { return; }
	if (IsConst(Reg)) {
		if (!WriteBackValue) {
			MipsRegState(Reg) = STATE_UNKNOWN;
			return;
		}
		if (Is64Bit(Reg)) {
			MoveConstToVariable(MipsRegHi(Reg),&GPR[Reg].UW[1]);
			MoveConstToVariable(MipsRegLo(Reg),&GPR[Reg].UW[0]);
			MipsRegState(Reg) = STATE_UNKNOWN;
			return;
		}
		if ((MipsRegLo(Reg) & 0x80000000) != 0) {
			MoveConstToVariable(0xFFFFFFFF,&GPR[Reg].UW[1]);
		} else {
			MoveConstToVariable(0,&GPR[Reg].UW[1]);
		}
		MoveConstToVariable(MipsRegLo(Reg),&GPR[Reg].UW[0]);
		MipsRegState(Reg) = STATE_UNKNOWN;
		return;
	}
	if (Is64Bit(Reg)) {
		x86Mapped(MipsRegHi(Reg)) = NotMapped;
		x86Protected(MipsRegHi(Reg)) = 0;
	}
	x86Mapped(MipsRegLo(Reg)) = NotMapped;
	x86Protected(MipsRegLo(Reg)) = 0;
	if (!WriteBackValue) {
		MipsRegState(Reg) = STATE_UNKNOWN;
		return;
	}
	MoveX86regToVariable(MipsRegLo(Reg),&GPR[Reg].UW[0]);
	if (Is64Bit(Reg)) {
		MoveX86regToVariable(MipsRegHi(Reg),&GPR[Reg].UW[1]);
	} else {
		if (IsSigned(Reg)) {
			ShiftRightSignImmed(MipsRegLo(Reg),31);
			MoveX86regToVariable(MipsRegLo(Reg),&GPR[Reg].UW[1]);
		} else {
			MoveConstToVariable(0,&GPR[Reg].UW[1]);
		}
	}
	MipsRegState(Reg) = STATE_UNKNOWN;
}

int UnMap_TempReg (BLOCK_SECTION * Section) {
	int count;

	for (count = 0; count < 64; count ++) {
		if (x86Mapped(count) == Temp_Mapped) {
			if (x86Protected(count) == 0) {
				x86Mapped(count) = NotMapped;
				return count & 0xff;
			}
		}
	}
	return -1;
}

int UnMap_TempRegSet (REG_INFO * RegWorking) {
	int count;

	for (count = 0; count < 64; count ++) {
		if (RegWorking->x86reg_MappedTo[count] == Temp_Mapped) {
			if (RegWorking->x86reg_Protected[count] == 0) {
				RegWorking->x86reg_MappedTo[count] = NotMapped;
				return count;
			}
		}
	}
	return -1;
}


uint32_t UnMap_X86reg (BLOCK_SECTION * Section, uint32_t x86Reg) {
	int count;

	if (x86Mapped(x86Reg) == NotMapped && x86Protected(x86Reg) == 0) { return 1; }
	if (x86Mapped(x86Reg) == Temp_Mapped) {
		if (x86Protected(x86Reg) == 0) {
			x86Mapped(x86Reg) = NotMapped;
			return 1;
		}
		return 0;
	}
	for (count = 1; count < 32; count ++) {
		if (IsMapped(count)) {
			if (Is64Bit(count)) {
				if (MipsRegHi(count) == x86Reg) {
					if (x86Protected(x86Reg) == 0) {
						UnMap_GPR(Section,count,1);
						return 1;
					}
					break;
				}
			}
			if (MipsRegLo(count) == x86Reg) {
				if (x86Protected(x86Reg) == 0) {
					UnMap_GPR(Section,count,1);
					return 1;
				}
				break;
			}
		}
	}
	return 0;
}

void UnProtectGPR(BLOCK_SECTION * Section, uint32_t Reg) {
	if (IsUnknown(Reg)) { return; }
	if (IsConst(Reg)) { return; }
	if (Is64Bit(Reg)) {
		x86Protected(MipsRegHi(Reg)) = 0;
	}
	x86Protected(MipsRegLo(Reg)) = 0;
}

void UpdateCurrentHalfLine (void) {
	if (Timers->Timer < 0) {
		HalfLine = 0;
		return;
	}
	HalfLine = (Timers->Timer / 1500);
	HalfLine &= ~1;
	HalfLine += ViFieldNumber;

}

void WriteBackRegisters (BLOCK_SECTION * Section) {
	int32_t count;
	uint32_t bEdiZero = 0;
	uint32_t bEsiSign = 0;
	/*** coming soon ***/
	uint32_t bEaxGprLo = 0;
	uint32_t bEbxGprHi = 0;

	for (count = 1; count < 64; count ++) { x86Protected(count) = 0; }
	for (count = 1; count < 64; count ++) { UnMap_X86reg (Section, count); }

	/*************************************/

	for (count = 1; count < 32; count ++) {
		switch (MipsRegState(count)) {
		case STATE_UNKNOWN: break;
		case STATE_CONST_32:
			if (!bEdiZero && (!MipsRegLo(count) || !(MipsRegLo(count) & 0x80000000))) {
				XorX86RegToX86Reg(x86_EDI, x86_EDI);
				bEdiZero = 1;
			}
			if (!bEsiSign && (MipsRegLo(count) & 0x80000000)) {
				MoveConstToX86reg(0xFFFFFFFF, x86_ESI);
				bEsiSign = 1;
			}

			if ((MipsRegLo(count) & 0x80000000) != 0) {
				MoveX86regToVariable(x86_ESI,&GPR[count].UW[1]);
			} else {
				MoveX86regToVariable(x86_EDI,&GPR[count].UW[1]);
			}

			if (MipsRegLo(count) == 0) {
				MoveX86regToVariable(x86_EDI,&GPR[count].UW[0]);
			} else if (MipsRegLo(count) == 0xFFFFFFFF) {
				MoveX86regToVariable(x86_ESI,&GPR[count].UW[0]);
			} else
				MoveConstToVariable(MipsRegLo(count),&GPR[count].UW[0]);

			MipsRegState(count) = STATE_UNKNOWN;
			break;
		case STATE_CONST_64:
			if (MipsRegLo(count) == 0 || MipsRegHi(count) == 0) {
				XorX86RegToX86Reg(x86_EDI, x86_EDI);
				bEdiZero = 1;
			}
			if (MipsRegLo(count) == 0xFFFFFFFF || MipsRegHi(count) == 0xFFFFFFFF) {
				MoveConstToX86reg(0xFFFFFFFF, x86_ESI);
				bEsiSign = 1;
			}

			if (MipsRegHi(count) == 0) {
				MoveX86regToVariable(x86_EDI,&GPR[count].UW[1]);
			} else if (MipsRegLo(count) == 0xFFFFFFFF) {
				MoveX86regToVariable(x86_ESI,&GPR[count].UW[1]);
			} else {
				MoveConstToVariable(MipsRegHi(count),&GPR[count].UW[1]);
			}

			if (MipsRegLo(count) == 0) {
				MoveX86regToVariable(x86_EDI,&GPR[count].UW[0]);
			} else if (MipsRegLo(count) == 0xFFFFFFFF) {
				MoveX86regToVariable(x86_ESI,&GPR[count].UW[0]);
			} else {
				MoveConstToVariable(MipsRegLo(count),&GPR[count].UW[0]);
			}
			MipsRegState(count) = STATE_UNKNOWN;
			break;

		}
	}
	UnMap_AllFPRs(Section);
}

