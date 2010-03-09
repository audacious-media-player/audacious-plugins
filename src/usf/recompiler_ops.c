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

uint32_t BranchCompare = 0;

void CompileReadTLBMiss (BLOCK_SECTION * Section, int32_t AddressReg, int32_t LookUpReg ) {
#ifdef USEX64
    MoveX86regToMemory(AddressReg, x86_R15, (uintptr_t)TLBLoadAddress-(uintptr_t)TLB_Map);
#else
	MoveX86regToVariable(AddressReg,TLBLoadAddress);
#endif
    TestX86RegToX86Reg(LookUpReg,LookUpReg);
	CompileExit(Section->CompilePC,&Section->RegWorking,TLBReadMiss,0,JeLabel32);
}

/************************** Branch functions  ************************/
void Compile_R4300i_Branch (BLOCK_SECTION * Section, void (*CompareFunc)(BLOCK_SECTION * Section), int32_t BranchType, uint32_t Link) {
	static int32_t EffectDelaySlot, DoneJumpDelay, DoneContinueDelay;
	static REG_INFO RegBeforeDelay;
	int32_t count;

	if ( NextInstruction == NORMAL ) {

		if ((Section->CompilePC & 0xFFC) != 0xFFC) {
			switch (BranchType) {
			case BranchTypeRs: EffectDelaySlot = DelaySlotEffectsCompare(Section->CompilePC,Opcode.rs,0); break;
			case BranchTypeRsRt: EffectDelaySlot = DelaySlotEffectsCompare(Section->CompilePC,Opcode.rs,Opcode.rt); break;
			case BranchTypeCop1:
				{
					OPCODE Command;

					if (!r4300i_LW_VAddr(Section->CompilePC + 4, &Command.Hex)) {
						//DisplayError(GS(MSG_FAIL_LOAD_WORD));
						StopEmulation();
					}

					EffectDelaySlot = 0;
					if (Command.op == R4300i_CP1) {
						if (Command.fmt == R4300i_COP1_S && (Command.funct & 0x30) == 0x30 ) {
							EffectDelaySlot = 1;
						}
						if (Command.fmt == R4300i_COP1_D && (Command.funct & 0x30) == 0x30 ) {
							EffectDelaySlot = 1;
						}
					}
				}
				break;

			}
		} else {
			EffectDelaySlot = 1;
		}
		Section->Jump.TargetPC        = Section->CompilePC + ((short)Opcode.offset << 2) + 4;
		Section->Jump.LinkLocation    = NULL;
		Section->Jump.LinkLocation2   = NULL;
		Section->Jump.DoneDelaySlot   = 0;
		Section->Cont.TargetPC        = Section->CompilePC + 8;
		Section->Cont.LinkLocation    = NULL;
		Section->Cont.LinkLocation2   = NULL;
		Section->Cont.DoneDelaySlot   = 0;
		if (Section->Jump.TargetPC < Section->Cont.TargetPC) {
			Section->Cont.FallThrough = 0;
			Section->Jump.FallThrough = 1;
		} else {
			Section->Cont.FallThrough = 1;
			Section->Jump.FallThrough = 0;
		}
		if (Link) {
			UnMap_GPR(Section, 31, 0);
			MipsRegLo(31) = Section->CompilePC + 8;
			MipsRegState(31) = STATE_CONST_32;
		}
		if (EffectDelaySlot) {
			CompareFunc(Section);

			if ((Section->CompilePC & 0xFFC) == 0xFFC) {
				GenerateSectionLinkage(Section);
				NextInstruction = END_BLOCK;
				return;
			}
			if (!Section->Jump.FallThrough && !Section->Cont.FallThrough) {
				if (Section->Jump.LinkLocation != NULL) {
					SetJump32((uint32_t *)Section->Jump.LinkLocation,(uint32_t *)RecompPos);
					Section->Jump.LinkLocation = NULL;
					if (Section->Jump.LinkLocation2 != NULL) {
						SetJump32((uint32_t *)Section->Jump.LinkLocation2,(uint32_t *)RecompPos);
						Section->Jump.LinkLocation2 = NULL;
					}
					Section->Jump.FallThrough = 1;
				} else if (Section->Cont.LinkLocation != NULL){

					SetJump32((uint32_t *)Section->Cont.LinkLocation,(uint32_t *)RecompPos);
					Section->Cont.LinkLocation = NULL;
					if (Section->Cont.LinkLocation2 != NULL) {
						SetJump32((uint32_t *)Section->Cont.LinkLocation2,(uint32_t *)RecompPos);
						Section->Cont.LinkLocation2 = NULL;
					}
					Section->Cont.FallThrough = 1;
				}
			}
			for (count = 1; count < 64; count ++) { x86Protected(count) = 0; }
			memcpy(&RegBeforeDelay,&Section->RegWorking,sizeof(REG_INFO));
		}
		NextInstruction = DO_DELAY_SLOT;
	} else if (NextInstruction == DELAY_SLOT_DONE ) {
		if (EffectDelaySlot) {
			JUMP_INFO * FallInfo = Section->Jump.FallThrough?&Section->Jump:&Section->Cont;
			JUMP_INFO * JumpInfo = Section->Jump.FallThrough?&Section->Cont:&Section->Jump;

			if (FallInfo->FallThrough && !FallInfo->DoneDelaySlot) {
				for (count = 1; count < 64; count ++) { x86Protected(count) = 0; }
				memcpy(&FallInfo->RegSet,&Section->RegWorking,sizeof(REG_INFO));
				FallInfo->DoneDelaySlot = 1;
				if (!JumpInfo->DoneDelaySlot) {
					FallInfo->FallThrough = 0;
					JmpLabel32(0);
					FallInfo->LinkLocation = RecompPos - 4;

					if (JumpInfo->LinkLocation != NULL) {
						SetJump32((uint32_t *)JumpInfo->LinkLocation,(uint32_t *)RecompPos);
						JumpInfo->LinkLocation = NULL;
						if (JumpInfo->LinkLocation2 != NULL) {
							SetJump32((uint32_t *)JumpInfo->LinkLocation2,(uint32_t *)RecompPos);
							JumpInfo->LinkLocation2 = NULL;
						}
						JumpInfo->FallThrough = 1;
						NextInstruction = DO_DELAY_SLOT;
						memcpy(&Section->RegWorking,&RegBeforeDelay,sizeof(REG_INFO));
						return;
					}
				}
			}
		} else {
			int32_t count;

			CompareFunc(Section);
			for (count = 1; count < 64; count ++) { x86Protected(count) = 0; }
			memcpy(&Section->Cont.RegSet,&Section->RegWorking,sizeof(REG_INFO));
			memcpy(&Section->Jump.RegSet,&Section->RegWorking,sizeof(REG_INFO));
		}
		GenerateSectionLinkage(Section);
		NextInstruction = END_BLOCK;
	} else {

	}
}

void Compile_R4300i_BranchLikely (BLOCK_SECTION * Section, void (*CompareFunc)(BLOCK_SECTION * Section), uint32_t Link) {
	int32_t count;

	if ( NextInstruction == NORMAL ) {
		Section->Jump.TargetPC      = Section->CompilePC + ((short)Opcode.offset << 2) + 4;
		Section->Jump.FallThrough   = 1;
		Section->Jump.LinkLocation  = NULL;
		Section->Jump.LinkLocation2 = NULL;
		Section->Cont.TargetPC      = Section->CompilePC + 8;
		Section->Cont.FallThrough   = 0;
		Section->Cont.LinkLocation  = NULL;
		Section->Cont.LinkLocation2 = NULL;
		if (Link) {
			UnMap_GPR(Section, 31, 0);
			MipsRegLo(31) = Section->CompilePC + 8;
			MipsRegState(31) = STATE_CONST_32;
		}
		CompareFunc(Section);
		for (count = 1; count < 64; count ++) { x86Protected(count) = 0; }
		memcpy(&Section->Cont.RegSet,&Section->RegWorking,sizeof(REG_INFO));

		if (Section->Cont.FallThrough)  {
			if (Section->Jump.LinkLocation != NULL) {
			}
			GenerateSectionLinkage(Section);
			NextInstruction = END_BLOCK;
		} else {
			if ((Section->CompilePC & 0xFFC) == 0xFFC) {
				Section->Jump.FallThrough = 0;
				if (Section->Jump.LinkLocation != NULL) {
					SetJump32(Section->Jump.LinkLocation,RecompPos);
					Section->Jump.LinkLocation = NULL;
					if (Section->Jump.LinkLocation2 != NULL) {
						SetJump32(Section->Jump.LinkLocation2,RecompPos);
						Section->Jump.LinkLocation2 = NULL;
					}
				}
				JmpLabel32(0);
				Section->Jump.LinkLocation = RecompPos - 4;
				if (Section->Cont.LinkLocation != NULL) {
					SetJump32(Section->Cont.LinkLocation,RecompPos);
					Section->Cont.LinkLocation = NULL;
					if (Section->Cont.LinkLocation2 != NULL) {
						SetJump32(Section->Cont.LinkLocation2,RecompPos);
						Section->Cont.LinkLocation2 = NULL;
					}
				}
				CompileExit (Section->CompilePC + 8,&Section->RegWorking,Normal,1,NULL);
				GenerateSectionLinkage(Section);
				NextInstruction = END_BLOCK;
			} else {
				NextInstruction = DO_DELAY_SLOT;
			}
		}
	} else if (NextInstruction == DELAY_SLOT_DONE ) {
		for (count = 1; count < 64; count ++) { x86Protected(count) = 0; }
		memcpy(&Section->Jump.RegSet,&Section->RegWorking,sizeof(REG_INFO));
		GenerateSectionLinkage(Section);
		NextInstruction = END_BLOCK;
	} else {
	}
}

void BNE_Compare (BLOCK_SECTION * Section) {
	uint8_t *Jump;

	if (IsKnown(Opcode.rs) && IsKnown(Opcode.rt)) {
		if (IsConst(Opcode.rs) && IsConst(Opcode.rt)) {
			if (Is64Bit(Opcode.rs) || Is64Bit(Opcode.rt)) {
				Compile_R4300i_UnknownOpcode(Section);
			} else if (MipsRegLo(Opcode.rs) != MipsRegLo(Opcode.rt)) {
				Section->Jump.FallThrough = 1;
				Section->Cont.FallThrough = 0;
			} else {
				Section->Jump.FallThrough = 0;
				Section->Cont.FallThrough = 1;
			}
		} else if (IsMapped(Opcode.rs) && IsMapped(Opcode.rt)) {
			if (Is64Bit(Opcode.rs) || Is64Bit(Opcode.rt)) {
				ProtectGPR(Section,Opcode.rs);
				ProtectGPR(Section,Opcode.rt);

				CompX86RegToX86Reg(
					Is32Bit(Opcode.rs)?Map_TempReg(Section,x86_Any,Opcode.rs,1):MipsRegHi(Opcode.rs),
					Is32Bit(Opcode.rt)?Map_TempReg(Section,x86_Any,Opcode.rt,1):MipsRegHi(Opcode.rt)
				);

				if (Section->Jump.FallThrough) {
					JneLabel8(0);
					Jump = RecompPos - 1;
				} else {
					JneLabel32(0);
					Section->Jump.LinkLocation = RecompPos - 4;
				}
				CompX86RegToX86Reg(MipsRegLo(Opcode.rs),MipsRegLo(Opcode.rt));
				if (Section->Cont.FallThrough) {
					JneLabel32 ( 0 );
					Section->Jump.LinkLocation2 = RecompPos - 4;
				} else if (Section->Jump.FallThrough) {
					JeLabel32 (  0 );
					Section->Cont.LinkLocation = RecompPos - 4;
					SetJump8(Jump,RecompPos);
				} else {
					JeLabel32 (  0 );
					Section->Cont.LinkLocation = RecompPos - 4;
					JmpLabel32(0);
					Section->Jump.LinkLocation2 = RecompPos - 4;
				}
			} else {
				CompX86RegToX86Reg(MipsRegLo(Opcode.rs),MipsRegLo(Opcode.rt));
				if (Section->Cont.FallThrough) {
					JneLabel32 (  0 );
					Section->Jump.LinkLocation = RecompPos - 4;
				} else if (Section->Jump.FallThrough) {
					JeLabel32 (  0 );
					Section->Cont.LinkLocation = RecompPos - 4;
				} else {
					JeLabel32 (  0 );
					Section->Cont.LinkLocation = RecompPos - 4;
					JmpLabel32(0);
					Section->Jump.LinkLocation = RecompPos - 4;
				}
			}
		} else {
			uint32_t ConstReg = IsConst(Opcode.rt)?Opcode.rt:Opcode.rs;
			uint32_t MappedReg = IsConst(Opcode.rt)?Opcode.rs:Opcode.rt;

			if (Is64Bit(ConstReg) || Is64Bit(MappedReg)) {
				if (Is32Bit(ConstReg) || Is32Bit(MappedReg)) {
					ProtectGPR(Section,MappedReg);
					if (Is32Bit(MappedReg)) {
						CompConstToX86reg(Map_TempReg(Section,x86_Any,MappedReg,1),MipsRegHi(ConstReg));
					} else {
						CompConstToX86reg(MipsRegHi(MappedReg),(int32_t)MipsRegLo(ConstReg) >> 31);
					}
				} else {
					CompConstToX86reg(MipsRegHi(MappedReg),MipsRegHi(ConstReg));
				}
				if (Section->Jump.FallThrough) {
					JneLabel8(0);
					Jump = RecompPos - 1;
				} else {
					JneLabel32(0);
					Section->Jump.LinkLocation = RecompPos - 4;
				}
				if (MipsRegLo(ConstReg) == 0) {
					OrX86RegToX86Reg(MipsRegLo(MappedReg),MipsRegLo(MappedReg));
				} else {
					CompConstToX86reg(MipsRegLo(MappedReg),MipsRegLo(ConstReg));
				}
				if (Section->Cont.FallThrough) {
					JneLabel32 (  0 );
					Section->Jump.LinkLocation2 = RecompPos - 4;
				} else if (Section->Jump.FallThrough) {
					JeLabel32 (  0 );
					Section->Cont.LinkLocation = RecompPos - 4;


					SetJump8(Jump,RecompPos);
				} else {
					JeLabel32 (  0 );
					Section->Cont.LinkLocation = RecompPos - 4;
					JmpLabel32(0);
					Section->Jump.LinkLocation2 = RecompPos - 4;
				}
			} else {
				if (MipsRegLo(ConstReg) == 0) {
					OrX86RegToX86Reg(MipsRegLo(MappedReg),MipsRegLo(MappedReg));
				} else {
					CompConstToX86reg(MipsRegLo(MappedReg),MipsRegLo(ConstReg));
				}
				if (Section->Cont.FallThrough) {
					JneLabel32 (  0 );
					Section->Jump.LinkLocation = RecompPos - 4;
				} else if (Section->Jump.FallThrough) {
					JeLabel32 (  0 );
					Section->Cont.LinkLocation = RecompPos - 4;
				} else {
					JeLabel32 (  0 );
					Section->Cont.LinkLocation = RecompPos - 4;
					JmpLabel32(0);
					Section->Jump.LinkLocation = RecompPos - 4;
				}
			}
		}
	} else if (IsKnown(Opcode.rs) || IsKnown(Opcode.rt)) {
		uint32_t KnownReg = IsKnown(Opcode.rt)?Opcode.rt:Opcode.rs;
		uint32_t UnknownReg = IsKnown(Opcode.rt)?Opcode.rs:Opcode.rt;

		if (IsConst(KnownReg)) {
			if (Is64Bit(KnownReg)) {
				CompConstToVariable(MipsRegHi(KnownReg),&GPR[UnknownReg].W[1]);
			} else if (IsSigned(KnownReg)) {
				CompConstToVariable(((int32_t)MipsRegLo(KnownReg) >> 31),&GPR[UnknownReg].W[1]);
			} else {
				CompConstToVariable(0,&GPR[UnknownReg].W[1]);
			}
		} else {
			if (Is64Bit(KnownReg)) {
				CompX86regToVariable(MipsRegHi(KnownReg),&GPR[UnknownReg].W[1]);
			} else if (IsSigned(KnownReg)) {
				ProtectGPR(Section,KnownReg);
				CompX86regToVariable(Map_TempReg(Section,x86_Any,KnownReg,1),&GPR[UnknownReg].W[1]);
			} else {
				CompConstToVariable(0,&GPR[UnknownReg].W[1]);
			}
		}
		if (Section->Jump.FallThrough) {
			JneLabel8(0);
			Jump = RecompPos - 1;
		} else {
			JneLabel32(0);
			Section->Jump.LinkLocation = RecompPos - 4;
		}
		if (IsConst(KnownReg)) {
			CompConstToVariable(MipsRegLo(KnownReg),&GPR[UnknownReg].W[0]);
		} else {
			CompX86regToVariable(MipsRegLo(KnownReg),&GPR[UnknownReg].W[0]);
		}
		if (Section->Cont.FallThrough) {
			JneLabel32 (  0 );
			Section->Jump.LinkLocation2 = RecompPos - 4;
		} else if (Section->Jump.FallThrough) {
			JeLabel32 (  0 );
			Section->Cont.LinkLocation = RecompPos - 4;


			SetJump8(Jump,RecompPos);
		} else {
			JeLabel32 (  0 );
			Section->Cont.LinkLocation = RecompPos - 4;
			JmpLabel32(0);
			Section->Jump.LinkLocation2 = RecompPos - 4;
		}
	} else {
		int32_t x86Reg;

		x86Reg = Map_TempReg(Section,x86_Any,Opcode.rt,1);
		CompX86regToVariable(x86Reg,&GPR[Opcode.rs].W[1]);
		if (Section->Jump.FallThrough) {
			JneLabel8(0);
			Jump = RecompPos - 1;
		} else {
			JneLabel32(0);
			Section->Jump.LinkLocation = RecompPos - 4;
		}

		x86Reg = Map_TempReg(Section,x86Reg,Opcode.rt,0);
		CompX86regToVariable(x86Reg,&GPR[Opcode.rs].W[0]);
		if (Section->Cont.FallThrough) {
			JneLabel32 (  0 );
			Section->Jump.LinkLocation2 = RecompPos - 4;
		} else if (Section->Jump.FallThrough) {
			JeLabel32 (  0 );
			Section->Cont.LinkLocation = RecompPos - 4;


			SetJump8(Jump,RecompPos);
		} else {
			JeLabel32 (  0 );
			Section->Cont.LinkLocation = RecompPos - 4;
			JmpLabel32(0);
			Section->Jump.LinkLocation2 = RecompPos - 4;
		}
	}
}

void BEQ_Compare (BLOCK_SECTION * Section) {
	uint8_t *Jump;

	if (IsKnown(Opcode.rs) && IsKnown(Opcode.rt)) {
		if (IsConst(Opcode.rs) && IsConst(Opcode.rt)) {
			if (Is64Bit(Opcode.rs) || Is64Bit(Opcode.rt)) {
				Compile_R4300i_UnknownOpcode(Section);
			} else if (MipsRegLo(Opcode.rs) == MipsRegLo(Opcode.rt)) {
				Section->Jump.FallThrough = 1;
				Section->Cont.FallThrough = 0;
			} else {
				Section->Jump.FallThrough = 0;
				Section->Cont.FallThrough = 1;
			}
		} else if (IsMapped(Opcode.rs) && IsMapped(Opcode.rt)) {
			if (Is64Bit(Opcode.rs) || Is64Bit(Opcode.rt)) {
				ProtectGPR(Section,Opcode.rs);
				ProtectGPR(Section,Opcode.rt);

				CompX86RegToX86Reg(
					Is32Bit(Opcode.rs)?Map_TempReg(Section,x86_Any,Opcode.rs,1):MipsRegHi(Opcode.rs),
					Is32Bit(Opcode.rt)?Map_TempReg(Section,x86_Any,Opcode.rt,1):MipsRegHi(Opcode.rt)
				);
				if (Section->Cont.FallThrough) {
					JneLabel8(0);
					Jump = RecompPos - 1;
				} else {
					JneLabel32(0);
					Section->Cont.LinkLocation = RecompPos - 4;
				}
				CompX86RegToX86Reg(MipsRegLo(Opcode.rs),MipsRegLo(Opcode.rt));
				if (Section->Cont.FallThrough) {
					JeLabel32 (  0 );
					Section->Jump.LinkLocation = RecompPos - 4;


					SetJump8(Jump,RecompPos);
				} else if (Section->Jump.FallThrough) {
					JneLabel32 (  0 );
					Section->Cont.LinkLocation2 = RecompPos - 4;
				} else {
					JneLabel32 (  0 );
					Section->Cont.LinkLocation2 = RecompPos - 4;
					JmpLabel32(0);
					Section->Jump.LinkLocation = RecompPos - 4;
				}
			} else {
				CompX86RegToX86Reg(MipsRegLo(Opcode.rs),MipsRegLo(Opcode.rt));
				if (Section->Cont.FallThrough) {
					JeLabel32 (  0 );
					Section->Jump.LinkLocation = RecompPos - 4;
				} else if (Section->Jump.FallThrough) {
					JneLabel32 (  0 );
					Section->Cont.LinkLocation = RecompPos - 4;
				} else {
					JneLabel32 (  0 );
					Section->Cont.LinkLocation = RecompPos - 4;
					JmpLabel32(0);
					Section->Jump.LinkLocation = RecompPos - 4;
				}
			}
		} else {
			uint32_t ConstReg = IsConst(Opcode.rt)?Opcode.rt:Opcode.rs;
			uint32_t MappedReg = IsConst(Opcode.rt)?Opcode.rs:Opcode.rt;

			if (Is64Bit(ConstReg) || Is64Bit(MappedReg)) {
				if (Is32Bit(ConstReg) || Is32Bit(MappedReg)) {
					if (Is32Bit(MappedReg)) {
						ProtectGPR(Section,MappedReg);
						CompConstToX86reg(Map_TempReg(Section,x86_Any,MappedReg,1),MipsRegHi(ConstReg));
					} else {
						CompConstToX86reg(MipsRegHi(MappedReg),(int32_t)MipsRegLo(ConstReg) >> 31);
					}
				} else {
					CompConstToX86reg(MipsRegHi(MappedReg),MipsRegHi(ConstReg));
				}
				if (Section->Cont.FallThrough) {
					JneLabel8(0);
					Jump = RecompPos - 1;
				} else {
					JneLabel32(0);
					Section->Cont.LinkLocation = RecompPos - 4;
				}
				if (MipsRegLo(ConstReg) == 0) {
					OrX86RegToX86Reg(MipsRegLo(MappedReg),MipsRegLo(MappedReg));
				} else {
					CompConstToX86reg(MipsRegLo(MappedReg),MipsRegLo(ConstReg));
				}
				if (Section->Cont.FallThrough) {
					JeLabel32 (  0 );
					Section->Jump.LinkLocation = RecompPos - 4;


					SetJump8(Jump,RecompPos);
				} else if (Section->Jump.FallThrough) {
					JneLabel32 (  0 );
					Section->Cont.LinkLocation2 = RecompPos - 4;
				} else {
					JneLabel32 (  0 );
					Section->Cont.LinkLocation2 = RecompPos - 4;
					JmpLabel32(0);
					Section->Jump.LinkLocation = RecompPos - 4;
				}
			} else {
				if (MipsRegLo(ConstReg) == 0) {
					OrX86RegToX86Reg(MipsRegLo(MappedReg),MipsRegLo(MappedReg));
				} else {
					CompConstToX86reg(MipsRegLo(MappedReg),MipsRegLo(ConstReg));
				}
				if (Section->Cont.FallThrough) {
					JeLabel32 (  0 );
					Section->Jump.LinkLocation = RecompPos - 4;
				} else if (Section->Jump.FallThrough) {
					JneLabel32 (  0 );
					Section->Cont.LinkLocation = RecompPos - 4;
				} else {
					JneLabel32 (  0 );
					Section->Cont.LinkLocation = RecompPos - 4;
					JmpLabel32(0);
					Section->Jump.LinkLocation = RecompPos - 4;
				}
			}
		}
	} else if (IsKnown(Opcode.rs) || IsKnown(Opcode.rt)) {
		uint32_t KnownReg = IsKnown(Opcode.rt)?Opcode.rt:Opcode.rs;
		uint32_t UnknownReg = IsKnown(Opcode.rt)?Opcode.rs:Opcode.rt;

		if (IsConst(KnownReg)) {
			if (Is64Bit(KnownReg)) {
				CompConstToVariable(MipsRegHi(KnownReg),&GPR[UnknownReg].W[1]);
			} else if (IsSigned(KnownReg)) {
				CompConstToVariable((int32_t)MipsRegLo(KnownReg) >> 31,&GPR[UnknownReg].W[1]);
			} else {
				CompConstToVariable(0,&GPR[UnknownReg].W[1]);
			}
		} else {
			ProtectGPR(Section,KnownReg);
			if (Is64Bit(KnownReg)) {
				CompX86regToVariable(MipsRegHi(KnownReg),&GPR[UnknownReg].W[1]);
			} else if (IsSigned(KnownReg)) {
				CompX86regToVariable(Map_TempReg(Section,x86_Any,KnownReg,1),&GPR[UnknownReg].W[1]);
			} else {
				CompConstToVariable(0,&GPR[UnknownReg].W[1]);
			}
		}
		if (Section->Cont.FallThrough) {
			JneLabel8(0);
			Jump = RecompPos - 1;
		} else {
			JneLabel32(0);
			Section->Cont.LinkLocation = RecompPos - 4;
		}
		if (IsConst(KnownReg)) {
			CompConstToVariable(MipsRegLo(KnownReg),&GPR[UnknownReg].W[0]);
		} else {
			CompX86regToVariable(MipsRegLo(KnownReg),&GPR[UnknownReg].W[0]);
		}
		if (Section->Cont.FallThrough) {
			JeLabel32 (  0 );
			Section->Jump.LinkLocation = RecompPos - 4;


			SetJump8(Jump,RecompPos);
		} else if (Section->Jump.FallThrough) {
			JneLabel32 (  0 );
			Section->Cont.LinkLocation2 = RecompPos - 4;
		} else {
			JneLabel32 (  0 );
			Section->Cont.LinkLocation2 = RecompPos - 4;
			JmpLabel32(0);
			Section->Jump.LinkLocation = RecompPos - 4;
		}
	} else {
		int32_t x86Reg = Map_TempReg(Section,x86_Any,Opcode.rs,1);
		CompX86regToVariable(x86Reg,&GPR[Opcode.rt].W[1]);
		if (Section->Cont.FallThrough) {
			JneLabel8(0);
			Jump = RecompPos - 1;
		} else {
			JneLabel32(0);
			Section->Cont.LinkLocation = RecompPos - 4;
		}
		CompX86regToVariable(Map_TempReg(Section,x86Reg,Opcode.rs,0),&GPR[Opcode.rt].W[0]);
		if (Section->Cont.FallThrough) {
			JeLabel32 (  0 );
			Section->Jump.LinkLocation = RecompPos - 4;


			SetJump8(Jump,RecompPos);
		} else if (Section->Jump.FallThrough) {
			JneLabel32 (  0 );
			Section->Cont.LinkLocation2 = RecompPos - 4;
		} else {
			JneLabel32 (  0 );
			Section->Cont.LinkLocation2 = RecompPos - 4;
			JmpLabel32(0);
			Section->Jump.LinkLocation = RecompPos - 4;
		}
	}
}

void BGTZ_Compare (BLOCK_SECTION * Section) {
	if (IsConst(Opcode.rs)) {
		if (Is64Bit(Opcode.rs)) {
			if (MipsReg_S(Opcode.rs) > 0) {
				Section->Jump.FallThrough = 1;
				Section->Cont.FallThrough = 0;
			} else {
				Section->Jump.FallThrough = 0;
				Section->Cont.FallThrough = 1;
			}
		} else {
			if (MipsRegLo_S(Opcode.rs) > 0) {
				Section->Jump.FallThrough = 1;
				Section->Cont.FallThrough = 0;
			} else {
				Section->Jump.FallThrough = 0;
				Section->Cont.FallThrough = 1;
			}
		}
	} else if (IsMapped(Opcode.rs) && Is32Bit(Opcode.rs)) {
		CompConstToX86reg(MipsRegLo(Opcode.rs),0);
		if (Section->Jump.FallThrough) {
			JleLabel32 ( 0 );
			Section->Cont.LinkLocation = RecompPos - 4;
		} else if (Section->Cont.FallThrough) {
			JgLabel32(0);
			Section->Jump.LinkLocation = RecompPos - 4;
		} else {
			JleLabel32 ( 0 );
			Section->Cont.LinkLocation = RecompPos - 4;
			JmpLabel32(0);
			Section->Jump.LinkLocation = RecompPos - 4;
		}
	} else {
		uint8_t *Jump;

		if (IsMapped(Opcode.rs)) {
			CompConstToX86reg(MipsRegHi(Opcode.rs),0);
		} else {
			CompConstToVariable(0,&GPR[Opcode.rs].W[1]);
		}
		if (Section->Jump.FallThrough) {
			JlLabel32 ( 0 );
			Section->Cont.LinkLocation = RecompPos - 4;
			JgLabel8(0);
			Jump = RecompPos - 1;
		} else if (Section->Cont.FallThrough) {
			JlLabel8(0);
			Jump = RecompPos - 1;
			JgLabel32(0);
			Section->Jump.LinkLocation = RecompPos - 4;
		} else {
			JlLabel32 ( 0 );
			Section->Cont.LinkLocation = RecompPos - 4;
			JgLabel32(0);
			Section->Jump.LinkLocation = RecompPos - 4;
		}

		if (IsMapped(Opcode.rs)) {
			CompConstToX86reg(MipsRegLo(Opcode.rs),0);
		} else {
			CompConstToVariable(0,&GPR[Opcode.rs].W[0]);
		}
		if (Section->Jump.FallThrough) {
			JeLabel32 ( 0 );
			Section->Cont.LinkLocation2 = RecompPos - 4;

			*((uint8_t *)(Jump))=(uint8_t)(RecompPos - Jump - 1);
		} else if (Section->Cont.FallThrough) {
			JneLabel32(0);
			Section->Jump.LinkLocation = RecompPos - 4;

			*((uint8_t *)(Jump))=(uint8_t)(RecompPos - Jump - 1);
		} else {
			JneLabel32(0);
			Section->Jump.LinkLocation = RecompPos - 4;
			JmpLabel32 ( 0 );
			Section->Cont.LinkLocation2 = RecompPos - 4;
		}
	}
}

void BLEZ_Compare (BLOCK_SECTION * Section) {
	if (IsConst(Opcode.rs)) {
		if (Is64Bit(Opcode.rs)) {
			if (MipsReg_S(Opcode.rs) <= 0) {
				Section->Jump.FallThrough = 1;
				Section->Cont.FallThrough = 0;
			} else {
				Section->Jump.FallThrough = 0;
				Section->Cont.FallThrough = 1;
			}
		} else if (IsSigned(Opcode.rs)) {
			if (MipsRegLo_S(Opcode.rs) <= 0) {
				Section->Jump.FallThrough = 1;
				Section->Cont.FallThrough = 0;
			} else {
				Section->Jump.FallThrough = 0;
				Section->Cont.FallThrough = 1;
			}
		} else {
			if (MipsRegLo(Opcode.rs) == 0) {
				Section->Jump.FallThrough = 1;
				Section->Cont.FallThrough = 0;
			} else {
				Section->Jump.FallThrough = 0;
				Section->Cont.FallThrough = 1;
			}
		}
	} else {
		if (IsMapped(Opcode.rs) && Is32Bit(Opcode.rs)) {
			CompConstToX86reg(MipsRegLo(Opcode.rs),0);
			if (Section->Jump.FallThrough) {
				JgLabel32 ( 0 );
				Section->Cont.LinkLocation = RecompPos - 4;
			} else if (Section->Cont.FallThrough) {
				JleLabel32(0);
				Section->Jump.LinkLocation = RecompPos - 4;
			} else {
				JgLabel32 ( 0 );
				Section->Cont.LinkLocation = RecompPos - 4;
				JmpLabel32(0);
				Section->Jump.LinkLocation = RecompPos - 4;
			}
		} else {
			uint8_t *Jump;

			if (IsMapped(Opcode.rs)) {
				CompConstToX86reg(MipsRegHi(Opcode.rs),0);
			} else {
				CompConstToVariable(0,&GPR[Opcode.rs].W[1]);
			}
			if (Section->Jump.FallThrough) {
				JgLabel32 ( 0 );
				Section->Cont.LinkLocation = RecompPos - 4;
				JlLabel8(0);
				Jump = RecompPos - 1;
			} else if (Section->Cont.FallThrough) {
				JgLabel8(0);
				Jump = RecompPos - 1;
				JlLabel32(0);
				Section->Jump.LinkLocation = RecompPos - 4;
			} else {
				JgLabel32 ( 0 );
				Section->Cont.LinkLocation = RecompPos - 4;
				JlLabel32(0);
				Section->Jump.LinkLocation = RecompPos - 4;
			}

			if (IsMapped(Opcode.rs)) {
				CompConstToX86reg(MipsRegLo(Opcode.rs),0);
			} else {
				CompConstToVariable(0,&GPR[Opcode.rs].W[0]);
			}
			if (Section->Jump.FallThrough) {
				JneLabel32 ( 0 );
				Section->Cont.LinkLocation2 = RecompPos - 4;

				*((uint8_t *)(Jump))=(uint8_t)(RecompPos - Jump - 1);
			} else if (Section->Cont.FallThrough) {
				JeLabel32 ( 0 );
				Section->Jump.LinkLocation2 = RecompPos - 4;

				*((uint8_t *)(Jump))=(uint8_t)(RecompPos - Jump - 1);
			} else {
				JneLabel32 ( 0 );
				Section->Cont.LinkLocation2 = RecompPos - 4;
				JmpLabel32(0);
				Section->Jump.LinkLocation2 = RecompPos - 4;
			}
		}
	}
}

void BLTZ_Compare (BLOCK_SECTION * Section) {
	if (IsConst(Opcode.rs)) {
		if (Is64Bit(Opcode.rs)) {
			if (MipsReg_S(Opcode.rs) < 0) {
				Section->Jump.FallThrough = 1;
				Section->Cont.FallThrough = 0;
			} else {
				Section->Jump.FallThrough = 0;
				Section->Cont.FallThrough = 1;
			}
		} else if (IsSigned(Opcode.rs)) {
			if (MipsRegLo_S(Opcode.rs) < 0) {
				Section->Jump.FallThrough = 1;
				Section->Cont.FallThrough = 0;
			} else {
				Section->Jump.FallThrough = 0;
				Section->Cont.FallThrough = 1;
			}
		} else {
			Section->Jump.FallThrough = 0;
			Section->Cont.FallThrough = 1;
		}
	} else if (IsMapped(Opcode.rs)) {
		if (Is64Bit(Opcode.rs)) {
			CompConstToX86reg(MipsRegHi(Opcode.rs),0);
			if (Section->Jump.FallThrough) {
				JgeLabel32 ( 0 );
				Section->Cont.LinkLocation = RecompPos - 4;
			} else if (Section->Cont.FallThrough) {
				JlLabel32(0);
				Section->Jump.LinkLocation = RecompPos - 4;
			} else {
				JgeLabel32 ( 0 );
				Section->Cont.LinkLocation = RecompPos - 4;
				JmpLabel32(0);
				Section->Jump.LinkLocation = RecompPos - 4;
			}
		} else if (IsSigned(Opcode.rs)) {
			CompConstToX86reg(MipsRegLo(Opcode.rs),0);
			if (Section->Jump.FallThrough) {
				JgeLabel32 ( 0 );
				Section->Cont.LinkLocation = RecompPos - 4;
			} else if (Section->Cont.FallThrough) {
				JlLabel32(0);
				Section->Jump.LinkLocation = RecompPos - 4;
			} else {
				JgeLabel32 ( 0 );
				Section->Cont.LinkLocation = RecompPos - 4;
				JmpLabel32(0);
				Section->Jump.LinkLocation = RecompPos - 4;
			}
		} else {
			Section->Jump.FallThrough = 0;
			Section->Cont.FallThrough = 1;
		}
	} else if (IsUnknown(Opcode.rs)) {
		CompConstToVariable(0,&GPR[Opcode.rs].W[1]);
		if (Section->Jump.FallThrough) {
			JgeLabel32 ( 0 );
			Section->Cont.LinkLocation = RecompPos - 4;
		} else if (Section->Cont.FallThrough) {
			JlLabel32(0);
			Section->Jump.LinkLocation = RecompPos - 4;
		} else {
			JlLabel32(0);
			Section->Jump.LinkLocation = RecompPos - 4;
			JmpLabel32 ( 0 );
			Section->Cont.LinkLocation = RecompPos - 4;
		}
	}
}

void BGEZ_Compare (BLOCK_SECTION * Section) {
	if (IsConst(Opcode.rs)) {
		if (Is64Bit(Opcode.rs)) {
#ifndef EXTERNAL_RELEASE
			DisplayError("BGEZ 1");
#endif
			Compile_R4300i_UnknownOpcode(Section);
		} else if IsSigned(Opcode.rs) {
			if (MipsRegLo_S(Opcode.rs) >= 0) {
				Section->Jump.FallThrough = 1;
				Section->Cont.FallThrough = 0;
			} else {
				Section->Jump.FallThrough = 0;
				Section->Cont.FallThrough = 1;
			}
		} else {
			Section->Jump.FallThrough = 1;
			Section->Cont.FallThrough = 0;
		}
	} else if (IsMapped(Opcode.rs)) {
		if (Is64Bit(Opcode.rs)) {
			CompConstToX86reg(MipsRegHi(Opcode.rs),0);
			if (Section->Cont.FallThrough) {
				JgeLabel32 (  0 );
				Section->Jump.LinkLocation = RecompPos - 4;
			} else if (Section->Jump.FallThrough) {
				JlLabel32 (  0 );
				Section->Cont.LinkLocation = RecompPos - 4;
			} else {
				JlLabel32 (  0 );
				Section->Cont.LinkLocation = RecompPos - 4;
				JmpLabel32(0);
				Section->Jump.LinkLocation = RecompPos - 4;
			}
		} else if (IsSigned(Opcode.rs)) {
			CompConstToX86reg(MipsRegLo(Opcode.rs),0);
			if (Section->Cont.FallThrough) {
				JgeLabel32 (  0 );
				Section->Jump.LinkLocation = RecompPos - 4;
			} else if (Section->Jump.FallThrough) {
				JlLabel32 (  0 );
				Section->Cont.LinkLocation = RecompPos - 4;
			} else {
				JlLabel32 (  0 );
				Section->Cont.LinkLocation = RecompPos - 4;
				JmpLabel32(0);
				Section->Jump.LinkLocation = RecompPos - 4;
			}
		} else {
			Section->Jump.FallThrough = 1;
			Section->Cont.FallThrough = 0;
		}
	} else {
		CompConstToVariable(0,&GPR[Opcode.rs].W[1]);
		if (Section->Cont.FallThrough) {
			JgeLabel32 (  0 );
			Section->Jump.LinkLocation = RecompPos - 4;
		} else if (Section->Jump.FallThrough) {
			JlLabel32 (  0 );
			Section->Cont.LinkLocation = RecompPos - 4;
		} else {
			JlLabel32 (  0 );
			Section->Cont.LinkLocation = RecompPos - 4;
			JmpLabel32(0);
			Section->Jump.LinkLocation = RecompPos - 4;
		}
	}
}

void COP1_BCF_Compare (BLOCK_SECTION * Section) {
	TestVariable(FPCSR_C,&FPCR[31]);
	if (Section->Cont.FallThrough) {
		JeLabel32 (  0 );
		Section->Jump.LinkLocation = RecompPos - 4;
	} else if (Section->Jump.FallThrough) {
		JneLabel32 (  0 );
		Section->Cont.LinkLocation = RecompPos - 4;
	} else {
		JneLabel32 (  0 );
		Section->Cont.LinkLocation = RecompPos - 4;
		JmpLabel32(0);
		Section->Jump.LinkLocation = RecompPos - 4;
	}
}

void COP1_BCT_Compare (BLOCK_SECTION * Section) {
	TestVariable(FPCSR_C,&FPCR[31]);
	if (Section->Cont.FallThrough) {
		JneLabel32 (  0 );
		Section->Jump.LinkLocation = RecompPos - 4;
	} else if (Section->Jump.FallThrough) {
		JeLabel32 (  0 );
		Section->Cont.LinkLocation = RecompPos - 4;
	} else {
		JeLabel32 (  0 );
		Section->Cont.LinkLocation = RecompPos - 4;
		JmpLabel32(0);
		Section->Jump.LinkLocation = RecompPos - 4;
	}
}
/*************************  OpCode functions *************************/
void Compile_R4300i_J (BLOCK_SECTION * Section) {
	if ( NextInstruction == NORMAL ) {
		Section->Jump.TargetPC      = (Section->CompilePC & 0xF0000000) + (Opcode.target << 2);;
		Section->Jump.FallThrough   = 1;
		Section->Jump.LinkLocation  = NULL;
		Section->Jump.LinkLocation2 = NULL;
		NextInstruction = DO_DELAY_SLOT;
		if ((Section->CompilePC & 0xFFC) == 0xFFC) {
			memcpy(&Section->Jump.RegSet,&Section->RegWorking,sizeof(REG_INFO));
			GenerateSectionLinkage(Section);
			NextInstruction = END_BLOCK;
		}
	} else if (NextInstruction == DELAY_SLOT_DONE ) {
		memcpy(&Section->Jump.RegSet,&Section->RegWorking,sizeof(REG_INFO));
		GenerateSectionLinkage(Section);
		NextInstruction = END_BLOCK;
	} else {
#ifndef EXTERNAL_RELEASE
		DisplayError("WTF\n\nJal\nNextInstruction = %X", NextInstruction);
#endif
	}
}

void Compile_R4300i_JAL (BLOCK_SECTION * Section) {

	//UnMap_TempReg(Section);
	if ( NextInstruction == NORMAL ) {

		UnMap_GPR(Section, 31, 0);
		MipsRegLo(31) = Section->CompilePC + 8;
		MipsRegState(31) = STATE_CONST_32;
		if ((Section->CompilePC & 0xFFC) == 0xFFC) {
			MoveConstToVariable((Section->CompilePC & 0xF0000000) + (Opcode.target << 2),&JumpToLocation);
			MoveConstToVariable(Section->CompilePC + 4,&PROGRAM_COUNTER);
			if (BlockCycleCount != 0) {
				AddConstToVariable(BlockCycleCount,&CP0[9]);
				SubConstFromVariable(BlockCycleCount,&Timers->Timer);
			}
			if (BlockRandomModifier != 0) { SubConstFromVariable(BlockRandomModifier,&CP0[1]); }
			WriteBackRegisters(Section);
			MoveConstToVariable(DELAY_SLOT,&NextInstruction);
			Ret();
			NextInstruction = END_BLOCK;
			return;
		}
		NextInstruction = DO_DELAY_SLOT;
	} else if (NextInstruction == DELAY_SLOT_DONE ) {
		MoveConstToVariable((Section->CompilePC & 0xF0000000) + (Opcode.target << 2),&PROGRAM_COUNTER);
		CompileExit((uint32_t)-1,&Section->RegWorking,Normal,1,NULL);
		NextInstruction = END_BLOCK;
	} else {
#ifndef EXTERNAL_RELEASE
		DisplayError("WTF\n\nBranch\nNextInstruction = %X", NextInstruction);
#endif
	}
	return;

	if ( NextInstruction == NORMAL ) {


		UnMap_GPR(Section, 31, 0);
		MipsRegLo(31) = Section->CompilePC + 8;
		MipsRegState(31) = STATE_CONST_32;
		NextInstruction = DO_DELAY_SLOT;

		Section->Jump.TargetPC      = (Section->CompilePC & 0xF0000000) + (Opcode.target << 2);
		Section->Jump.FallThrough   = 1;
		Section->Jump.LinkLocation  = NULL;
		Section->Jump.LinkLocation2 = NULL;
		if ((Section->CompilePC & 0xFFC) == 0xFFC) {
			memcpy(&Section->Jump.RegSet,&Section->RegWorking,sizeof(REG_INFO));
			GenerateSectionLinkage(Section);
			NextInstruction = END_BLOCK;
		}
	} else if (NextInstruction == DELAY_SLOT_DONE ) {
		memcpy(&Section->Jump.RegSet,&Section->RegWorking,sizeof(REG_INFO));
		GenerateSectionLinkage(Section);
		NextInstruction = END_BLOCK;
	} else {
#ifndef EXTERNAL_RELEASE
		DisplayError("WTF\n\nJal\nNextInstruction = %X", NextInstruction);
#endif
	}
}

void Compile_R4300i_ADDI (BLOCK_SECTION * Section) {

	if (Opcode.rt == 0) { return; }
	if (IsConst(Opcode.rs)) {
		if (IsMapped(Opcode.rt)) { UnMap_GPR(Section,Opcode.rt, 0); }
		MipsRegLo(Opcode.rt) = MipsRegLo(Opcode.rs) + (short)Opcode.immediate;
		MipsRegState(Opcode.rt) = STATE_CONST_32;
		return;
	}
	Map_GPR_32bit(Section,Opcode.rt,1,Opcode.rs);
	if (Opcode.immediate == 0) {
	} else if (Opcode.immediate == 1) {
		IncX86reg(MipsRegLo(Opcode.rt));
	} else if (Opcode.immediate == 0xFFFF) {
		DecX86reg(MipsRegLo(Opcode.rt));
	} else {
		AddConstToX86Reg(MipsRegLo(Opcode.rt),(short)Opcode.immediate);
	}

}

void Compile_R4300i_ADDIU (BLOCK_SECTION * Section) {



	if (Opcode.rt == 0) { return; }

	if (IsConst(Opcode.rs)) {
		if (IsMapped(Opcode.rt)) { UnMap_GPR(Section,Opcode.rt, 0); }
		MipsRegLo(Opcode.rt) = MipsRegLo(Opcode.rs) + (short)Opcode.immediate;
		MipsRegState(Opcode.rt) = STATE_CONST_32;
		return;
	}
	Map_GPR_32bit(Section,Opcode.rt,1,Opcode.rs);
	if (Opcode.immediate == 0) {
	} else if (Opcode.immediate == 1) {
		IncX86reg(MipsRegLo(Opcode.rt));
	} else if (Opcode.immediate == 0xFFFF) {
		DecX86reg(MipsRegLo(Opcode.rt));
	} else {
		AddConstToX86Reg(MipsRegLo(Opcode.rt),(short)Opcode.immediate);
	}
}

void Compile_R4300i_SLTIU (BLOCK_SECTION * Section) {

	if (Opcode.rt == 0) { return; }

	if (IsConst(Opcode.rs)) {
		uint32_t Result;

		if (Is64Bit(Opcode.rs)) {
			int64_t Immediate = (int64_t)((short)Opcode.immediate);
			Result = MipsReg(Opcode.rs) < ((unsigned)(Immediate))?1:0;
		} else if (Is32Bit(Opcode.rs)) {
			Result = MipsRegLo(Opcode.rs) < ((unsigned)((short)Opcode.immediate))?1:0;
		}
		UnMap_GPR(Section,Opcode.rt, 0);
		MipsRegState(Opcode.rt) = STATE_CONST_32;
		MipsRegLo(Opcode.rt) = Result;
	} else if (IsMapped(Opcode.rs)) {
		if (Is64Bit(Opcode.rs)) {
			uint8_t * Jump[2];

			CompConstToX86reg(MipsRegHi(Opcode.rs),((short)Opcode.immediate >> 31));
			JeLabel8(0);
			Jump[0] = RecompPos - 1;
			SetbVariable(&BranchCompare);
			JmpLabel8(0);
			Jump[1] = RecompPos - 1;


			*((uint8_t *)(Jump[0]))=(uint8_t)(RecompPos - Jump[0] - 1);
			CompConstToX86reg(MipsRegLo(Opcode.rs),(short)Opcode.immediate);
			SetbVariable(&BranchCompare);


			*((uint8_t *)(Jump[1]))=(uint8_t)(RecompPos - Jump[1] - 1);
			Map_GPR_32bit(Section,Opcode.rt,0, -1);
			MoveVariableToX86reg(&BranchCompare,MipsRegLo(Opcode.rt));
		} else {
			CompConstToX86reg(MipsRegLo(Opcode.rs),(short)Opcode.immediate);
			SetbVariable(&BranchCompare);
			Map_GPR_32bit(Section,Opcode.rt,0, -1);
			MoveVariableToX86reg(&BranchCompare,MipsRegLo(Opcode.rt));
		}
	} else {
		uint8_t * Jump;

		CompConstToVariable(((short)Opcode.immediate >> 31),&GPR[Opcode.rs].W[1]);
		JneLabel8(0);
		Jump = RecompPos - 1;
		CompConstToVariable((short)Opcode.immediate,&GPR[Opcode.rs].W[0]);


		*((uint8_t *)(Jump))=(uint8_t)(RecompPos - Jump - 1);
		SetbVariable(&BranchCompare);
		Map_GPR_32bit(Section,Opcode.rt,0, -1);
		MoveVariableToX86reg(&BranchCompare,MipsRegLo(Opcode.rt));


		/*SetbVariable(&BranchCompare);
		JmpLabel8(0);
		Jump[1] = RecompPos - 1;


		*((uint8_t *)(Jump[0]))=(uint8_t)(RecompPos - Jump[0] - 1);
		CompConstToVariable((short)Opcode.immediate,&GPR[Opcode.rs].W[0]);
		SetbVariable(&BranchCompare);


		*((uint8_t *)(Jump[1]))=(uint8_t)(RecompPos - Jump[1] - 1);
		Map_GPR_32bit(Section,Opcode.rt,0, -1);
		MoveVariableToX86reg(&BranchCompare,MipsRegLo(Opcode.rt));*/
	}
}

void Compile_R4300i_SLTI (BLOCK_SECTION * Section) {

	if (Opcode.rt == 0) { return; }

	if (IsConst(Opcode.rs)) {
		uint32_t Result;

		if (Is64Bit(Opcode.rs)) {
			int64_t Immediate = (int64_t)((short)Opcode.immediate);
			Result = (int64_t)MipsReg(Opcode.rs) < Immediate?1:0;
		} else if (Is32Bit(Opcode.rs)) {
			Result = MipsRegLo_S(Opcode.rs) < (short)Opcode.immediate?1:0;
		}
		UnMap_GPR(Section,Opcode.rt, 0);
		MipsRegState(Opcode.rt) = STATE_CONST_32;
		MipsRegLo(Opcode.rt) = Result;
	} else if (IsMapped(Opcode.rs)) {
		if (Is64Bit(Opcode.rs)) {
			uint8_t * Jump[2];

			CompConstToX86reg(MipsRegHi(Opcode.rs),((short)Opcode.immediate >> 31));
			JeLabel8(0);
			Jump[0] = RecompPos - 1;
			SetlVariable(&BranchCompare);
			JmpLabel8(0);
			Jump[1] = RecompPos - 1;


			*((uint8_t *)(Jump[0]))=(uint8_t)(RecompPos - Jump[0] - 1);
			CompConstToX86reg(MipsRegLo(Opcode.rs),(short)Opcode.immediate);
			SetbVariable(&BranchCompare);


			*((uint8_t *)(Jump[1]))=(uint8_t)(RecompPos - Jump[1] - 1);
			Map_GPR_32bit(Section,Opcode.rt,0, -1);
			MoveVariableToX86reg(&BranchCompare,MipsRegLo(Opcode.rt));
		} else {
		/*	CompConstToX86reg(MipsRegLo(Opcode.rs),(short)Opcode.immediate);
			SetlVariable(&BranchCompare);
			Map_GPR_32bit(Section,Opcode.rt,0, -1);
			MoveVariableToX86reg(&BranchCompare,MipsRegLo(Opcode.rt));
			*/
			ProtectGPR(Section, Opcode.rs);
			Map_GPR_32bit(Section,Opcode.rt,0, -1);
			CompConstToX86reg(MipsRegLo(Opcode.rs),(short)Opcode.immediate);

			if (MipsRegLo(Opcode.rt) > x86_EDX) {
				SetlVariable(&BranchCompare);
				MoveVariableToX86reg(&BranchCompare,MipsRegLo(Opcode.rt));
			} else {
				Setl(MipsRegLo(Opcode.rt));
				AndConstToX86Reg(MipsRegLo(Opcode.rt), 1);
			}
		}
	} else {
		uint8_t * Jump[2];

		CompConstToVariable(((short)Opcode.immediate >> 31),&GPR[Opcode.rs].W[1]);
		JeLabel8(0);
		Jump[0] = RecompPos - 1;
		SetlVariable(&BranchCompare);
		JmpLabel8(0);
		Jump[1] = RecompPos - 1;


		*((uint8_t *)(Jump[0]))=(uint8_t)(RecompPos - Jump[0] - 1);
		CompConstToVariable((short)Opcode.immediate,&GPR[Opcode.rs].W[0]);
		SetbVariable(&BranchCompare);


		*((uint8_t *)(Jump[1]))=(uint8_t)(RecompPos - Jump[1] - 1);
		Map_GPR_32bit(Section,Opcode.rt,0, -1);
		MoveVariableToX86reg(&BranchCompare,MipsRegLo(Opcode.rt));
	}
}

void Compile_R4300i_ANDI (BLOCK_SECTION * Section) {


	if (Opcode.rt == 0) { return;}

	if (IsConst(Opcode.rs)) {
		if (IsMapped(Opcode.rt)) { UnMap_GPR(Section,Opcode.rt, 0); }
		MipsRegState(Opcode.rt) = STATE_CONST_32;
		MipsRegLo(Opcode.rt) = MipsRegLo(Opcode.rs) & Opcode.immediate;
	} else if (Opcode.immediate != 0) {
		Map_GPR_32bit(Section,Opcode.rt,0,Opcode.rs);
		AndConstToX86Reg(MipsRegLo(Opcode.rt),Opcode.immediate);
	} else {
		Map_GPR_32bit(Section,Opcode.rt,0,0);
	}
}

void Compile_R4300i_ORI (BLOCK_SECTION * Section) {

	if (Opcode.rt == 0) { return;}

	if (IsConst(Opcode.rs)) {
		if (IsMapped(Opcode.rt)) { UnMap_GPR(Section,Opcode.rt, 0); }
		MipsRegState(Opcode.rt) = MipsRegState(Opcode.rs);
		MipsRegHi(Opcode.rt) = MipsRegHi(Opcode.rs);
		MipsRegLo(Opcode.rt) = MipsRegLo(Opcode.rs) | Opcode.immediate;
	} else if (IsMapped(Opcode.rs)) {
		if (Is64Bit(Opcode.rs)) {
			Map_GPR_64bit(Section,Opcode.rt,Opcode.rs);
		} else {
			Map_GPR_32bit(Section,Opcode.rt,IsSigned(Opcode.rs),Opcode.rs);
		}
		OrConstToX86Reg(Opcode.immediate,MipsRegLo(Opcode.rt));
	} else {
		Map_GPR_64bit(Section,Opcode.rt,Opcode.rs);
		OrConstToX86Reg(Opcode.immediate,MipsRegLo(Opcode.rt));
	}
}

void Compile_R4300i_XORI (BLOCK_SECTION * Section) {

	if (Opcode.rt == 0) { return;}

	if (IsConst(Opcode.rs)) {
		if (Opcode.rs != Opcode.rt) { UnMap_GPR(Section,Opcode.rt, 0); }
		MipsRegState(Opcode.rt) = MipsRegState(Opcode.rs);
		MipsRegHi(Opcode.rt) = MipsRegHi(Opcode.rs);
		MipsRegLo(Opcode.rt) = MipsRegLo(Opcode.rs) ^ Opcode.immediate;
	} else {
		if (IsMapped(Opcode.rs) && Is32Bit(Opcode.rs)) {
			Map_GPR_32bit(Section,Opcode.rt,IsSigned(Opcode.rs),Opcode.rs);
		} else {
			Map_GPR_64bit(Section,Opcode.rt,Opcode.rs);
		}
		if (Opcode.immediate != 0) { XorConstToX86Reg(MipsRegLo(Opcode.rt),Opcode.immediate); }
	}
}

void Compile_R4300i_LUI (BLOCK_SECTION * Section) {

	if (Opcode.rt == 0) { return;}

	UnMap_GPR(Section,Opcode.rt, 0);
	MipsRegLo(Opcode.rt) = ((short)Opcode.offset << 16);
	MipsRegState(Opcode.rt) = STATE_CONST_32;
}

void Compile_R4300i_DADDIU (BLOCK_SECTION * Section) {


	if (Opcode.rs != 0) { UnMap_GPR(Section,Opcode.rs,1); }
	if (Opcode.rs != 0) { UnMap_GPR(Section,Opcode.rt,1); }
	Pushad();
	MoveConstToVariable(Opcode.Hex, &Opcode.Hex );
	Call_Direct(r4300i_DADDIU);
	Popad();
}

void Compile_R4300i_LDL (BLOCK_SECTION * Section) {

	if (Opcode.base != 0) { UnMap_GPR(Section,Opcode.base,1); }
	if (Opcode.rt != 0) { UnMap_GPR(Section,Opcode.rt,1); }
	Pushad();
	MoveConstToVariable(Opcode.Hex, &Opcode.Hex );
	Call_Direct(r4300i_LDL);
	Popad();

}

void Compile_R4300i_LDR (BLOCK_SECTION * Section) {

	if (Opcode.base != 0) { UnMap_GPR(Section,Opcode.base,1); }
	if (Opcode.rt != 0) { UnMap_GPR(Section,Opcode.rt,1); }
	Pushad();
	MoveConstToVariable(Opcode.Hex, &Opcode.Hex );
	Call_Direct(r4300i_LDR);
	Popad();
}


void Compile_R4300i_LB (BLOCK_SECTION * Section) {
	uint32_t TempReg1, TempReg2;

	if (Opcode.rt == 0) return;

//	UnMap_TempReg(Section); //hax
	if (IsConst(Opcode.base)) {
		uint32_t Address = (MipsRegLo(Opcode.base) + (short)Opcode.offset) ^ 3;
		Map_GPR_32bit(Section,Opcode.rt,1,0);
		Compile_LB(MipsRegLo(Opcode.rt),Address,1);
		return;
	}
	if (IsMapped(Opcode.rt)) { ProtectGPR(Section,Opcode.rt); }
	if (IsMapped(Opcode.base)) {
		ProtectGPR(Section,Opcode.base);
		if (Opcode.offset != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,0);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.base),(short)Opcode.offset);
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		}
	} else {
		TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		if (Opcode.immediate == 0) {
		} else if (Opcode.immediate == 1) {
			IncX86reg(TempReg1);
		} else if (Opcode.immediate == 0xFFFF) {
			DecX86reg(TempReg1);
		} else {
			AddConstToX86Reg(TempReg1,(short)Opcode.immediate);
		}
	}


	TempReg2 = Map_TempReg(Section,x64_Any,-1,0);
	MoveX86RegToX86Reg(TempReg1, TempReg2);
	ShiftRightUnsignImmed(TempReg2,12);

	LOAD_FROM_TLB(TempReg2, TempReg2);

	CompileReadTLBMiss(Section,TempReg1,TempReg2);
	XorConstToX86Reg(TempReg1|x64_Reg,3);
	Map_GPR_32bit(Section,Opcode.rt,1,-1);
	MoveSxByteX86regPointerToX86reg(TempReg1, TempReg2,MipsRegLo(Opcode.rt));
}


void Compile_R4300i_LH (BLOCK_SECTION * Section) {
	uint32_t TempReg1, TempReg2;



	if (Opcode.rt == 0) return;

	if (IsConst(Opcode.base)) {
		uint32_t Address = (MipsRegLo(Opcode.base) + (short)Opcode.offset) ^ 2;
		Map_GPR_32bit(Section,Opcode.rt,1,0);
		Compile_LH(MipsRegLo(Opcode.rt),Address,1);
		return;
	}
	if (IsMapped(Opcode.rt)) { ProtectGPR(Section,Opcode.rt); }
	if (IsMapped(Opcode.base)) {
		ProtectGPR(Section,Opcode.base);
		if (Opcode.offset != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,0);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.base),(short)Opcode.offset);
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		}
	} else {
		TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		if (Opcode.immediate == 0) {
		} else if (Opcode.immediate == 1) {
			IncX86reg(TempReg1);
		} else if (Opcode.immediate == 0xFFFF) {
			DecX86reg(TempReg1);
		} else {
			AddConstToX86Reg(TempReg1,(short)Opcode.immediate);
		}
	}

	TempReg2 = Map_TempReg(Section,x64_Any,-1,0);
	MoveX86RegToX86Reg(TempReg1, TempReg2);
	ShiftRightUnsignImmed(TempReg2,12);
	LOAD_FROM_TLB(TempReg2, TempReg2);
	CompileReadTLBMiss(Section,TempReg1,TempReg2);
	XorConstToX86Reg(TempReg1,2);
	Map_GPR_32bit(Section,Opcode.rt,1,-1);
	MoveSxHalfX86regPointerToX86reg(TempReg1, TempReg2,MipsRegLo(Opcode.rt));

}

void Compile_R4300i_LWL (BLOCK_SECTION * Section) {
	uint32_t TempReg1, TempReg2, Offset, shift;



	if (Opcode.rt == 0) return;

	if (IsConst(Opcode.base)) {
		uint32_t Address, Value;

		Address = MipsRegLo(Opcode.base) + (short)Opcode.offset;
		Offset  = Address & 3;

		Map_GPR_32bit(Section,Opcode.rt,1,Opcode.rt);
		Value = Map_TempReg(Section,x86_Any,-1,0);
		Compile_LW(Value,(Address & ~3));
		AndConstToX86Reg(MipsRegLo(Opcode.rt),LWL_MASK[Offset]);
		ShiftLeftSignImmed(Value,(uint8_t)LWL_SHIFT[Offset]);
		AddX86RegToX86Reg(MipsRegLo(Opcode.rt),Value);
		return;
	}

	shift = Map_TempReg(Section,x86_ECX,-1,0);
	if (IsMapped(Opcode.rt)) { ProtectGPR(Section,Opcode.rt); }
	if (IsMapped(Opcode.base)) {
		ProtectGPR(Section,Opcode.base);
		if (Opcode.offset != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,0);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.base),(short)Opcode.offset);
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		}
		UnProtectGPR(Section,Opcode.base);
	} else {
		TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		if (Opcode.immediate == 0) {
		} else if (Opcode.immediate == 1) {
			IncX86reg(TempReg1);
		} else if (Opcode.immediate == 0xFFFF) {
			DecX86reg(TempReg1);
		} else {
			AddConstToX86Reg(TempReg1,(short)Opcode.immediate);
		}
	}

	TempReg2 = Map_TempReg(Section,x64_Any,-1,0);
	MoveX86RegToX86Reg(TempReg1, TempReg2);
	ShiftRightUnsignImmed(TempReg2,12);
	LOAD_FROM_TLB(TempReg2, TempReg2);

	CompileReadTLBMiss(Section,TempReg1,TempReg2);

	Offset = Map_TempReg(Section,x64_Any,-1,0);
	MoveX86RegToX86Reg(TempReg1, Offset);
	AndConstToX86Reg(Offset,3);
	AndConstToX86Reg(TempReg1,~3);

	Map_GPR_32bit(Section,Opcode.rt,1,Opcode.rt);
	AndVariableDispToX86Reg(LWL_MASK,MipsRegLo(Opcode.rt),Offset,4);
	MoveVariableDispToX86Reg(LWL_SHIFT,shift,Offset,4);

	MoveX86regPointerToX86reg(TempReg1, TempReg2,TempReg1);

	ShiftLeftSign(TempReg1);
	AddX86RegToX86Reg(MipsRegLo(Opcode.rt),TempReg1);
}

void Compile_R4300i_LW (BLOCK_SECTION * Section) {
	uint32_t TempReg1, TempReg2;

	if (Opcode.rt == 0) return;

	{
		if (IsConst(Opcode.base)) {
			uint32_t Address = MipsRegLo(Opcode.base) + (short)Opcode.offset;
			Map_GPR_32bit(Section,Opcode.rt,1,-1);
			Compile_LW(MipsRegLo(Opcode.rt),Address);
			return;
		}

		if (IsMapped(Opcode.rt)) { ProtectGPR(Section,Opcode.rt); }
		if (IsMapped(Opcode.base) && Opcode.offset == 0) {
			ProtectGPR(Section,Opcode.base);
			TempReg1 = MipsRegLo(Opcode.base);
		} else {
			if (IsMapped(Opcode.base)) {
				ProtectGPR(Section,Opcode.base);
				if (Opcode.offset != 0) {
					TempReg1 = Map_TempReg(Section,x86_Any,-1,0);
					LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.base),(short)Opcode.offset);
				} else {
					TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
				}
			} else {
				TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
				if (Opcode.immediate == 0) {
				} else if (Opcode.immediate == 1) {
					IncX86reg(TempReg1);
				} else if (Opcode.immediate == 0xFFFF) {
					DecX86reg(TempReg1);
				} else {
					AddConstToX86Reg(TempReg1,(short)Opcode.immediate);
				}
			}
		}
		TempReg2 = Map_TempReg(Section,x64_Any,-1,0);
		Map_GPR_32bit(Section,Opcode.rt,1,-1);
		MoveX86RegToX86Reg(TempReg1, TempReg2);
		ShiftRightUnsignImmed(TempReg2,12);
		LOAD_FROM_TLB(TempReg2, TempReg2);
		CompileReadTLBMiss(Section,TempReg1,TempReg2);
		MoveX86regPointerToX86reg(TempReg1, TempReg2,MipsRegLo(Opcode.rt));
	}
}

void Compile_R4300i_LBU (BLOCK_SECTION * Section) {
	uint32_t TempReg1, TempReg2;



	if (Opcode.rt == 0) return;

	if (IsConst(Opcode.base)) {
		uint32_t Address = (MipsRegLo(Opcode.base) + (short)Opcode.offset) ^ 3;
		Map_GPR_32bit(Section,Opcode.rt,0,0);
		Compile_LB(MipsRegLo(Opcode.rt),Address,0);
		return;
	}
	if (IsMapped(Opcode.rt)) { ProtectGPR(Section,Opcode.rt); }
	if (IsMapped(Opcode.base)) {
		ProtectGPR(Section,Opcode.base);
		if (Opcode.offset != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,0);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.base),(short)Opcode.offset);
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		}
	} else {
		TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		if (Opcode.immediate == 0) {
		} else if (Opcode.immediate == 1) {
			IncX86reg(TempReg1);
		} else if (Opcode.immediate == 0xFFFF) {
			DecX86reg(TempReg1);
		} else {
			AddConstToX86Reg(TempReg1,(short)Opcode.immediate);
		}
	}

	TempReg2 = Map_TempReg(Section,x64_Any,-1,0);
	MoveX86RegToX86Reg(TempReg1, TempReg2);
	ShiftRightUnsignImmed(TempReg2,12);
	LOAD_FROM_TLB(TempReg2, TempReg2);
	CompileReadTLBMiss(Section,TempReg1,TempReg2);
	XorConstToX86Reg(TempReg1,3);
	Map_GPR_32bit(Section,Opcode.rt,0,-1);
	MoveZxByteX86regPointerToX86reg(TempReg1, TempReg2,MipsRegLo(Opcode.rt));

}

void Compile_R4300i_LHU (BLOCK_SECTION * Section) {
	uint32_t TempReg1, TempReg2;



	if (Opcode.rt == 0) return;

	if (IsConst(Opcode.base)) {
		uint32_t Address = (MipsRegLo(Opcode.base) + (short)Opcode.offset) ^ 2;
		Map_GPR_32bit(Section,Opcode.rt,0,0);
		Compile_LH(MipsRegLo(Opcode.rt),Address,0);
		return;
	}
	if (IsMapped(Opcode.rt)) { ProtectGPR(Section,Opcode.rt); }
	if (IsMapped(Opcode.base)) {
		ProtectGPR(Section,Opcode.base);
		if (Opcode.offset != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,0);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.base),(short)Opcode.offset);
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		}
	} else {
		TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		if (Opcode.immediate == 0) {
		} else if (Opcode.immediate == 1) {
			IncX86reg(TempReg1);
		} else if (Opcode.immediate == 0xFFFF) {
			DecX86reg(TempReg1);
		} else {
			AddConstToX86Reg(TempReg1,(short)Opcode.immediate);
		}
	}

	TempReg2 = Map_TempReg(Section,x64_Any,-1,0);
	MoveX86RegToX86Reg(TempReg1, TempReg2);
	ShiftRightUnsignImmed(TempReg2,12);
	LOAD_FROM_TLB(TempReg2, TempReg2);
	CompileReadTLBMiss(Section,TempReg1,TempReg2);
	XorConstToX86Reg(TempReg1,2);
	Map_GPR_32bit(Section,Opcode.rt,0,-1);
	MoveZxHalfX86regPointerToX86reg(TempReg1, TempReg2,MipsRegLo(Opcode.rt));

}

void Compile_R4300i_LWR (BLOCK_SECTION * Section) {
	uint32_t TempReg1, TempReg2, Offset, shift;



	if (Opcode.rt == 0) return;

	if (IsConst(Opcode.base)) {
		uint32_t Address, Value;

		Address = MipsRegLo(Opcode.base) + (short)Opcode.offset;
		Offset  = Address & 3;

		Map_GPR_32bit(Section,Opcode.rt,1,Opcode.rt);
		Value = Map_TempReg(Section,x86_Any,-1,0);
		Compile_LW(Value,(Address & ~3));
		AndConstToX86Reg(MipsRegLo(Opcode.rt),LWR_MASK[Offset]);
		ShiftRightUnsignImmed(Value,(uint8_t)LWR_SHIFT[Offset]);
		AddX86RegToX86Reg(MipsRegLo(Opcode.rt),Value);
		return;
	}

	shift = Map_TempReg(Section,x86_ECX,-1,0);
	if (IsMapped(Opcode.rt)) { ProtectGPR(Section,Opcode.rt); }
	if (IsMapped(Opcode.base)) {
		ProtectGPR(Section,Opcode.base);
		if (Opcode.offset != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,0);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.base),(short)Opcode.offset);
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		}
		UnProtectGPR(Section,Opcode.base);
	} else {
		TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		if (Opcode.immediate == 0) {
		} else if (Opcode.immediate == 1) {
			IncX86reg(TempReg1);
		} else if (Opcode.immediate == 0xFFFF) {
			DecX86reg(TempReg1);
		} else {
			AddConstToX86Reg(TempReg1,(short)Opcode.immediate);
		}
	}


	TempReg2 = Map_TempReg(Section,x64_Any,-1,0);
	MoveX86RegToX86Reg(TempReg1, TempReg2);
	ShiftRightUnsignImmed(TempReg2,12);
	LOAD_FROM_TLB(TempReg2, TempReg2);

	CompileReadTLBMiss(Section,TempReg1,TempReg2);

	Offset = Map_TempReg(Section,x64_Any,-1,0);
    //BreakPoint();
	MoveX86RegToX86Reg(TempReg1, Offset);
	AndConstToX86Reg(Offset,3);
	AndConstToX86Reg(TempReg1,~3);

	Map_GPR_32bit(Section,Opcode.rt,1,Opcode.rt);
	AndVariableDispToX86Reg(LWR_MASK,MipsRegLo(Opcode.rt),Offset,4);
	MoveVariableDispToX86Reg(LWR_SHIFT,shift,Offset,4);

	MoveX86regPointerToX86reg(TempReg1, TempReg2,TempReg1);

	ShiftRightUnsign(TempReg1);
	AddX86RegToX86Reg(MipsRegLo(Opcode.rt),TempReg1);
}

void Compile_R4300i_LWU (BLOCK_SECTION * Section) {			//added by Witten
	uint32_t TempReg1, TempReg2;



	if (Opcode.rt == 0) return;

	if (IsConst(Opcode.base)) {
		uint32_t Address = (MipsRegLo(Opcode.base) + (short)Opcode.offset);
		Map_GPR_32bit(Section,Opcode.rt,0,0);
		Compile_LW(MipsRegLo(Opcode.rt),Address);
		return;
	}
	if (IsMapped(Opcode.rt)) { ProtectGPR(Section,Opcode.rt); }
	if (IsMapped(Opcode.base)) {
		ProtectGPR(Section,Opcode.base);
		if (Opcode.offset != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,0);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.base),(short)Opcode.offset);
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		}
	} else {
		TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		if (Opcode.immediate == 0) {
		} else if (Opcode.immediate == 1) {
			IncX86reg(TempReg1);
		} else if (Opcode.immediate == 0xFFFF) {
			DecX86reg(TempReg1);
		} else {
			AddConstToX86Reg(TempReg1,(short)Opcode.immediate);
		}
	}

	TempReg2 = Map_TempReg(Section,x64_Any,-1,0);
	MoveX86RegToX86Reg(TempReg1, TempReg2);
	ShiftRightUnsignImmed(TempReg2,12);
	//MoveVariableDispToX86Reg(TLB_Map,TempReg2,TempReg2,8);
	LOAD_FROM_TLB(TempReg2, TempReg2);
	CompileReadTLBMiss(Section,TempReg1,TempReg2);
	Map_GPR_32bit(Section,Opcode.rt,0,-1);
	MoveZxHalfX86regPointerToX86reg(TempReg1, TempReg2,MipsRegLo(Opcode.rt));

}

void Compile_R4300i_SB (BLOCK_SECTION * Section){
	uint32_t TempReg1, TempReg2;



	if (IsConst(Opcode.base)) {
		uint32_t Address = (MipsRegLo(Opcode.base) + (short)Opcode.offset) ^ 3;

		if (IsConst(Opcode.rt)) {
			Compile_SB_Const((uint8_t)MipsRegLo(Opcode.rt), Address);
		} else if (IsMapped(Opcode.rt) && Is8BitReg(MipsRegLo(Opcode.rt))) {
			Compile_SB_Register(MipsRegLo(Opcode.rt), Address);
		} else {
			Compile_SB_Register(Map_TempReg(Section,x86_Any8Bit,Opcode.rt,0), Address);
		}
		return;
	}
	if (IsMapped(Opcode.rt)) { ProtectGPR(Section,Opcode.rt); }
	if (IsMapped(Opcode.base)) {
		ProtectGPR(Section,Opcode.base);
		if (Opcode.offset != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,0);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.base),(short)Opcode.offset);
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		}
		UnProtectGPR(Section,Opcode.base);
	} else {
		TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		if (Opcode.immediate == 0) {
		} else if (Opcode.immediate == 1) {
			IncX86reg(TempReg1);
		} else if (Opcode.immediate == 0xFFFF) {
			DecX86reg(TempReg1);
		} else {
			AddConstToX86Reg(TempReg1,(short)Opcode.immediate);
		}
	}

	TempReg2 = Map_TempReg(Section,x64_Any,-1,0);
	MoveX86RegToX86Reg(TempReg1, TempReg2);
	ShiftRightUnsignImmed(TempReg2,12);
	//MoveVariableDispToX86Reg(TLB_Map,TempReg2,TempReg2,8);
	LOAD_FROM_TLB(TempReg2, TempReg2);
	//For tlb miss
	//0041C522 85 C0                test        eax,eax
	//0041C524 75 01                jne         0041C527

	XorConstToX86Reg(TempReg1 | x64_Reg,3);
	if (IsConst(Opcode.rt)) {
		MoveConstByteToX86regPointer((uint8_t)MipsRegLo(Opcode.rt),TempReg1, TempReg2);
	} else if (IsMapped(Opcode.rt) && Is8BitReg(MipsRegLo(Opcode.rt))) {
		MoveX86regByteToX86regPointer(MipsRegLo(Opcode.rt),TempReg1, TempReg2);
	} else {

		UnProtectGPR(Section,Opcode.rt);
		MoveX86regByteToX86regPointer(Map_TempReg(Section,x86_Any8Bit,Opcode.rt,0),TempReg1, TempReg2);
	}

}

void Compile_R4300i_SH (BLOCK_SECTION * Section){
	uint32_t TempReg1, TempReg2;



	if (IsConst(Opcode.base)) {
		uint32_t Address = (MipsRegLo(Opcode.base) + (short)Opcode.offset) ^ 2;

		if (IsConst(Opcode.rt)) {
			Compile_SH_Const((uint16_t)MipsRegLo(Opcode.rt), Address);
		} else if (IsMapped(Opcode.rt)) {
			Compile_SH_Register(MipsRegLo(Opcode.rt), Address);
		} else {
			Compile_SH_Register(Map_TempReg(Section,x86_Any,Opcode.rt,0), Address);
		}
		return;
	}
	if (IsMapped(Opcode.rt)) { ProtectGPR(Section,Opcode.rt); }
	if (IsMapped(Opcode.base)) {
		ProtectGPR(Section,Opcode.base);
		if (Opcode.offset != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,0);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.base),(short)Opcode.offset);
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		}
		UnProtectGPR(Section,Opcode.base);
	} else {
		TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		if (Opcode.immediate == 0) {
		} else if (Opcode.immediate == 1) {
			IncX86reg(TempReg1);
		} else if (Opcode.immediate == 0xFFFF) {
			DecX86reg(TempReg1);
		} else {
			AddConstToX86Reg(TempReg1,(short)Opcode.immediate);
		}
	}

	TempReg2 = Map_TempReg(Section,x64_Any,-1,0);
	MoveX86RegToX86Reg(TempReg1, TempReg2);
	ShiftRightUnsignImmed(TempReg2,12);
	//MoveVariableDispToX86Reg(TLB_Map,TempReg2,TempReg2,8);
	LOAD_FROM_TLB(TempReg2, TempReg2);
	//For tlb miss
	//0041C522 85 C0                test        eax,eax
	//0041C524 75 01                jne         0041C527

	XorConstToX86Reg(TempReg1,2);
	if (IsConst(Opcode.rt)) {
		MoveConstHalfToX86regPointer((uint16_t)MipsRegLo(Opcode.rt),TempReg1, TempReg2);
	} else if (IsMapped(Opcode.rt)) {
		MoveX86regHalfToX86regPointer(MipsRegLo(Opcode.rt),TempReg1, TempReg2);
	} else {
		MoveX86regHalfToX86regPointer(Map_TempReg(Section,x86_Any,Opcode.rt,0),TempReg1, TempReg2);
	}

}

void Compile_R4300i_SWL (BLOCK_SECTION * Section) {
	uint32_t TempReg1, TempReg2, Value, Offset, shift;



	if (IsConst(Opcode.base)) {
		uint32_t Address;

		Address = MipsRegLo(Opcode.base) + (short)Opcode.offset;
		Offset  = Address & 3;

		Value = Map_TempReg(Section,x86_Any,-1,0);
		Compile_LW(Value,(Address & ~3));
		AndConstToX86Reg(Value,SWL_MASK[Offset]);
		TempReg1 = Map_TempReg(Section,x86_Any,Opcode.rt,0);
		ShiftRightUnsignImmed(TempReg1,(uint8_t)SWL_SHIFT[Offset]);
		AddX86RegToX86Reg(Value,TempReg1);
		Compile_SW_Register(Value, (Address & ~3));
		return;
	}
	shift = Map_TempReg(Section,x86_ECX,-1,0);
	if (IsMapped(Opcode.base)) {
		ProtectGPR(Section,Opcode.base);
		if (Opcode.offset != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,0);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.base),(short)Opcode.offset);
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		}
		UnProtectGPR(Section,Opcode.base);
	} else {
		TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		if (Opcode.immediate == 0) {
		} else if (Opcode.immediate == 1) {
			IncX86reg(TempReg1);
		} else if (Opcode.immediate == 0xFFFF) {
			DecX86reg(TempReg1);
		} else {
			AddConstToX86Reg(TempReg1,(short)Opcode.immediate);
		}
	}

	TempReg2 = Map_TempReg(Section,x64_Any,-1,0);
	MoveX86RegToX86Reg(TempReg1, TempReg2);
	ShiftRightUnsignImmed(TempReg2,12);
	//MoveVariableDispToX86Reg(TLB_Map,TempReg2,TempReg2,8);
	LOAD_FROM_TLB(TempReg2, TempReg2);

		//For tlb miss
		//0041C522 85 C0                test        eax,eax
		//0041C524 75 01                jne         0041C527

	Offset = Map_TempReg(Section,x64_Any,-1,0);
	MoveX86RegToX86Reg(TempReg1, Offset);
	AndConstToX86Reg(Offset,3);
	AndConstToX86Reg(TempReg1,~3);

	Value = Map_TempReg(Section,x86_Any,-1,0);

	MoveX86regPointerToX86reg(TempReg1, TempReg2,Value);


	AndVariableDispToX86Reg(SWL_MASK,Value,Offset,4);
	if (!IsConst(Opcode.rt) || MipsRegLo(Opcode.rt) != 0) {
		MoveVariableDispToX86Reg(SWL_SHIFT,shift,Offset,4);
		if (IsConst(Opcode.rt)) {
			MoveConstToX86reg(MipsRegLo(Opcode.rt),Offset);
		} else if (IsMapped(Opcode.rt)) {
			MoveX86RegToX86Reg(MipsRegLo(Opcode.rt),Offset);
		} else {
			MoveVariableToX86reg(&GPR[Opcode.rt].UW[0],Offset);
		}
		ShiftRightUnsign(Offset);
		AddX86RegToX86Reg(Value,Offset);
	}


	MoveX86RegToX86Reg(TempReg1, TempReg2);
	ShiftRightUnsignImmed(TempReg2,12);
	//MoveVariableDispToX86Reg(TLB_Map,TempReg2,TempReg2,8);
	LOAD_FROM_TLB(TempReg2, TempReg2);

	MoveX86regToX86regPointer(Value,TempReg1, TempReg2);

}

void Compile_R4300i_SW (BLOCK_SECTION * Section) {
	uint32_t TempReg1, TempReg2;
	uint8_t * JumpPos = 0, * JumpPos2 = 0;

	{
		if (IsConst(Opcode.base)) {
			uint32_t Address = MipsRegLo(Opcode.base) + (short)Opcode.offset;


			if (IsConst(Opcode.rt)) {
				Compile_SW_Const(MipsRegLo(Opcode.rt), Address);
			} else if (IsMapped(Opcode.rt)) {
				Compile_SW_Register(MipsRegLo(Opcode.rt), Address);
			} else {
				Compile_SW_Register(Map_TempReg(Section,x86_Any,Opcode.rt,0), Address);
			}
			return;
		}
		if (IsMapped(Opcode.rt)) { ProtectGPR(Section,Opcode.rt); }
		if (IsMapped(Opcode.base)) {
			ProtectGPR(Section,Opcode.base);
			if (Opcode.offset != 0) {
				TempReg1 = Map_TempReg(Section,x86_Any,-1,0);
				LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.base),(short)Opcode.offset);
			} else {
				TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
			}
			UnProtectGPR(Section,Opcode.base);
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
			if (Opcode.immediate == 0) {
			} else if (Opcode.immediate == 1) {
				IncX86reg(TempReg1);
			} else if (Opcode.immediate == 0xFFFF) {
				DecX86reg(TempReg1);
			} else {
				AddConstToX86Reg(TempReg1,(short)Opcode.immediate);
			}
		}

		TempReg2 = Map_TempReg(Section,x64_Any,-1,0);
		MoveX86RegToX86Reg(TempReg1, TempReg2);
		ShiftRightUnsignImmed(TempReg2,12);
		LOAD_FROM_TLB(TempReg2, TempReg2);
		//For tlb miss
		//0041C522 85 C0                test        eax,eax
		//0041C524 75 01                jne         0041C527

		if (IsConst(Opcode.rt)) {
			MoveConstToX86regPointer(MipsRegLo(Opcode.rt),TempReg1, TempReg2);
		} else if (IsMapped(Opcode.rt)) {
			MoveX86regToX86regPointer(MipsRegLo(Opcode.rt),TempReg1, TempReg2);
		} else {
			MoveX86regToX86regPointer(Map_TempReg(Section,x86_Any,Opcode.rt,0),TempReg1, TempReg2);
		}

	}
}

void Compile_R4300i_SWR (BLOCK_SECTION * Section) {
	uint32_t TempReg1, TempReg2, Value, Offset, shift;



	if (IsConst(Opcode.base)) {
		uint32_t Address;

		Address = MipsRegLo(Opcode.base) + (short)Opcode.offset;
		Offset  = Address & 3;

		Value = Map_TempReg(Section,x86_Any,-1,0);
		Compile_LW(Value,(Address & ~3));
		AndConstToX86Reg(Value,SWR_MASK[Offset]);
		TempReg1 = Map_TempReg(Section,x86_Any,Opcode.rt,0);
		ShiftLeftSignImmed(TempReg1,(uint8_t)SWR_SHIFT[Offset]);
		AddX86RegToX86Reg(Value,TempReg1);
		Compile_SW_Register(Value, (Address & ~3));
		return;
	}
	shift = Map_TempReg(Section,x86_ECX,-1,0);
	if (IsMapped(Opcode.base)) {
		ProtectGPR(Section,Opcode.base);
		if (Opcode.offset != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,0);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.base),(short)Opcode.offset);
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		}
		UnProtectGPR(Section,Opcode.base);
	} else {
		TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		if (Opcode.immediate == 0) {
		} else if (Opcode.immediate == 1) {
			IncX86reg(TempReg1);
		} else if (Opcode.immediate == 0xFFFF) {
			DecX86reg(TempReg1);
		} else {
			AddConstToX86Reg(TempReg1,(short)Opcode.immediate);
		}
	}

	TempReg2 = Map_TempReg(Section,x64_Any,-1,0);
	MoveX86RegToX86Reg(TempReg1, TempReg2);
	ShiftRightUnsignImmed(TempReg2,12);
	//MoveVariableDispToX86Reg(TLB_Map,TempReg2,TempReg2,8);
	LOAD_FROM_TLB(TempReg2, TempReg2);

		//For tlb miss
		//0041C522 85 C0                test        eax,eax
		//0041C524 75 01                jne         0041C527

	Offset = Map_TempReg(Section,x64_Any,-1,0);
	MoveX86RegToX86Reg(TempReg1, Offset);
	AndConstToX86Reg(Offset,3);
	AndConstToX86Reg(TempReg1,~3);

	Value = Map_TempReg(Section,x86_Any,-1,0);

	MoveX86regPointerToX86reg(TempReg1, TempReg2,Value);

	AndVariableDispToX86Reg(SWR_MASK,Value,Offset,4);
	if (!IsConst(Opcode.rt) || MipsRegLo(Opcode.rt) != 0) {
		MoveVariableDispToX86Reg(SWR_SHIFT,shift,Offset,4);
		if (IsConst(Opcode.rt)) {
			MoveConstToX86reg(MipsRegLo(Opcode.rt),Offset);
		} else if (IsMapped(Opcode.rt)) {
			MoveX86RegToX86Reg(MipsRegLo(Opcode.rt),Offset);
		} else {
			MoveVariableToX86reg(&GPR[Opcode.rt].UW[0],Offset);
		}
		ShiftLeftSign(Offset);
		AddX86RegToX86Reg(Value,Offset);
	}



	MoveX86RegToX86Reg(TempReg1, TempReg2);
	ShiftRightUnsignImmed(TempReg2,12);
	//MoveVariableDispToX86Reg(TLB_Map,TempReg2,TempReg2,8);
	LOAD_FROM_TLB(TempReg2, TempReg2);

	MoveX86regToX86regPointer(Value,TempReg1, TempReg2);

}

void Compile_R4300i_SDL (BLOCK_SECTION * Section) {

	if (Opcode.base != 0) { UnMap_GPR(Section,Opcode.base,1); }
	if (Opcode.rt != 0) { UnMap_GPR(Section,Opcode.rt,1); }
	Pushad();
	MoveConstToVariable(Opcode.Hex, &Opcode.Hex );
	Call_Direct(r4300i_SDL);
	Popad();

}

void Compile_R4300i_SDR (BLOCK_SECTION * Section) {

	if (Opcode.base != 0) { UnMap_GPR(Section,Opcode.base,1); }
	if (Opcode.rt != 0) { UnMap_GPR(Section,Opcode.rt,1); }
	Pushad();
	MoveConstToVariable(Opcode.Hex, &Opcode.Hex );
	Call_Direct(r4300i_SDR);
	Popad();

}

void  ClearRecomplierCache (uint32_t Address) {
	if (!TranslateVaddr(&Address)) { DisplayError("Cache: Failed to translate: %X",Address); return; }
	if (Address < RdramSize) {
		uint32_t Block = Address >> 12;
		if (N64_Blocks.NoOfRDRamBlocks[Block] > 0) {
			N64_Blocks.NoOfRDRamBlocks[Block] = 0;
			memset(JumpTable + (Block << 10),0,0x1000);
			*(DelaySlotTable + Block) = NULL;
		}
	} else {
#ifndef EXTERNAL_RELEASE
		DisplayError("ClearRecomplierCache: %X",Address);
#endif
	}
}

void Compile_R4300i_CACHE (BLOCK_SECTION * Section){
/*
	if (SelfModCheck != ModCode_Cache && SelfModCheck != ModCode_ChangeMemory && SelfModCheck != ModCode_CheckMemory2 && SelfModCheck != ModCode_CheckMemoryCache) {
		return;
	}

	switch(Opcode.rt) {
	case 0:
	case 16:
		Pushad();
		if (IsConst(Opcode.base)) {
			uint32_t Address = MipsRegLo(Opcode.base) + (short)Opcode.offset;
			MoveConstToX86reg(Address,x86_ECX);
		} else if (IsMapped(Opcode.base)) {
			if (MipsRegLo(Opcode.base) == x86_ECX) {
				AddConstToX86Reg(x86_ECX,(short)Opcode.offset);
			} else {
				LeaSourceAndOffset(x86_ECX,MipsRegLo(Opcode.base),(short)Opcode.offset);
			}
		} else {
			MoveVariableToX86reg(&GPR[Opcode.base].UW[0],x86_ECX);
			AddConstToX86Reg(x86_ECX,(short)Opcode.offset);
		}
		Call_Direct(ClearRecomplierCache);
		Popad();
		break;
	case 1:
	case 3:
	case 13:
	case 5:
	case 8:
	case 9:
	case 17:
	case 21:
	case 25:
		break;
#ifndef EXTERNAL_RELEASE
	default:
		DisplayError("cache: %d",Opcode.rt);
#endif
	}
	*/
}

void Compile_R4300i_LL (BLOCK_SECTION * Section) {
	uint32_t TempReg1, TempReg2;



	if (Opcode.rt == 0) return;

	if (IsConst(Opcode.base)) {
		uint32_t Address = MipsRegLo(Opcode.base) + (short)Opcode.offset;
		Map_GPR_32bit(Section,Opcode.rt,1,-1);
		Compile_LW(MipsRegLo(Opcode.rt),Address);
		MoveConstToVariable(1,&LLBit);
		TranslateVaddr(&Address);
		MoveConstToVariable(Address,&LLAddr);
		return;
	}

	if (IsMapped(Opcode.rt)) { ProtectGPR(Section,Opcode.rt); }
	if (IsMapped(Opcode.base) && Opcode.offset == 0) {
		ProtectGPR(Section,Opcode.base);
		TempReg1 = MipsRegLo(Opcode.base);
	} else {
		if (IsMapped(Opcode.base)) {
			ProtectGPR(Section,Opcode.base);
			if (Opcode.offset != 0) {
				TempReg1 = Map_TempReg(Section,x86_Any,-1,0);
				LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.base),(short)Opcode.offset);
			} else {
				TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
			}
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
			if (Opcode.immediate == 0) {
			} else if (Opcode.immediate == 1) {
				IncX86reg(TempReg1);
			} else if (Opcode.immediate == 0xFFFF) {
				DecX86reg(TempReg1);
			} else {
				AddConstToX86Reg(TempReg1,(short)Opcode.immediate);
			}
		}
	}
	TempReg2 = Map_TempReg(Section,x64_Any,-1,0);
	MoveX86RegToX86Reg(TempReg1, TempReg2);
	ShiftRightUnsignImmed(TempReg2,12);
	//MoveVariableDispToX86Reg(TLB_Map,TempReg2,TempReg2,8);
	LOAD_FROM_TLB(TempReg2, TempReg2);
	CompileReadTLBMiss(Section,TempReg1,TempReg2);
	Map_GPR_32bit(Section,Opcode.rt,1,-1);
	MoveX86regPointerToX86reg(TempReg1, TempReg2,MipsRegLo(Opcode.rt));
	MoveConstToVariable(1,&LLBit);
	MoveX86regToVariable(TempReg1,&LLAddr);
	AddX86regToVariable(TempReg2,&LLAddr);
	SubConstFromVariable((uintptr_t)N64MEM,&LLAddr);

}

void Compile_R4300i_SC (BLOCK_SECTION * Section){
	uint32_t TempReg1, TempReg2;
	uint8_t * Jump;



	CompConstToVariable(1,&LLBit);
	JneLabel32(0);
	Jump = RecompPos - 4;
	if (IsConst(Opcode.base)) {
		uint32_t Address = MipsRegLo(Opcode.base) + (short)Opcode.offset;

		if (IsConst(Opcode.rt)) {
			Compile_SW_Const(MipsRegLo(Opcode.rt), Address);
		} else if (IsMapped(Opcode.rt)) {
			Compile_SW_Register(MipsRegLo(Opcode.rt), Address);
		} else {
			Compile_SW_Register(Map_TempReg(Section,x86_Any,Opcode.rt,0), Address);
		}

		*((uint32_t *)(Jump))=(uint8_t)(RecompPos - Jump - 4);
		Map_GPR_32bit(Section,Opcode.rt,0,-1);
		MoveVariableToX86reg(&LLBit,MipsRegLo(Opcode.rt));
		return;
	}
	if (IsMapped(Opcode.rt)) { ProtectGPR(Section,Opcode.rt); }
	if (IsMapped(Opcode.base)) {
		ProtectGPR(Section,Opcode.base);
		if (Opcode.offset != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,0);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.base),(short)Opcode.offset);
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		}
		UnProtectGPR(Section,Opcode.base);
	} else {
		TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		if (Opcode.immediate == 0) {
		} else if (Opcode.immediate == 1) {
			IncX86reg(TempReg1);
		} else if (Opcode.immediate == 0xFFFF) {
			DecX86reg(TempReg1);
		} else {
			AddConstToX86Reg(TempReg1,(short)Opcode.immediate);
		}
	}

	TempReg2 = Map_TempReg(Section,x64_Any,-1,0);
	MoveX86RegToX86Reg(TempReg1, TempReg2);
	ShiftRightUnsignImmed(TempReg2,12);
	//MoveVariableDispToX86Reg(TLB_Map,TempReg2,TempReg2,8);
	LOAD_FROM_TLB(TempReg2, TempReg2);
	//For tlb miss
	//0041C522 85 C0                test        eax,eax
	//0041C524 75 01                jne         0041C527

	if (IsConst(Opcode.rt)) {
		MoveConstToX86regPointer(MipsRegLo(Opcode.rt),TempReg1, TempReg2);
	} else if (IsMapped(Opcode.rt)) {
		MoveX86regToX86regPointer(MipsRegLo(Opcode.rt),TempReg1, TempReg2);
	} else {
		MoveX86regToX86regPointer(Map_TempReg(Section,x86_Any,Opcode.rt,0),TempReg1, TempReg2);
	}


	*((uint32_t *)(Jump))=(uint8_t)(RecompPos - Jump - 4);
	Map_GPR_32bit(Section,Opcode.rt,0,-1);
	MoveVariableToX86reg(&LLBit,MipsRegLo(Opcode.rt));

}

void Compile_R4300i_LD (BLOCK_SECTION * Section) {
	uint32_t TempReg1, TempReg2;



	if (Opcode.rt == 0) return;

	if (IsConst(Opcode.base)) {
		uint32_t Address = MipsRegLo(Opcode.base) + (short)Opcode.offset;
		Map_GPR_64bit(Section,Opcode.rt,-1);
		Compile_LW(MipsRegHi(Opcode.rt),Address);
		Compile_LW(MipsRegLo(Opcode.rt),Address + 4);
		return;
	}
	if (IsMapped(Opcode.rt)) { ProtectGPR(Section,Opcode.rt); }
	if (IsMapped(Opcode.base) && Opcode.offset == 0) {

	ProtectGPR(Section,Opcode.base);
	TempReg1 = MipsRegLo(Opcode.base);

	} else {
		if (IsMapped(Opcode.base)) {
			ProtectGPR(Section,Opcode.base);
			if (Opcode.offset != 0) {
				TempReg1 = Map_TempReg(Section,x86_Any,-1,0);
				LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.base),(short)Opcode.offset);
			} else {
				TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
			}
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
			if (Opcode.immediate == 0) {
			} else if (Opcode.immediate == 1) {
				IncX86reg(TempReg1);
			} else if (Opcode.immediate == 0xFFFF) {
				DecX86reg(TempReg1);
			} else {
				AddConstToX86Reg(TempReg1,(short)Opcode.immediate);
			}
		}
	}

	TempReg2 = Map_TempReg(Section,x64_Any,-1,0);
	MoveX86RegToX86Reg(TempReg1, TempReg2);
	ShiftRightUnsignImmed(TempReg2,12);
	//MoveVariableDispToX86Reg(TLB_Map,TempReg2,TempReg2,8);
	LOAD_FROM_TLB(TempReg2, TempReg2);
	//For tlb miss
	//0041C522 85 C0                test        eax,eax
	//0041C524 75 01                jne         0041C527
	Map_GPR_64bit(Section,Opcode.rt,-1);
	MoveX86regPointerToX86reg(TempReg1, TempReg2,MipsRegHi(Opcode.rt));
	MoveX86regPointerToX86regDisp8(TempReg1, TempReg2,MipsRegLo(Opcode.rt),4);


}

void Compile_R4300i_SD (BLOCK_SECTION * Section){
	uint32_t TempReg1, TempReg2;



	if (IsConst(Opcode.base)) {
		uint32_t Address = MipsRegLo(Opcode.base) + (short)Opcode.offset;

		if (IsConst(Opcode.rt)) {
			if (Is64Bit(Opcode.rt)) {
				Compile_SW_Const(MipsRegHi(Opcode.rt), Address);
			} else {
				Compile_SW_Const((MipsRegLo_S(Opcode.rt) >> 31), Address);
			}
			Compile_SW_Const(MipsRegLo(Opcode.rt), Address + 4);
		} else if (IsMapped(Opcode.rt)) {
			if (Is64Bit(Opcode.rt)) {
				Compile_SW_Register(MipsRegHi(Opcode.rt), Address);
			} else {
				Compile_SW_Register(Map_TempReg(Section,x86_Any,Opcode.rt,1), Address);
			}
			Compile_SW_Register(MipsRegLo(Opcode.rt), Address + 4);
		} else {
			Compile_SW_Register(TempReg1 = Map_TempReg(Section,x86_Any,Opcode.rt,1), Address);
			Compile_SW_Register(Map_TempReg(Section,TempReg1,Opcode.rt,0), Address + 4);
		}
		return;
	}
	if (IsMapped(Opcode.rt)) { ProtectGPR(Section,Opcode.rt); }
	if (IsMapped(Opcode.base)) {
		ProtectGPR(Section,Opcode.base);
		if (Opcode.offset != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,0);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.base),(short)Opcode.offset);
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		}
	} else {
		TempReg1 = Map_TempReg(Section,x86_Any,Opcode.base,0);
		if (Opcode.immediate == 0) {
		} else if (Opcode.immediate == 1) {
			IncX86reg(TempReg1);
		} else if (Opcode.immediate == 0xFFFF) {
			DecX86reg(TempReg1);
		} else {
			AddConstToX86Reg(TempReg1,(short)Opcode.immediate);
		}
	}

	TempReg2 = Map_TempReg(Section,x64_Any,-1,0);
	MoveX86RegToX86Reg(TempReg1, TempReg2);
	ShiftRightUnsignImmed(TempReg2,12);
	//MoveVariableDispToX86Reg(TLB_Map,TempReg2,TempReg2,8);
	LOAD_FROM_TLB(TempReg2, TempReg2);
	//For tlb miss
	//0041C522 85 C0                test        eax,eax
	//0041C524 75 01                jne         0041C527

	if (IsConst(Opcode.rt)) {
		if (Is64Bit(Opcode.rt)) {
			MoveConstToX86regPointer(MipsRegHi(Opcode.rt),TempReg1, TempReg2);
		} else {
			MoveConstToX86regPointer((MipsRegLo_S(Opcode.rt) >> 31),TempReg1, TempReg2);
		}
		AddConstToX86Reg(TempReg1,4);
		MoveConstToX86regPointer(MipsRegLo(Opcode.rt),TempReg1, TempReg2);
	} else if (IsMapped(Opcode.rt)) {
		if (Is64Bit(Opcode.rt)) {
			MoveX86regToX86regPointer(MipsRegHi(Opcode.rt),TempReg1, TempReg2);
		} else {
			MoveX86regToX86regPointer(Map_TempReg(Section,x86_Any,Opcode.rt,1),TempReg1, TempReg2);
		}
		AddConstToX86Reg(TempReg1,4);
		MoveX86regToX86regPointer(MipsRegLo(Opcode.rt),TempReg1, TempReg2);
	} else {
		int32_t X86Reg = Map_TempReg(Section,x86_Any,Opcode.rt,1);
		MoveX86regToX86regPointer(X86Reg,TempReg1, TempReg2);
		AddConstToX86Reg(TempReg1,4);
		MoveX86regToX86regPointer(Map_TempReg(Section,X86Reg,Opcode.rt,0),TempReg1, TempReg2);
	}

}

/********************** R4300i OpCodes: Special **********************/
void Compile_R4300i_SPECIAL_SLL (BLOCK_SECTION * Section) {


	if (Opcode.rd == 0) { return; }
	if (IsConst(Opcode.rt)) {
		if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }
		MipsRegLo(Opcode.rd) = MipsRegLo(Opcode.rt) << Opcode.sa;
		MipsRegState(Opcode.rd) = STATE_CONST_32;
		return;
	}
	if (Opcode.rd != Opcode.rt && IsMapped(Opcode.rt)) {
		switch (Opcode.sa) {
		case 0:
			Map_GPR_32bit(Section,Opcode.rd,1,Opcode.rt);
			break;
		case 1:
			ProtectGPR(Section,Opcode.rt);
			Map_GPR_32bit(Section,Opcode.rd,1,-1);
			LeaRegReg(MipsRegLo(Opcode.rd),MipsRegLo(Opcode.rt), 2);
			break;
		case 2:
			ProtectGPR(Section,Opcode.rt);
			Map_GPR_32bit(Section,Opcode.rd,1,-1);
			LeaRegReg(MipsRegLo(Opcode.rd),MipsRegLo(Opcode.rt), 4);
			break;
		case 3:
			ProtectGPR(Section,Opcode.rt);
			Map_GPR_32bit(Section,Opcode.rd,1,-1);
			LeaRegReg(MipsRegLo(Opcode.rd),MipsRegLo(Opcode.rt), 8);
			break;
		default:
			Map_GPR_32bit(Section,Opcode.rd,1,Opcode.rt);
			ShiftLeftSignImmed(MipsRegLo(Opcode.rd),(uint8_t)Opcode.sa);
		}
	} else {
		Map_GPR_32bit(Section,Opcode.rd,1,Opcode.rt);
		ShiftLeftSignImmed(MipsRegLo(Opcode.rd),(uint8_t)Opcode.sa);
	}
}

void Compile_R4300i_SPECIAL_SRL (BLOCK_SECTION * Section) {

	if (Opcode.rd == 0) { return; }

	if (IsConst(Opcode.rt)) {
		if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }
		MipsRegLo(Opcode.rd) = MipsRegLo(Opcode.rt) >> Opcode.sa;
		MipsRegState(Opcode.rd) = STATE_CONST_32;
		return;
	}
	Map_GPR_32bit(Section,Opcode.rd,1,Opcode.rt);
	ShiftRightUnsignImmed(MipsRegLo(Opcode.rd),(uint8_t)Opcode.sa);
}

void Compile_R4300i_SPECIAL_SRA (BLOCK_SECTION * Section) {

	if (Opcode.rd == 0) { return; }

	if (IsConst(Opcode.rt)) {
		if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }
		MipsRegLo(Opcode.rd) = MipsRegLo_S(Opcode.rt) >> Opcode.sa;
		MipsRegState(Opcode.rd) = STATE_CONST_32;
		return;
	}
	Map_GPR_32bit(Section,Opcode.rd,1,Opcode.rt);
	ShiftRightSignImmed(MipsRegLo(Opcode.rd),(uint8_t)Opcode.sa);
}

void Compile_R4300i_SPECIAL_SLLV (BLOCK_SECTION * Section) {

	if (Opcode.rd == 0) { return; }

	if (IsConst(Opcode.rs)) {
		uint32_t Shift = (MipsRegLo(Opcode.rs) & 0x1F);
		if (IsConst(Opcode.rt)) {
			if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }
			MipsRegLo(Opcode.rd) = MipsRegLo(Opcode.rt) << Shift;
			MipsRegState(Opcode.rd) = STATE_CONST_32;
		} else {
			Map_GPR_32bit(Section,Opcode.rd,1,Opcode.rt);
			ShiftLeftSignImmed(MipsRegLo(Opcode.rd),(uint8_t)Shift);
		}
		return;
	}
	Map_TempReg(Section,x86_ECX,Opcode.rs,0);
	AndConstToX86Reg(x86_ECX,0x1F);
	Map_GPR_32bit(Section,Opcode.rd,1,Opcode.rt);
	ShiftLeftSign(MipsRegLo(Opcode.rd));
}

void Compile_R4300i_SPECIAL_SRLV (BLOCK_SECTION * Section) {

	if (Opcode.rd == 0) { return; }

	if (IsKnown(Opcode.rs) && IsConst(Opcode.rs)) {
		uint32_t Shift = (MipsRegLo(Opcode.rs) & 0x1F);
		if (IsConst(Opcode.rt)) {
			if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }
			MipsRegLo(Opcode.rd) = MipsRegLo(Opcode.rt) >> Shift;
			MipsRegState(Opcode.rd) = STATE_CONST_32;
			return;
		}
		Map_GPR_32bit(Section,Opcode.rd,1,Opcode.rt);
		ShiftRightUnsignImmed(MipsRegLo(Opcode.rd),(uint8_t)Shift);
		return;
	}
	Map_TempReg(Section,x86_ECX,Opcode.rs,0);
	AndConstToX86Reg(x86_ECX,0x1F);
	Map_GPR_32bit(Section,Opcode.rd,1,Opcode.rt);
	ShiftRightUnsign(MipsRegLo(Opcode.rd));
}

void Compile_R4300i_SPECIAL_SRAV (BLOCK_SECTION * Section) {

	if (Opcode.rd == 0) { return; }

	if (IsKnown(Opcode.rs) && IsConst(Opcode.rs)) {
		uint32_t Shift = (MipsRegLo(Opcode.rs) & 0x1F);
		if (IsConst(Opcode.rt)) {
			if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }
			MipsRegLo(Opcode.rd) = MipsRegLo_S(Opcode.rt) >> Shift;
			MipsRegState(Opcode.rd) = STATE_CONST_32;
			return;
		}
		Map_GPR_32bit(Section,Opcode.rd,1,Opcode.rt);
		ShiftRightSignImmed(MipsRegLo(Opcode.rd),(uint8_t)Shift);
		return;
	}
	Map_TempReg(Section,x86_ECX,Opcode.rs,0);
	AndConstToX86Reg(x86_ECX,0x1F);
	Map_GPR_32bit(Section,Opcode.rd,1,Opcode.rt);
	ShiftRightSign(MipsRegLo(Opcode.rd));
}

void Compile_R4300i_SPECIAL_JR (BLOCK_SECTION * Section) {

	if ( NextInstruction == NORMAL ) {

		if (IsConst(Opcode.rs)) {
			Section->Jump.TargetPC      = MipsRegLo(Opcode.rs);
			Section->Jump.FallThrough   = 1;
			Section->Jump.LinkLocation  = NULL;
			Section->Jump.LinkLocation2 = NULL;
			Section->Cont.FallThrough   = 0;
			Section->Cont.LinkLocation  = NULL;
			Section->Cont.LinkLocation2 = NULL;
			if ((Section->CompilePC & 0xFFC) == 0xFFC) {
				GenerateSectionLinkage(Section);
				NextInstruction = END_BLOCK;
				return;
			}
		}
		if ((Section->CompilePC & 0xFFC) == 0xFFC) {
			if (IsMapped(Opcode.rs)) {
				MoveX86regToVariable(MipsRegLo(Opcode.rs),&JumpToLocation);
			} else {
				MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.rs,0),&JumpToLocation);
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
			NextInstruction = END_BLOCK;
			return;
		}
		if (DelaySlotEffectsCompare(Section->CompilePC,Opcode.rs,0)) {
			if (IsConst(Opcode.rs)) {
				MoveConstToVariable(MipsRegLo(Opcode.rs),&PROGRAM_COUNTER);
			} else 	if (IsMapped(Opcode.rs)) {
				MoveX86regToVariable(MipsRegLo(Opcode.rs),&PROGRAM_COUNTER);
			} else {
				MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.rs,0),&PROGRAM_COUNTER);
			}
		}
		NextInstruction = DO_DELAY_SLOT;
	} else if (NextInstruction == DELAY_SLOT_DONE ) {
		if (DelaySlotEffectsCompare(Section->CompilePC,Opcode.rs,0)) {

			CompileExit((uint32_t)-1,&Section->RegWorking,Normal,1,NULL);
		} else {
			if (IsConst(Opcode.rs)) {
				memcpy(&Section->Jump.RegSet,&Section->RegWorking,sizeof(REG_INFO));
				GenerateSectionLinkage(Section);
			} else {
				if (IsMapped(Opcode.rs)) {

					MoveX86regToVariable(MipsRegLo(Opcode.rs),&PROGRAM_COUNTER);
				} else {
					MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.rs,0),&PROGRAM_COUNTER);
				}
				CompileExit((uint32_t)-1,&Section->RegWorking,Normal,1,NULL);
			}
		}
		NextInstruction = END_BLOCK;
	} else {
#ifndef EXTERNAL_RELEASE
		DisplayError("WTF\n\nBranch\nNextInstruction = %X", NextInstruction);
#endif
	}
}

void Compile_R4300i_SPECIAL_JALR (BLOCK_SECTION * Section) {
	if ( NextInstruction == NORMAL ) {

		if (DelaySlotEffectsCompare(Section->CompilePC,Opcode.rs,0)) {
			Compile_R4300i_UnknownOpcode(Section);
		}
		UnMap_GPR(Section, Opcode.rd, 0);
		MipsRegLo(Opcode.rd) = Section->CompilePC + 8;
		MipsRegState(Opcode.rd) = STATE_CONST_32;
		if ((Section->CompilePC & 0xFFC) == 0xFFC) {
			if (IsMapped(Opcode.rs)) {
				MoveX86regToVariable(MipsRegLo(Opcode.rs),&JumpToLocation);
			} else {
				MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.rs,0),&JumpToLocation);
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
			NextInstruction = END_BLOCK;
			return;
		}
		NextInstruction = DO_DELAY_SLOT;
	} else if (NextInstruction == DELAY_SLOT_DONE ) {
		if (IsConst(Opcode.rs)) {
			memcpy(&Section->Jump.RegSet,&Section->RegWorking,sizeof(REG_INFO));
			Section->Jump.TargetPC      = MipsRegLo(Opcode.rs);
			Section->Jump.FallThrough   = 1;
			Section->Jump.LinkLocation  = NULL;
			Section->Jump.LinkLocation2 = NULL;
			Section->Cont.FallThrough   = 0;
			Section->Cont.LinkLocation  = NULL;
			Section->Cont.LinkLocation2 = NULL;

			GenerateSectionLinkage(Section);
		} else {
			if (IsMapped(Opcode.rs)) {
				MoveX86regToVariable(MipsRegLo(Opcode.rs),&PROGRAM_COUNTER);
			} else {
				MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.rs,0),&PROGRAM_COUNTER);
			}
			CompileExit((uint32_t)-1,&Section->RegWorking,Normal,1,NULL);
		}
		NextInstruction = END_BLOCK;
	} else {
	}
}

void Compile_R4300i_SPECIAL_SYSCALL (BLOCK_SECTION * Section) {
	CompileExit(Section->CompilePC,&Section->RegWorking,DoSysCall,1,NULL);
}

void Compile_R4300i_SPECIAL_BREAK (BLOCK_SECTION * Section) {
	//CompileExit(Section->CompilePC,Section->RegWorking,DoBreak,1,NULL);
	MoveConstToVariable(1,WaitMode);
}


void Compile_R4300i_SPECIAL_MFLO (BLOCK_SECTION * Section) {

	if (Opcode.rd == 0) { return; }

	Map_GPR_64bit(Section,Opcode.rd,-1);
	MoveVariableToX86reg(&LO.UW[0],MipsRegLo(Opcode.rd));
	MoveVariableToX86reg(&LO.UW[1],MipsRegHi(Opcode.rd));
}

void Compile_R4300i_SPECIAL_MTLO (BLOCK_SECTION * Section) {


	if (IsKnown(Opcode.rs) && IsConst(Opcode.rs)) {
		if (Is64Bit(Opcode.rs)) {
			MoveConstToVariable(MipsRegHi(Opcode.rs),&LO.UW[1]);
		} else if (IsSigned(Opcode.rs) && ((MipsRegLo(Opcode.rs) & 0x80000000) != 0)) {
			MoveConstToVariable(0xFFFFFFFF,&LO.UW[1]);
		} else {
			MoveConstToVariable(0,&LO.UW[1]);
		}
		MoveConstToVariable(MipsRegLo(Opcode.rs), &LO.UW[0]);
	} else if (IsKnown(Opcode.rs) && IsMapped(Opcode.rs)) {
		if (Is64Bit(Opcode.rs)) {
			MoveX86regToVariable(MipsRegHi(Opcode.rs),&LO.UW[1]);
		} else if (IsSigned(Opcode.rs)) {
			MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.rs,1),&LO.UW[1]);
		} else {
			MoveConstToVariable(0,&LO.UW[1]);
		}
		MoveX86regToVariable(MipsRegLo(Opcode.rs), &LO.UW[0]);
	} else {
		int32_t x86reg = Map_TempReg(Section,x86_Any,Opcode.rs,1);
		MoveX86regToVariable(x86reg,&LO.UW[1]);
		MoveX86regToVariable(Map_TempReg(Section,x86reg,Opcode.rs,0), &LO.UW[0]);
	}
}

void Compile_R4300i_SPECIAL_MFHI (BLOCK_SECTION * Section) {

	if (Opcode.rd == 0) { return; }

	Map_GPR_64bit(Section,Opcode.rd,-1);
	MoveVariableToX86reg(&HI.UW[0],MipsRegLo(Opcode.rd));
	MoveVariableToX86reg(&HI.UW[1],MipsRegHi(Opcode.rd));
}

void Compile_R4300i_SPECIAL_MTHI (BLOCK_SECTION * Section) {

	if (IsKnown(Opcode.rs) && IsConst(Opcode.rs)) {
		if (Is64Bit(Opcode.rs)) {
			MoveConstToVariable(MipsRegHi(Opcode.rs),&HI.UW[1]);
		} else if (IsSigned(Opcode.rs) && ((MipsRegLo(Opcode.rs) & 0x80000000) != 0)) {
			MoveConstToVariable(0xFFFFFFFF,&HI.UW[1]);
		} else {
			MoveConstToVariable(0,&HI.UW[1]);
		}
		MoveConstToVariable(MipsRegLo(Opcode.rs), &HI.UW[0]);
	} else if (IsKnown(Opcode.rs) && IsMapped(Opcode.rs)) {
		if (Is64Bit(Opcode.rs)) {
			MoveX86regToVariable(MipsRegHi(Opcode.rs),&HI.UW[1]);
		} else if (IsSigned(Opcode.rs)) {
			MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.rs,1),&HI.UW[1]);
		} else {
			MoveConstToVariable(0,&HI.UW[1]);
		}
		MoveX86regToVariable(MipsRegLo(Opcode.rs), &HI.UW[0]);
	} else {
		int32_t x86reg = Map_TempReg(Section,x86_Any,Opcode.rs,1);
		MoveX86regToVariable(x86reg,&HI.UW[1]);
		MoveX86regToVariable(Map_TempReg(Section,x86reg,Opcode.rs,0), &HI.UW[0]);
	}
}

void Compile_R4300i_SPECIAL_DSLLV (BLOCK_SECTION * Section) {
	uint8_t * Jump[2];


	if (Opcode.rd == 0) { return; }

	if (IsConst(Opcode.rs)) {
		uint32_t Shift = (MipsRegLo(Opcode.rs) & 0x3F);
		Compile_R4300i_UnknownOpcode(Section);
		return;
	}
	Map_TempReg(Section,x86_ECX,Opcode.rs,0);
	AndConstToX86Reg(x86_ECX,0x3F);
	Map_GPR_64bit(Section,Opcode.rd,Opcode.rt);
	CompConstToX86reg(x86_ECX,0x20);
	JaeLabel8( 0);
	Jump[0] = RecompPos - 1;
	ShiftLeftDouble(MipsRegHi(Opcode.rd),MipsRegLo(Opcode.rd));
	ShiftLeftSign(MipsRegLo(Opcode.rd));
	JmpLabel8( 0);
	Jump[1] = RecompPos - 1;

	//MORE32:


	*((uint8_t *)(Jump[0]))=(uint8_t)(RecompPos - Jump[0] - 1);
	MoveX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegHi(Opcode.rd));
	XorX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(Opcode.rd));
	AndConstToX86Reg(x86_ECX,0x1F);
	ShiftLeftSign(MipsRegHi(Opcode.rd));

	//continue:


	*((uint8_t *)(Jump[1]))=(uint8_t)(RecompPos - Jump[1] - 1);
}

void Compile_R4300i_SPECIAL_DSRLV (BLOCK_SECTION * Section) {
	uint8_t * Jump[2];


	if (Opcode.rd == 0) { return; }

	if (IsConst(Opcode.rs)) {
		uint32_t Shift = (MipsRegLo(Opcode.rs) & 0x3F);
		if (IsConst(Opcode.rt)) {
			if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }
			MipsReg(Opcode.rd) = Is64Bit(Opcode.rt)?MipsReg(Opcode.rt):(int64_t)MipsRegLo_S(Opcode.rt);
			MipsReg(Opcode.rd) = MipsReg(Opcode.rd) >> Shift;
			if ((MipsRegHi(Opcode.rd) == 0) && (MipsRegLo(Opcode.rd) & 0x80000000) == 0) {
				MipsRegState(Opcode.rd) = STATE_CONST_32;
			} else if ((MipsRegHi(Opcode.rd) == 0xFFFFFFFF) && (MipsRegLo(Opcode.rd) & 0x80000000) != 0) {
				MipsRegState(Opcode.rd) = STATE_CONST_32;
			} else {
				MipsRegState(Opcode.rd) = STATE_CONST_64;
			}
			return;
		}
		//if (Shift < 0x20) {
		//} else {
		//}
		//Compile_R4300i_UnknownOpcode(Section);
		//return;
	}
	Map_TempReg(Section,x86_ECX,Opcode.rs,0);
	AndConstToX86Reg(x86_ECX,0x3F);
	Map_GPR_64bit(Section,Opcode.rd,Opcode.rt);
	CompConstToX86reg(x86_ECX,0x20);
	JaeLabel8( 0);
	Jump[0] = RecompPos - 1;
	ShiftRightDouble(MipsRegLo(Opcode.rd),MipsRegHi(Opcode.rd));
	ShiftRightUnsign(MipsRegHi(Opcode.rd));
	JmpLabel8( 0);
	Jump[1] = RecompPos - 1;

	//MORE32:


	*((uint8_t *)(Jump[0]))=(uint8_t)(RecompPos - Jump[0] - 1);
	MoveX86RegToX86Reg(MipsRegHi(Opcode.rd),MipsRegLo(Opcode.rd));
	XorX86RegToX86Reg(MipsRegHi(Opcode.rd),MipsRegHi(Opcode.rd));
	AndConstToX86Reg(x86_ECX,0x1F);
	ShiftRightUnsign(MipsRegLo(Opcode.rd));

	//continue:


	*((uint8_t *)(Jump[1]))=(uint8_t)(RecompPos - Jump[1] - 1);
}

void Compile_R4300i_SPECIAL_DSRAV (BLOCK_SECTION * Section) {
	uint8_t * Jump[2];


	if (Opcode.rd == 0) { return; }

	if (IsConst(Opcode.rs)) {
		uint32_t Shift = (MipsRegLo(Opcode.rs) & 0x3F);
		Compile_R4300i_UnknownOpcode(Section);
		return;
	}
	Map_TempReg(Section,x86_ECX,Opcode.rs,0);
	AndConstToX86Reg(x86_ECX,0x3F);
	Map_GPR_64bit(Section,Opcode.rd,Opcode.rt);
	CompConstToX86reg(x86_ECX,0x20);
	JaeLabel8( 0);
	Jump[0] = RecompPos - 1;
	ShiftRightDouble(MipsRegLo(Opcode.rd),MipsRegHi(Opcode.rd));
	ShiftRightSign(MipsRegHi(Opcode.rd));
	JmpLabel8( 0);
	Jump[1] = RecompPos - 1;

	//MORE32:


	*((uint8_t *)(Jump[0]))=(uint8_t)(RecompPos - Jump[0] - 1);
	MoveX86RegToX86Reg(MipsRegHi(Opcode.rd),MipsRegLo(Opcode.rd));
	ShiftRightSignImmed(MipsRegHi(Opcode.rd),0x1F);
	AndConstToX86Reg(x86_ECX,0x1F);
	ShiftRightSign(MipsRegLo(Opcode.rd));

	//continue:


	*((uint8_t *)(Jump[1]))=(uint8_t)(RecompPos - Jump[1] - 1);
}

void Compile_R4300i_SPECIAL_MULT ( BLOCK_SECTION * Section) {


	x86Protected(x86_EDX) = 1;
	Map_TempReg(Section,x86_EAX,Opcode.rs,0);
	x86Protected(x86_EDX) = 0;
	Map_TempReg(Section,x86_EDX,Opcode.rt,0);

	imulX86reg(x86_EDX);

	MoveX86regToVariable(x86_EAX,&LO.UW[0]);
	MoveX86regToVariable(x86_EDX,&HI.UW[0]);
	ShiftRightSignImmed(x86_EAX,31);	/* paired */
	ShiftRightSignImmed(x86_EDX,31);
	MoveX86regToVariable(x86_EAX,&LO.UW[1]);
	MoveX86regToVariable(x86_EDX,&HI.UW[1]);
}

void Compile_R4300i_SPECIAL_MULTU (BLOCK_SECTION * Section) {


	x86Protected(x86_EDX) = 1;
	Map_TempReg(Section,x86_EAX,Opcode.rs,0);
	x86Protected(x86_EDX) = 0;
	Map_TempReg(Section,x86_EDX,Opcode.rt,0);

	MulX86reg(x86_EDX);

	MoveX86regToVariable(x86_EAX,&LO.UW[0]);
	MoveX86regToVariable(x86_EDX,&HI.UW[0]);
	ShiftRightSignImmed(x86_EAX,31);	/* paired */
	ShiftRightSignImmed(x86_EDX,31);
	MoveX86regToVariable(x86_EAX,&LO.UW[1]);
	MoveX86regToVariable(x86_EDX,&HI.UW[1]);
}

void Compile_R4300i_SPECIAL_DIV (BLOCK_SECTION * Section) {
	uint8_t *Jump[2];



	if (IsConst(Opcode.rt)) {
		if (MipsRegLo(Opcode.rt) == 0) {
			MoveConstToVariable(0, &LO.UW[0]);
			MoveConstToVariable(0, &LO.UW[1]);
			MoveConstToVariable(0, &HI.UW[0]);
			MoveConstToVariable(0, &HI.UW[1]);
			return;
		}
		Jump[1] = NULL;
	} else {
		if (IsMapped(Opcode.rt)) {
			CompConstToX86reg(MipsRegLo(Opcode.rt),0);
		} else {
			CompConstToVariable(0, &GPR[Opcode.rt].W[0]);
		}
		JneLabel8( 0);
		Jump[0] = RecompPos - 1;

		MoveConstToVariable(0, &LO.UW[0]);
		MoveConstToVariable(0, &LO.UW[1]);
		MoveConstToVariable(0, &HI.UW[0]);
		MoveConstToVariable(0, &HI.UW[1]);

		JmpLabel8( 0);
		Jump[1] = RecompPos - 1;



		*((uint8_t *)(Jump[0]))=(uint8_t)(RecompPos - Jump[0] - 1);
	}
	/*	lo = (SD)rs / (SD)rt;
		hi = (SD)rs % (SD)rt; */

	x86Protected(x86_EDX) = 1;
	Map_TempReg(Section,x86_EAX,Opcode.rs,0);

	/* edx is the signed portion to eax */
	x86Protected(x86_EDX) = 0;
	Map_TempReg(Section,x86_EDX, -1, 0);

	MoveX86RegToX86Reg(x86_EAX, x86_EDX);
	ShiftRightSignImmed(x86_EDX,31);

	if (IsMapped(Opcode.rt)) {
		idivX86reg(MipsRegLo(Opcode.rt));
	} else {
		idivX86reg(Map_TempReg(Section,x86_Any,Opcode.rt,0));
	}


	MoveX86regToVariable(x86_EAX,&LO.UW[0]);
	MoveX86regToVariable(x86_EDX,&HI.UW[0]);
	ShiftRightSignImmed(x86_EAX,31);	/* paired */
	ShiftRightSignImmed(x86_EDX,31);
	MoveX86regToVariable(x86_EAX,&LO.UW[1]);
	MoveX86regToVariable(x86_EDX,&HI.UW[1]);

	if( Jump[1] != NULL ) {


		*((uint8_t *)(Jump[1]))=(uint8_t)(RecompPos - Jump[1] - 1);
	}
}

void Compile_R4300i_SPECIAL_DIVU ( BLOCK_SECTION * Section) {
	uint8_t *Jump[2];
	int32_t x86reg;



	if (IsConst(Opcode.rt)) {
		if (MipsRegLo(Opcode.rt) == 0) {
			MoveConstToVariable(0, &LO.UW[0]);
			MoveConstToVariable(0, &LO.UW[1]);
			MoveConstToVariable(0, &HI.UW[0]);
			MoveConstToVariable(0, &HI.UW[1]);
			return;
		}
		Jump[1] = NULL;
	} else {
		if (IsMapped(Opcode.rt)) {
			CompConstToX86reg(MipsRegLo(Opcode.rt),0);
		} else {
			CompConstToVariable(0, &GPR[Opcode.rt].W[0]);
		}
		JneLabel8( 0);
		Jump[0] = RecompPos - 1;

		MoveConstToVariable(0, &LO.UW[0]);
		MoveConstToVariable(0, &LO.UW[1]);
		MoveConstToVariable(0, &HI.UW[0]);
		MoveConstToVariable(0, &HI.UW[1]);

		JmpLabel8( 0);
		Jump[1] = RecompPos - 1;



		*((uint8_t *)(Jump[0]))=(uint8_t)(RecompPos - Jump[0] - 1);
	}


	/*	lo = (UD)rs / (UD)rt;
		hi = (UD)rs % (UD)rt; */

	x86Protected(x86_EAX) = 1;
	Map_TempReg(Section,x86_EDX, 0, 0);
	x86Protected(x86_EAX) = 0;

	Map_TempReg(Section,x86_EAX,Opcode.rs,0);
	x86reg = Map_TempReg(Section,x86_Any,Opcode.rt,0);

    DivX86reg(x86reg);

	MoveX86regToVariable(x86_EAX,&LO.UW[0]);
	MoveX86regToVariable(x86_EDX,&HI.UW[0]);

	/* wouldnt these be zero (???) */

	ShiftRightSignImmed(x86_EAX,31);	/* paired */
	ShiftRightSignImmed(x86_EDX,31);
	MoveX86regToVariable(x86_EAX,&LO.UW[1]);
	MoveX86regToVariable(x86_EDX,&HI.UW[1]);

	if( Jump[1] != NULL ) {


		*((uint8_t *)(Jump[1]))=(uint8_t)(RecompPos - Jump[1] - 1);
	}
}

void Compile_R4300i_SPECIAL_DMULT (BLOCK_SECTION * Section) {


	if (Opcode.rs != 0) { UnMap_GPR(Section,Opcode.rs,1); }
	if (Opcode.rs != 0) { UnMap_GPR(Section,Opcode.rt,1); }
	Pushad();
	MoveConstToVariable(Opcode.Hex, &Opcode.Hex );
	Call_Direct(r4300i_SPECIAL_DMULT);
	Popad();
}

void Compile_R4300i_SPECIAL_DMULTU (BLOCK_SECTION * Section) {


	/* LO.UDW = (uint64)GPR[Opcode.rs].UW[0] * (uint64)GPR[Opcode.rt].UW[0]; */
	x86Protected(x86_EDX) = 1;
	Map_TempReg(Section,x86_EAX,Opcode.rs,0);
	x86Protected(x86_EDX) = 0;
	Map_TempReg(Section,x86_EDX,Opcode.rt,0);

	MulX86reg(x86_EDX);
	MoveX86regToVariable(x86_EAX, &LO.UW[0]);
	MoveX86regToVariable(x86_EDX, &LO.UW[1]);

	/* HI.UDW = (uint64)GPR[Opcode.rs].UW[1] * (uint64)GPR[Opcode.rt].UW[1]; */
	Map_TempReg(Section,x86_EAX,Opcode.rs,1);
	Map_TempReg(Section,x86_EDX,Opcode.rt,1);

	MulX86reg(x86_EDX);
	MoveX86regToVariable(x86_EAX, &HI.UW[0]);
	MoveX86regToVariable(x86_EDX, &HI.UW[1]);

	/* Tmp[0].UDW = (uint64)GPR[Opcode.rs].UW[1] * (uint64)GPR[Opcode.rt].UW[0]; */
	Map_TempReg(Section,x86_EAX,Opcode.rs,1);
	Map_TempReg(Section,x86_EDX,Opcode.rt,0);

	Map_TempReg(Section,x86_EBX,-1,0);
	Map_TempReg(Section,x86_ECX,-1,0);

	MulX86reg(x86_EDX);
	MoveX86RegToX86Reg(x86_EAX, x86_EBX); /* EDX:EAX -> ECX:EBX */
	MoveX86RegToX86Reg(x86_EDX, x86_ECX);

	/* Tmp[1].UDW = (uint64)GPR[Opcode.rs].UW[0] * (uint64)GPR[Opcode.rt].UW[1]; */
	Map_TempReg(Section,x86_EAX,Opcode.rs,0);
	Map_TempReg(Section,x86_EDX,Opcode.rt,1);

	MulX86reg(x86_EDX);
	Map_TempReg(Section,x86_ESI,-1,0);
	Map_TempReg(Section,x86_EDI,-1,0);
	MoveX86RegToX86Reg(x86_EAX, x86_ESI); /* EDX:EAX -> EDI:ESI */
	MoveX86RegToX86Reg(x86_EDX, x86_EDI);

	/* Tmp[2].UDW = (uint64)LO.UW[1] + (uint64)Tmp[0].UW[0] + (uint64)Tmp[1].UW[0]; */
	XorX86RegToX86Reg(x86_EDX, x86_EDX);
	MoveVariableToX86reg(&LO.UW[1], x86_EAX);
	AddX86RegToX86Reg(x86_EAX, x86_EBX);
	AddConstToX86Reg(x86_EDX, 0);
	AddX86RegToX86Reg(x86_EAX, x86_ESI);
	AddConstToX86Reg(x86_EDX, 0);			/* EDX:EAX */

	/* LO.UDW += ((uint64)Tmp[0].UW[0] + (uint64)Tmp[1].UW[0]) << 32; */
	/* [low+4] += ebx + esi */

	AddX86regToVariable(x86_EBX, &LO.UW[1]);
	AddX86regToVariable(x86_ESI, &LO.UW[1]);

	/* HI.UDW += (uint64)Tmp[0].UW[1] + (uint64)Tmp[1].UW[1] + Tmp[2].UW[1]; */
	/* [hi] += ecx + edi + edx */

	/*AddX86regToVariable(x86_ECX, &HI.UW[0]);
	AdcConstToVariable(&HI.UW[1], 0);

	AddX86regToVariable(x86_EDI, &HI.UW[0]);
	AdcConstToVariable(&HI.UW[1], 0);

	AddX86regToVariable(x86_EDX, &HI.UW[0]);
	AdcConstToVariable(&HI.UW[1], 0);*/
}

void Compile_R4300i_SPECIAL_DDIV (BLOCK_SECTION * Section) {


	UnMap_GPR(Section,Opcode.rs,1);
	UnMap_GPR(Section,Opcode.rt,1);
	Pushad();
	MoveConstToVariable(Opcode.Hex, &Opcode.Hex );
	Call_Direct(r4300i_SPECIAL_DDIV);
	Popad();
}

void Compile_R4300i_SPECIAL_DDIVU (BLOCK_SECTION * Section) {


	UnMap_GPR(Section,Opcode.rs,1);
	UnMap_GPR(Section,Opcode.rt,1);
	Pushad();
	MoveConstToVariable(Opcode.Hex, &Opcode.Hex );
	Call_Direct(r4300i_SPECIAL_DDIVU);
	Popad();
}

void Compile_R4300i_SPECIAL_ADD (BLOCK_SECTION * Section) {
	int32_t source1 = Opcode.rd == Opcode.rt?Opcode.rt:Opcode.rs;
	int32_t source2 = Opcode.rd == Opcode.rt?Opcode.rs:Opcode.rt;


	if (Opcode.rd == 0) { return; }

	if (IsConst(source1) && IsConst(source2)) {
		uint32_t temp = MipsRegLo(source1) + MipsRegLo(source2);
		if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }
		MipsRegLo(Opcode.rd) = temp;
		MipsRegState(Opcode.rd) = STATE_CONST_32;
		return;
	}

	Map_GPR_32bit(Section,Opcode.rd,1, source1);
	if (IsConst(source2)) {
		if (MipsRegLo(source2) != 0) {
			AddConstToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(source2));
		}
	} else if (IsKnown(source2) && IsMapped(source2)) {
		AddX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(source2));
	} else {
		AddVariableToX86reg(MipsRegLo(Opcode.rd),&GPR[source2].W[0]);
	}
}

void Compile_R4300i_SPECIAL_ADDU (BLOCK_SECTION * Section) {
	int32_t source1 = Opcode.rd == Opcode.rt?Opcode.rt:Opcode.rs;
	int32_t source2 = Opcode.rd == Opcode.rt?Opcode.rs:Opcode.rt;


	if (Opcode.rd == 0) { return; }

	if (IsConst(source1) && IsConst(source2)) {
		uint32_t temp = MipsRegLo(source1) + MipsRegLo(source2);
		if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }
		MipsRegLo(Opcode.rd) = temp;
		MipsRegState(Opcode.rd) = STATE_CONST_32;
		return;
	}

	Map_GPR_32bit(Section,Opcode.rd,1, source1);
	if (IsConst(source2)) {
		if (MipsRegLo(source2) != 0) {
			AddConstToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(source2));
		}
	} else if (IsKnown(source2) && IsMapped(source2)) {
		AddX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(source2));
	} else {
		AddVariableToX86reg(MipsRegLo(Opcode.rd),&GPR[source2].W[0]);
	}
}

void Compile_R4300i_SPECIAL_SUB (BLOCK_SECTION * Section) {

	if (Opcode.rd == 0) { return; }

	if (IsConst(Opcode.rt)  && IsConst(Opcode.rs)) {
		uint32_t temp = MipsRegLo(Opcode.rs) - MipsRegLo(Opcode.rt);
		if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }
		MipsRegLo(Opcode.rd) = temp;
		MipsRegState(Opcode.rd) = STATE_CONST_32;
	} else {
		if (Opcode.rd == Opcode.rt) {
			int32_t x86Reg = Map_TempReg(Section,x86_Any,Opcode.rt,0);
			Map_GPR_32bit(Section,Opcode.rd,1, Opcode.rs);
			SubX86RegToX86Reg(MipsRegLo(Opcode.rd),x86Reg);
			return;
		}
		Map_GPR_32bit(Section,Opcode.rd,1, Opcode.rs);
		if (IsConst(Opcode.rt)) {
			SubConstFromX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(Opcode.rt));
		} else if (IsMapped(Opcode.rt)) {
			SubX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(Opcode.rt));
		} else {
			SubVariableFromX86reg(MipsRegLo(Opcode.rd),&GPR[Opcode.rt].W[0]);
		}
	}
}

void Compile_R4300i_SPECIAL_SUBU (BLOCK_SECTION * Section) {

	if (Opcode.rd == 0) { return; }

	if (IsConst(Opcode.rt)  && IsConst(Opcode.rs)) {
		uint32_t temp = MipsRegLo(Opcode.rs) - MipsRegLo(Opcode.rt);
		if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }
		MipsRegLo(Opcode.rd) = temp;
		MipsRegState(Opcode.rd) = STATE_CONST_32;
	} else {
		if (Opcode.rd == Opcode.rt) {
			int32_t x86Reg = Map_TempReg(Section,x86_Any,Opcode.rt,0);
			Map_GPR_32bit(Section,Opcode.rd,1, Opcode.rs);
			SubX86RegToX86Reg(MipsRegLo(Opcode.rd),x86Reg);
			return;
		}
		Map_GPR_32bit(Section,Opcode.rd,1, Opcode.rs);
		if (IsConst(Opcode.rt)) {
			SubConstFromX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(Opcode.rt));
		} else if (IsMapped(Opcode.rt)) {
			SubX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(Opcode.rt));
		} else {
			SubVariableFromX86reg(MipsRegLo(Opcode.rd),&GPR[Opcode.rt].W[0]);
		}
	}
}

void Compile_R4300i_SPECIAL_AND (BLOCK_SECTION * Section) {

	if (IsKnown(Opcode.rt) && IsKnown(Opcode.rs)) {
		if (IsConst(Opcode.rt) && IsConst(Opcode.rs)) {
			if (Is64Bit(Opcode.rt) || Is64Bit(Opcode.rs)) {
				MipsReg(Opcode.rd) =
					(Is64Bit(Opcode.rt)?MipsReg(Opcode.rt):(int64_t)MipsRegLo_S(Opcode.rt)) &
					(Is64Bit(Opcode.rs)?MipsReg(Opcode.rs):(int64_t)MipsRegLo_S(Opcode.rs));

				if (MipsRegLo_S(Opcode.rd) < 0 && MipsRegHi_S(Opcode.rd) == -1){
					MipsRegState(Opcode.rd) = STATE_CONST_32;
				} else if (MipsRegLo_S(Opcode.rd) >= 0 && MipsRegHi_S(Opcode.rd) == 0){
					MipsRegState(Opcode.rd) = STATE_CONST_32;
				} else {
					MipsRegState(Opcode.rd) = STATE_CONST_64;
				}
			} else {
				MipsReg(Opcode.rd) = MipsRegLo(Opcode.rt) & MipsReg(Opcode.rs);
				MipsRegState(Opcode.rd) = STATE_CONST_32;
			}
		} else if (IsMapped(Opcode.rt) && IsMapped(Opcode.rs)) {
			int32_t source1 = Opcode.rd == Opcode.rt?Opcode.rt:Opcode.rs;
			int32_t source2 = Opcode.rd == Opcode.rt?Opcode.rs:Opcode.rt;

			ProtectGPR(Section,source1);
			ProtectGPR(Section,source2);
			if (Is32Bit(source1) && Is32Bit(source2)) {
				int32_t Sign = (IsSigned(Opcode.rt) && IsSigned(Opcode.rs))?1:0;
				Map_GPR_32bit(Section,Opcode.rd,Sign,source1);
				AndX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(source2));
			} else if (Is32Bit(source1) || Is32Bit(source2)) {
				if (IsUnsigned(Is32Bit(source1)?source1:source2)) {
					Map_GPR_32bit(Section,Opcode.rd,0,source1);
					AndX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(source2));
				} else {
					Map_GPR_64bit(Section,Opcode.rd,source1);
					if (Is32Bit(source2)) {
						AndX86RegToX86Reg(MipsRegHi(Opcode.rd),Map_TempReg(Section,x86_Any,source2,1));
					} else {
						AndX86RegToX86Reg(MipsRegHi(Opcode.rd),MipsRegHi(source2));
					}
					AndX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(source2));
				}
			} else {
				Map_GPR_64bit(Section,Opcode.rd,source1);
				AndX86RegToX86Reg(MipsRegHi(Opcode.rd),MipsRegHi(source2));
				AndX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(source2));
			}
		} else {
			int32_t ConstReg = IsConst(Opcode.rt)?Opcode.rt:Opcode.rs;
			int32_t MappedReg = IsConst(Opcode.rt)?Opcode.rs:Opcode.rt;

			if (Is64Bit(ConstReg)) {
				if (Is32Bit(MappedReg) && IsUnsigned(MappedReg)) {
					if (MipsRegLo(ConstReg) == 0) {
						Map_GPR_32bit(Section,Opcode.rd,0, 0);
					} else {
						uint32_t Value = MipsRegLo(ConstReg);
						Map_GPR_32bit(Section,Opcode.rd,0, MappedReg);
						AndConstToX86Reg(MipsRegLo(Opcode.rd),Value);
					}
				} else {
					int64_t Value = MipsReg(ConstReg);
					Map_GPR_64bit(Section,Opcode.rd,MappedReg);
					AndConstToX86Reg(MipsRegHi(Opcode.rd),(uint32_t)(Value >> 32));
					AndConstToX86Reg(MipsRegLo(Opcode.rd),(uint32_t)Value);
				}
			} else if (Is64Bit(MappedReg)) {
				uint32_t Value = MipsRegLo(ConstReg);
				if (Value != 0) {
					Map_GPR_32bit(Section,Opcode.rd,IsSigned(ConstReg)?1:0,MappedReg);
					AndConstToX86Reg(MipsRegLo(Opcode.rd),(uint32_t)Value);
				} else {
					Map_GPR_32bit(Section,Opcode.rd,IsSigned(ConstReg)?1:0, 0);
				}
			} else {
				uint32_t Value = MipsRegLo(ConstReg);
				int32_t Sign = 0;
				if (IsSigned(ConstReg) && IsSigned(MappedReg)) { Sign = 1; }
				if (Value != 0) {
					Map_GPR_32bit(Section,Opcode.rd,Sign,MappedReg);
					AndConstToX86Reg(MipsRegLo(Opcode.rd),Value);
				} else {
					Map_GPR_32bit(Section,Opcode.rd,0, 0);
				}
			}
		}
	} else if (IsKnown(Opcode.rt) || IsKnown(Opcode.rs)) {
		uint32_t KnownReg = IsKnown(Opcode.rt)?Opcode.rt:Opcode.rs;
		uint32_t UnknownReg = IsKnown(Opcode.rt)?Opcode.rs:Opcode.rt;

		if (IsConst(KnownReg)) {
			if (Is64Bit(KnownReg)) {
				uint64_t Value = MipsReg(KnownReg);
				Map_GPR_64bit(Section,Opcode.rd,UnknownReg);
				AndConstToX86Reg(MipsRegHi(Opcode.rd),(uint32_t)(Value >> 32));
				AndConstToX86Reg(MipsRegLo(Opcode.rd),(uint32_t)Value);
			} else {
				uint32_t Value = MipsRegLo(KnownReg);
				Map_GPR_32bit(Section,Opcode.rd,IsSigned(KnownReg),UnknownReg);
				AndConstToX86Reg(MipsRegLo(Opcode.rd),(uint32_t)Value);
			}
		} else {
			ProtectGPR(Section,KnownReg);
			if (KnownReg == Opcode.rd) {
				if (Is64Bit(KnownReg)) {
					Map_GPR_64bit(Section,Opcode.rd,KnownReg);
					AndVariableToX86Reg(&GPR[UnknownReg].W[1],MipsRegHi(Opcode.rd));
					AndVariableToX86Reg(&GPR[UnknownReg].W[0],MipsRegLo(Opcode.rd));
				} else {
					Map_GPR_32bit(Section,Opcode.rd,IsSigned(KnownReg),KnownReg);
					AndVariableToX86Reg(&GPR[UnknownReg].W[0],MipsRegLo(Opcode.rd));
				}
			} else {
				if (Is64Bit(KnownReg)) {
					Map_GPR_64bit(Section,Opcode.rd,UnknownReg);
					AndX86RegToX86Reg(MipsRegHi(Opcode.rd),MipsRegHi(KnownReg));
					AndX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(KnownReg));
				} else {
					Map_GPR_32bit(Section,Opcode.rd,IsSigned(KnownReg),UnknownReg);
					AndX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(KnownReg));
				}
			}
		}
	} else {
		Map_GPR_64bit(Section,Opcode.rd,Opcode.rt);
		AndVariableToX86Reg(&GPR[Opcode.rs].W[1],MipsRegHi(Opcode.rd));
		AndVariableToX86Reg(&GPR[Opcode.rs].W[0],MipsRegLo(Opcode.rd));
	}
}

void Compile_R4300i_SPECIAL_OR (BLOCK_SECTION * Section) {


	if (IsKnown(Opcode.rt) && IsKnown(Opcode.rs)) {
		if (IsConst(Opcode.rt) && IsConst(Opcode.rs)) {
			if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }
			if (Is64Bit(Opcode.rt) || Is64Bit(Opcode.rs)) {
				MipsReg(Opcode.rd) =
					(Is64Bit(Opcode.rt)?MipsReg(Opcode.rt):(int64_t)MipsRegLo_S(Opcode.rt)) |
					(Is64Bit(Opcode.rs)?MipsReg(Opcode.rs):(int64_t)MipsRegLo_S(Opcode.rs));
				if (MipsRegLo_S(Opcode.rd) < 0 && MipsRegHi_S(Opcode.rd) == -1){
					MipsRegState(Opcode.rd) = STATE_CONST_32;
				} else if (MipsRegLo_S(Opcode.rd) >= 0 && MipsRegHi_S(Opcode.rd) == 0){
					MipsRegState(Opcode.rd) = STATE_CONST_32;
				} else {
					MipsRegState(Opcode.rd) = STATE_CONST_64;
				}
			} else {
				MipsRegLo(Opcode.rd) = MipsRegLo(Opcode.rt) | MipsRegLo(Opcode.rs);
				MipsRegState(Opcode.rd) = STATE_CONST_32;
			}
		} else if (IsMapped(Opcode.rt) && IsMapped(Opcode.rs)) {
			int32_t source1 = Opcode.rd == Opcode.rt?Opcode.rt:Opcode.rs;
			int32_t source2 = Opcode.rd == Opcode.rt?Opcode.rs:Opcode.rt;

			ProtectGPR(Section,Opcode.rt);
			ProtectGPR(Section,Opcode.rs);
			if (Is64Bit(Opcode.rt) || Is64Bit(Opcode.rs)) {
				Map_GPR_64bit(Section,Opcode.rd,source1);
				if (Is64Bit(source2)) {
					OrX86RegToX86Reg(MipsRegHi(Opcode.rd),MipsRegHi(source2));
				} else {
					OrX86RegToX86Reg(MipsRegHi(Opcode.rd),Map_TempReg(Section,x86_Any,source2,1));
				}
			} else {
				ProtectGPR(Section,source2);
				Map_GPR_32bit(Section,Opcode.rd,1,source1);
			}
			OrX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(source2));
		} else {
			uint32_t ConstReg = IsConst(Opcode.rt)?Opcode.rt:Opcode.rs;
			uint32_t MappedReg = IsConst(Opcode.rt)?Opcode.rs:Opcode.rt;

			if (Is64Bit(Opcode.rt) || Is64Bit(Opcode.rs)) {
				uint64_t Value;

				if (Is64Bit(ConstReg)) {
					Value = MipsReg(ConstReg);
				} else {
					Value = IsSigned(ConstReg)?MipsRegLo_S(ConstReg):MipsRegLo(ConstReg);
				}
				Map_GPR_64bit(Section,Opcode.rd,MappedReg);
				if ((Value >> 32) != 0) {
					OrConstToX86Reg((uint32_t)(Value >> 32),MipsRegHi(Opcode.rd));
				}
				if ((uint32_t)Value != 0) {
					OrConstToX86Reg((uint32_t)Value,MipsRegLo(Opcode.rd));
				}
			} else {
				int32_t Value = MipsRegLo(ConstReg);
				Map_GPR_32bit(Section,Opcode.rd,1, MappedReg);
				if (Value != 0) { OrConstToX86Reg(Value,MipsRegLo(Opcode.rd)); }
			}
		}
	} else if (IsKnown(Opcode.rt) || IsKnown(Opcode.rs)) {
		int32_t KnownReg = IsKnown(Opcode.rt)?Opcode.rt:Opcode.rs;
		int32_t UnknownReg = IsKnown(Opcode.rt)?Opcode.rs:Opcode.rt;

		if (IsConst(KnownReg)) {
			uint64_t Value;

			Value = Is64Bit(KnownReg)?MipsReg(KnownReg):MipsRegLo_S(KnownReg);
			Map_GPR_64bit(Section,Opcode.rd,UnknownReg);
			if ((Value >> 32) != 0) {
				OrConstToX86Reg((uint32_t)(Value >> 32),MipsRegHi(Opcode.rd));
			}
			if ((uint32_t)Value != 0) {
				OrConstToX86Reg((uint32_t)Value,MipsRegLo(Opcode.rd));
			}
		} else {
			Map_GPR_64bit(Section,Opcode.rd,KnownReg);
			OrVariableToX86Reg(&GPR[UnknownReg].W[1],MipsRegHi(Opcode.rd));
			OrVariableToX86Reg(&GPR[UnknownReg].W[0],MipsRegLo(Opcode.rd));
		}
	} else {
		Map_GPR_64bit(Section,Opcode.rd,Opcode.rt);
		OrVariableToX86Reg(&GPR[Opcode.rs].W[1],MipsRegHi(Opcode.rd));
		OrVariableToX86Reg(&GPR[Opcode.rs].W[0],MipsRegLo(Opcode.rd));
	}
}

void Compile_R4300i_SPECIAL_XOR (BLOCK_SECTION * Section) {

	if (Opcode.rd == 0) { return; }

	//BreakPoint();

	if (Opcode.rt == Opcode.rs) {
		UnMap_GPR(Section, Opcode.rd, 0);
		MipsRegState(Opcode.rd) = STATE_CONST_32;
		MipsRegLo(Opcode.rd) = 0;
		return;
	}
	if (IsKnown(Opcode.rt) && IsKnown(Opcode.rs)) {
		if (IsConst(Opcode.rt) && IsConst(Opcode.rs)) {
			if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }
			if (Is64Bit(Opcode.rt) || Is64Bit(Opcode.rs)) {
#ifndef EXTERNAL_RELEASE
				DisplayError("XOR 1");
#endif
				Compile_R4300i_UnknownOpcode(Section);
			} else {
				MipsRegState(Opcode.rd) = STATE_CONST_32;
				MipsRegLo(Opcode.rd) = MipsRegLo(Opcode.rt) ^ MipsRegLo(Opcode.rs);
			}
		} else if (IsMapped(Opcode.rt) && IsMapped(Opcode.rs)) {
			int32_t source1 = Opcode.rd == Opcode.rt?Opcode.rt:Opcode.rs;
			int32_t source2 = Opcode.rd == Opcode.rt?Opcode.rs:Opcode.rt;

			ProtectGPR(Section,source1);
			ProtectGPR(Section,source2);
			if (Is64Bit(Opcode.rt) || Is64Bit(Opcode.rs)) {
				Map_GPR_64bit(Section,Opcode.rd,source1);
				if (Is64Bit(source2)) {
					XorX86RegToX86Reg(MipsRegHi(Opcode.rd),MipsRegHi(source2));
				} else if (IsSigned(source2)) {
					XorX86RegToX86Reg(MipsRegHi(Opcode.rd),Map_TempReg(Section,x86_Any,source2,1));
				}
				XorX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(source2));
			} else {
				if (IsSigned(Opcode.rt) != IsSigned(Opcode.rs)) {
					Map_GPR_32bit(Section,Opcode.rd,1,source1);
				} else {
					Map_GPR_32bit(Section,Opcode.rd,IsSigned(Opcode.rt),source1);
				}
				XorX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(source2));
			}
		} else {
			uint32_t ConstReg = IsConst(Opcode.rt)?Opcode.rt:Opcode.rs;
			uint32_t MappedReg = IsConst(Opcode.rt)?Opcode.rs:Opcode.rt;

			if (Is64Bit(Opcode.rt) || Is64Bit(Opcode.rs)) {
				uint32_t ConstHi, ConstLo;

				ConstHi = Is32Bit(ConstReg)?(uint32_t)(MipsRegLo_S(ConstReg) >> 31):MipsRegHi(ConstReg);
				ConstLo = MipsRegLo(ConstReg);
				Map_GPR_64bit(Section,Opcode.rd,MappedReg);
				if (ConstHi != 0) { XorConstToX86Reg(MipsRegHi(Opcode.rd),ConstHi); }
				if (ConstLo != 0) { XorConstToX86Reg(MipsRegLo(Opcode.rd),ConstLo); }
			} else {
				int32_t Value = MipsRegLo(ConstReg);
				if (IsSigned(Opcode.rt) != IsSigned(Opcode.rs)) {
					Map_GPR_32bit(Section,Opcode.rd,1, MappedReg);
				} else {
					Map_GPR_32bit(Section,Opcode.rd,IsSigned(MappedReg)?1:0, MappedReg);
				}
				if (Value != 0) { XorConstToX86Reg(MipsRegLo(Opcode.rd),Value); }
			}
		}
	} else if (IsKnown(Opcode.rt) || IsKnown(Opcode.rs)) {
		int32_t KnownReg = IsKnown(Opcode.rt)?Opcode.rt:Opcode.rs;
		int32_t UnknownReg = IsKnown(Opcode.rt)?Opcode.rs:Opcode.rt;

		if (IsConst(KnownReg)) {
			uint64_t Value;

			if (Is64Bit(KnownReg)) {
				Value = MipsReg(KnownReg);
			} else {
				if (IsSigned(KnownReg)) {
					Value = (int32_t)MipsRegLo(KnownReg);
				} else {
					Value = MipsRegLo(KnownReg);
				}
			}
			Map_GPR_64bit(Section,Opcode.rd,UnknownReg);
			if ((Value >> 32) != 0) {
				XorConstToX86Reg(MipsRegHi(Opcode.rd),(uint32_t)(Value >> 32));
			}
			if ((uint32_t)Value != 0) {
				XorConstToX86Reg(MipsRegLo(Opcode.rd),(uint32_t)Value);
			}
		} else {
			Map_GPR_64bit(Section,Opcode.rd,KnownReg);
			XorVariableToX86reg(&GPR[UnknownReg].W[1],MipsRegHi(Opcode.rd));
			XorVariableToX86reg(&GPR[UnknownReg].W[0],MipsRegLo(Opcode.rd));
		}
	} else {
		Map_GPR_64bit(Section,Opcode.rd,Opcode.rt);
		XorVariableToX86reg(&GPR[Opcode.rs].W[1],MipsRegHi(Opcode.rd));
		XorVariableToX86reg(&GPR[Opcode.rs].W[0],MipsRegLo(Opcode.rd));
	}
}

void Compile_R4300i_SPECIAL_NOR (BLOCK_SECTION * Section) {


	if (IsKnown(Opcode.rt) && IsKnown(Opcode.rs)) {
		if (IsConst(Opcode.rt) && IsConst(Opcode.rs)) {
			if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }
			if (Is64Bit(Opcode.rt) || Is64Bit(Opcode.rs)) {
				Compile_R4300i_UnknownOpcode(Section);
			} else {
				MipsRegLo(Opcode.rd) = ~(MipsRegLo(Opcode.rt) | MipsRegLo(Opcode.rs));
				MipsRegState(Opcode.rd) = STATE_CONST_32;
			}
		} else if (IsMapped(Opcode.rt) && IsMapped(Opcode.rs)) {
			int32_t source1 = Opcode.rd == Opcode.rt?Opcode.rt:Opcode.rs;
			int32_t source2 = Opcode.rd == Opcode.rt?Opcode.rs:Opcode.rt;

			ProtectGPR(Section,source1);
			ProtectGPR(Section,source2);
			if (Is64Bit(Opcode.rt) || Is64Bit(Opcode.rs)) {
				Map_GPR_64bit(Section,Opcode.rd,source1);
				if (Is64Bit(source2)) {
					OrX86RegToX86Reg(MipsRegHi(Opcode.rd),MipsRegHi(source2));
				} else {
					OrX86RegToX86Reg(MipsRegHi(Opcode.rd),Map_TempReg(Section,x86_Any,source2,1));
				}
				OrX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(source2));
				NotX86Reg(MipsRegHi(Opcode.rd));
				NotX86Reg(MipsRegLo(Opcode.rd));
			} else {
				ProtectGPR(Section,source2);
				if (IsSigned(Opcode.rt) != IsSigned(Opcode.rs)) {
					Map_GPR_32bit(Section,Opcode.rd,1,source1);
				} else {
					Map_GPR_32bit(Section,Opcode.rd,IsSigned(Opcode.rt),source1);
				}
				OrX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(source2));
				NotX86Reg(MipsRegLo(Opcode.rd));
			}
		} else {
			uint32_t ConstReg = IsConst(Opcode.rt)?Opcode.rt:Opcode.rs;
			uint32_t MappedReg = IsConst(Opcode.rt)?Opcode.rs:Opcode.rt;

			if (Is64Bit(Opcode.rt) || Is64Bit(Opcode.rs)) {
				uint64_t Value;

				if (Is64Bit(ConstReg)) {
					Value = MipsReg(ConstReg);
				} else {
					Value = IsSigned(ConstReg)?MipsRegLo_S(ConstReg):MipsRegLo(ConstReg);
				}
				Map_GPR_64bit(Section,Opcode.rd,MappedReg);
				if ((Value >> 32) != 0) {
					OrConstToX86Reg((uint32_t)(Value >> 32),MipsRegHi(Opcode.rd));
				}
				if ((uint32_t)Value != 0) {
					OrConstToX86Reg((uint32_t)Value,MipsRegLo(Opcode.rd));
				}
				NotX86Reg(MipsRegHi(Opcode.rd));
				NotX86Reg(MipsRegLo(Opcode.rd));
			} else {
				int32_t Value = MipsRegLo(ConstReg);
				if (IsSigned(Opcode.rt) != IsSigned(Opcode.rs)) {
					Map_GPR_32bit(Section,Opcode.rd,1, MappedReg);
				} else {
					Map_GPR_32bit(Section,Opcode.rd,IsSigned(MappedReg)?1:0, MappedReg);
				}
				if (Value != 0) { OrConstToX86Reg(Value,MipsRegLo(Opcode.rd)); }
				NotX86Reg(MipsRegLo(Opcode.rd));
			}
		}
	} else if (IsKnown(Opcode.rt) || IsKnown(Opcode.rs)) {
		int32_t KnownReg = IsKnown(Opcode.rt)?Opcode.rt:Opcode.rs;
		int32_t UnknownReg = IsKnown(Opcode.rt)?Opcode.rs:Opcode.rt;

		if (IsConst(KnownReg)) {
			uint64_t Value;

			Value = Is64Bit(KnownReg)?MipsReg(KnownReg):MipsRegLo_S(KnownReg);
			Map_GPR_64bit(Section,Opcode.rd,UnknownReg);
			if ((Value >> 32) != 0) {
				OrConstToX86Reg((uint32_t)(Value >> 32),MipsRegHi(Opcode.rd));
			}
			if ((uint32_t)Value != 0) {
				OrConstToX86Reg((uint32_t)Value,MipsRegLo(Opcode.rd));
			}
		} else {
			Map_GPR_64bit(Section,Opcode.rd,KnownReg);
			OrVariableToX86Reg(&GPR[UnknownReg].W[1],MipsRegHi(Opcode.rd));
			OrVariableToX86Reg(&GPR[UnknownReg].W[0],MipsRegLo(Opcode.rd));
		}
		NotX86Reg(MipsRegHi(Opcode.rd));
		NotX86Reg(MipsRegLo(Opcode.rd));
	} else {
		Map_GPR_64bit(Section,Opcode.rd,Opcode.rt);
		OrVariableToX86Reg(&GPR[Opcode.rs].W[1],MipsRegHi(Opcode.rd));
		OrVariableToX86Reg(&GPR[Opcode.rs].W[0],MipsRegLo(Opcode.rd));
		NotX86Reg(MipsRegHi(Opcode.rd));
		NotX86Reg(MipsRegLo(Opcode.rd));
	}
}

void Compile_R4300i_SPECIAL_SLT (BLOCK_SECTION * Section) {


	if (IsKnown(Opcode.rt) && IsKnown(Opcode.rs)) {
		if (IsConst(Opcode.rt) && IsConst(Opcode.rs)) {
			if (Is64Bit(Opcode.rt) || Is64Bit(Opcode.rs)) {
				DisplayError("1");
				Compile_R4300i_UnknownOpcode(Section);
			} else {
				if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }
				MipsRegState(Opcode.rd) = STATE_CONST_32;
				if (MipsRegLo_S(Opcode.rs) < MipsRegLo_S(Opcode.rt)) {
					MipsRegLo(Opcode.rd) = 1;
				} else {
					MipsRegLo(Opcode.rd) = 0;
				}
			}
		} else if (IsMapped(Opcode.rt) && IsMapped(Opcode.rs)) {
			ProtectGPR(Section,Opcode.rt);
			ProtectGPR(Section,Opcode.rs);
			if (Is64Bit(Opcode.rt) || Is64Bit(Opcode.rs)) {
				uint8_t *Jump[2];

				CompX86RegToX86Reg(
					Is64Bit(Opcode.rs)?MipsRegHi(Opcode.rs):Map_TempReg(Section,x86_Any,Opcode.rs,1),
					Is64Bit(Opcode.rt)?MipsRegHi(Opcode.rt):Map_TempReg(Section,x86_Any,Opcode.rt,1)
				);
				JeLabel8(0);
				Jump[0] = RecompPos - 1;
				SetlVariable(&BranchCompare);
				JmpLabel8(0);
				Jump[1] = RecompPos - 1;



				*((uint8_t *)(Jump[0]))=(uint8_t)(RecompPos - Jump[0] - 1);
				CompX86RegToX86Reg(MipsRegLo(Opcode.rs), MipsRegLo(Opcode.rt));
				SetbVariable(&BranchCompare);


				*((uint8_t *)(Jump[1]))=(uint8_t)(RecompPos - Jump[1] - 1);
				Map_GPR_32bit(Section,Opcode.rd,1, -1);
				MoveVariableToX86reg(&BranchCompare,MipsRegLo(Opcode.rd));
			} else {
				Map_GPR_32bit(Section,Opcode.rd,1, -1);
				CompX86RegToX86Reg(MipsRegLo(Opcode.rs), MipsRegLo(Opcode.rt));

				if (MipsRegLo(Opcode.rd) > x86_EDX) {
					SetlVariable(&BranchCompare);
					MoveVariableToX86reg(&BranchCompare,MipsRegLo(Opcode.rd));
				} else {
					Setl(MipsRegLo(Opcode.rd));
					AndConstToX86Reg(MipsRegLo(Opcode.rd), 1);
				}
			}
		} else {
			uint32_t ConstReg  = IsConst(Opcode.rs)?Opcode.rs:Opcode.rt;
			uint32_t MappedReg = IsConst(Opcode.rs)?Opcode.rt:Opcode.rs;

			ProtectGPR(Section,MappedReg);
			if (Is64Bit(Opcode.rt) || Is64Bit(Opcode.rs)) {
				uint8_t *Jump[2];

				CompConstToX86reg(
					Is64Bit(MappedReg)?MipsRegHi(MappedReg):Map_TempReg(Section,x86_Any,MappedReg,1),
					Is64Bit(ConstReg)?MipsRegHi(ConstReg):(MipsRegLo_S(ConstReg) >> 31)
				);
				JeLabel8(0);
				Jump[0] = RecompPos - 1;
				if (MappedReg == Opcode.rs) {
					SetlVariable(&BranchCompare);
				} else {
					SetgVariable(&BranchCompare);
				}
				JmpLabel8(0);
				Jump[1] = RecompPos - 1;



				*((uint8_t *)(Jump[0]))=(uint8_t)(RecompPos - Jump[0] - 1);
				CompConstToX86reg(MipsRegLo(MappedReg), MipsRegLo(ConstReg));
				if (MappedReg == Opcode.rs) {
					SetbVariable(&BranchCompare);
				} else {
					SetaVariable(&BranchCompare);
				}


				*((uint8_t *)(Jump[1]))=(uint8_t)(RecompPos - Jump[1] - 1);
				Map_GPR_32bit(Section,Opcode.rd,1, -1);
				MoveVariableToX86reg(&BranchCompare,MipsRegLo(Opcode.rd));
			} else {
				uint32_t Constant = MipsRegLo(ConstReg);
				Map_GPR_32bit(Section,Opcode.rd,1, -1);
				CompConstToX86reg(MipsRegLo(MappedReg), Constant);

				if (MipsRegLo(Opcode.rd) > x86_EDX) {
					if (MappedReg == Opcode.rs) {
						SetlVariable(&BranchCompare);
					} else {
						SetgVariable(&BranchCompare);
					}
					MoveVariableToX86reg(&BranchCompare,MipsRegLo(Opcode.rd));
				} else {
					if (MappedReg == Opcode.rs) {
						Setl(MipsRegLo(Opcode.rd));
					} else {
						Setg(MipsRegLo(Opcode.rd));
					}
					AndConstToX86Reg(MipsRegLo(Opcode.rd), 1);
				}
			}
		}
	} else if (IsKnown(Opcode.rt) || IsKnown(Opcode.rs)) {
		uint32_t KnownReg = IsKnown(Opcode.rt)?Opcode.rt:Opcode.rs;
		uint32_t UnknownReg = IsKnown(Opcode.rt)?Opcode.rs:Opcode.rt;
		uint8_t *Jump[2];

		if (IsConst(KnownReg)) {
			if (Is64Bit(KnownReg)) {
				CompConstToVariable(MipsRegHi(KnownReg),&GPR[UnknownReg].W[1]);
			} else {
				CompConstToVariable(((int32_t)MipsRegLo(KnownReg) >> 31),&GPR[UnknownReg].W[1]);
			}
		} else {
			if (Is64Bit(KnownReg)) {
				CompX86regToVariable(MipsRegHi(KnownReg),&GPR[UnknownReg].W[1]);
			} else {
				ProtectGPR(Section,KnownReg);
				CompX86regToVariable(Map_TempReg(Section,x86_Any,KnownReg,1),&GPR[UnknownReg].W[1]);
			}
		}
		JeLabel8(0);
		Jump[0] = RecompPos - 1;
		if (KnownReg == (IsConst(KnownReg)?Opcode.rs:Opcode.rt)) {
			SetgVariable(&BranchCompare);
		} else {
			SetlVariable(&BranchCompare);
		}
		JmpLabel8(0);
		Jump[1] = RecompPos - 1;



		*((uint8_t *)(Jump[0]))=(uint8_t)(RecompPos - Jump[0] - 1);
		if (IsConst(KnownReg)) {
			CompConstToVariable(MipsRegLo(KnownReg),&GPR[UnknownReg].W[0]);
		} else {
			CompX86regToVariable(MipsRegLo(KnownReg),&GPR[UnknownReg].W[0]);
		}
		if (KnownReg == (IsConst(KnownReg)?Opcode.rs:Opcode.rt)) {
			SetaVariable(&BranchCompare);
		} else {
			SetbVariable(&BranchCompare);
		}


		*((uint8_t *)(Jump[1]))=(uint8_t)(RecompPos - Jump[1] - 1);
		Map_GPR_32bit(Section,Opcode.rd,1, -1);
		MoveVariableToX86reg(&BranchCompare,MipsRegLo(Opcode.rd));
	} else {
		uint8_t *Jump[2];
		int32_t x86Reg;

		x86Reg = Map_TempReg(Section,x86_Any,Opcode.rs,1);
		CompX86regToVariable(x86Reg,&GPR[Opcode.rt].W[1]);
		JeLabel8(0);
		Jump[0] = RecompPos - 1;
		SetlVariable(&BranchCompare);
		JmpLabel8(0);
		Jump[1] = RecompPos - 1;



		*((uint8_t *)(Jump[0]))=(uint8_t)(RecompPos - Jump[0] - 1);
		CompX86regToVariable(Map_TempReg(Section,x86Reg,Opcode.rs,0),&GPR[Opcode.rt].W[0]);
		SetbVariable(&BranchCompare);


		*((uint8_t *)(Jump[1]))=(uint8_t)(RecompPos - Jump[1] - 1);
		Map_GPR_32bit(Section,Opcode.rd,1, -1);
		MoveVariableToX86reg(&BranchCompare,MipsRegLo(Opcode.rd));
	}
}

void Compile_R4300i_SPECIAL_SLTU (BLOCK_SECTION * Section) {

	if (Opcode.rd == 0) { return; }

	if (IsKnown(Opcode.rt) && IsKnown(Opcode.rs)) {
		if (IsConst(Opcode.rt) && IsConst(Opcode.rs)) {
			if (Is64Bit(Opcode.rt) || Is64Bit(Opcode.rs)) {
#ifndef EXTERNAL_RELEASE
				DisplayError("1");
#endif
				Compile_R4300i_UnknownOpcode(Section);
			} else {
				if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }
				MipsRegState(Opcode.rd) = STATE_CONST_32;
				if (MipsRegLo(Opcode.rs) < MipsRegLo(Opcode.rt)) {
					MipsRegLo(Opcode.rd) = 1;
				} else {
					MipsRegLo(Opcode.rd) = 0;
				}
			}
		} else if (IsMapped(Opcode.rt) && IsMapped(Opcode.rs)) {
			ProtectGPR(Section,Opcode.rt);
			ProtectGPR(Section,Opcode.rs);
			if (Is64Bit(Opcode.rt) || Is64Bit(Opcode.rs)) {
				uint8_t *Jump[2];

				CompX86RegToX86Reg(
					Is64Bit(Opcode.rs)?MipsRegHi(Opcode.rs):Map_TempReg(Section,x86_Any,Opcode.rs,1),
					Is64Bit(Opcode.rt)?MipsRegHi(Opcode.rt):Map_TempReg(Section,x86_Any,Opcode.rt,1)
				);
				JeLabel8(0);
				Jump[0] = RecompPos - 1;
				SetbVariable(&BranchCompare);
				JmpLabel8(0);
				Jump[1] = RecompPos - 1;



				*((uint8_t *)(Jump[0]))=(uint8_t)(RecompPos - Jump[0] - 1);
				CompX86RegToX86Reg(MipsRegLo(Opcode.rs), MipsRegLo(Opcode.rt));
				SetbVariable(&BranchCompare);


				*((uint8_t *)(Jump[1]))=(uint8_t)(RecompPos - Jump[1] - 1);
				Map_GPR_32bit(Section,Opcode.rd,1, -1);
				MoveVariableToX86reg(&BranchCompare,MipsRegLo(Opcode.rd));
			} else {
				CompX86RegToX86Reg(MipsRegLo(Opcode.rs), MipsRegLo(Opcode.rt));
				SetbVariable(&BranchCompare);
				Map_GPR_32bit(Section,Opcode.rd,1, -1);
				MoveVariableToX86reg(&BranchCompare,MipsRegLo(Opcode.rd));
			}
		} else {
			if (Is64Bit(Opcode.rt) || Is64Bit(Opcode.rs)) {
				uint32_t MappedRegHi, MappedRegLo, ConstHi, ConstLo, MappedReg, ConstReg;
				uint8_t *Jump[2];

				ConstReg  = IsConst(Opcode.rt)?Opcode.rt:Opcode.rs;
				MappedReg = IsConst(Opcode.rt)?Opcode.rs:Opcode.rt;

				ConstLo = MipsRegLo(ConstReg);
				ConstHi = (int32_t)ConstLo >> 31;
				if (Is64Bit(ConstReg)) { ConstHi = MipsRegHi(ConstReg); }

				ProtectGPR(Section,MappedReg);
				MappedRegLo = MipsRegLo(MappedReg);
				MappedRegHi = MipsRegHi(MappedReg);
				if (Is32Bit(MappedReg)) {
					MappedRegHi = Map_TempReg(Section,x86_Any,MappedReg,1);
				}


				Map_GPR_32bit(Section,Opcode.rd,1, -1);
				CompConstToX86reg(MappedRegHi, ConstHi);
				JeLabel8(0);
				Jump[0] = RecompPos - 1;
				if (MappedReg == Opcode.rs) {
					SetbVariable(&BranchCompare);
				} else {
					SetaVariable(&BranchCompare);
				}
				JmpLabel8(0);
				Jump[1] = RecompPos - 1;



				*((uint8_t *)(Jump[0]))=(uint8_t)(RecompPos - Jump[0] - 1);
				CompConstToX86reg(MappedRegLo, ConstLo);
				if (MappedReg == Opcode.rs) {
					SetbVariable(&BranchCompare);
				} else {
					SetaVariable(&BranchCompare);
				}


				*((uint8_t *)(Jump[1]))=(uint8_t)(RecompPos - Jump[1] - 1);
				Map_GPR_32bit(Section,Opcode.rd,1, -1);
				MoveVariableToX86reg(&BranchCompare,MipsRegLo(Opcode.rd));
			} else {
				uint32_t Const = IsConst(Opcode.rs)?MipsRegLo(Opcode.rs):MipsRegLo(Opcode.rt);
				uint32_t MappedReg = IsConst(Opcode.rt)?Opcode.rs:Opcode.rt;

				CompConstToX86reg(MipsRegLo(MappedReg), Const);
				if (MappedReg == Opcode.rs) {
					SetbVariable(&BranchCompare);
				} else {
					SetaVariable(&BranchCompare);
				}
				Map_GPR_32bit(Section,Opcode.rd,1, -1);
				MoveVariableToX86reg(&BranchCompare,MipsRegLo(Opcode.rd));
			}
		}
	} else if (IsKnown(Opcode.rt) || IsKnown(Opcode.rs)) {
		uint32_t KnownReg = IsKnown(Opcode.rt)?Opcode.rt:Opcode.rs;
		uint32_t UnknownReg = IsKnown(Opcode.rt)?Opcode.rs:Opcode.rt;
		uint8_t *Jump[2];

		if (IsConst(KnownReg)) {
			if (Is64Bit(KnownReg)) {
				CompConstToVariable(MipsRegHi(KnownReg),&GPR[UnknownReg].W[1]);
			} else {
				CompConstToVariable(((int32_t)MipsRegLo(KnownReg) >> 31),&GPR[UnknownReg].W[1]);
			}
		} else {
			if (Is64Bit(KnownReg)) {
				CompX86regToVariable(MipsRegHi(KnownReg),&GPR[UnknownReg].W[1]);
			} else {
				ProtectGPR(Section,KnownReg);
				CompX86regToVariable(Map_TempReg(Section,x86_Any,KnownReg,1),&GPR[UnknownReg].W[1]);
			}
		}
		JeLabel8(0);
		Jump[0] = RecompPos - 1;
		if (KnownReg == (IsConst(KnownReg)?Opcode.rs:Opcode.rt)) {
			SetaVariable(&BranchCompare);
		} else {
			SetbVariable(&BranchCompare);
		}
		JmpLabel8(0);
		Jump[1] = RecompPos - 1;



		*((uint8_t *)(Jump[0]))=(uint8_t)(RecompPos - Jump[0] - 1);
		if (IsConst(KnownReg)) {
			CompConstToVariable(MipsRegLo(KnownReg),&GPR[UnknownReg].W[0]);
		} else {
			CompX86regToVariable(MipsRegLo(KnownReg),&GPR[UnknownReg].W[0]);
		}
		if (KnownReg == (IsConst(KnownReg)?Opcode.rs:Opcode.rt)) {
			SetaVariable(&BranchCompare);
		} else {
			SetbVariable(&BranchCompare);
		}


		*((uint8_t *)(Jump[1]))=(uint8_t)(RecompPos - Jump[1] - 1);
		Map_GPR_32bit(Section,Opcode.rd,1, -1);
		MoveVariableToX86reg(&BranchCompare,MipsRegLo(Opcode.rd));
	} else {
		uint8_t *Jump[2];
		int32_t x86Reg;

		x86Reg = Map_TempReg(Section,x86_Any,Opcode.rs,1);
		CompX86regToVariable(x86Reg,&GPR[Opcode.rt].W[1]);
		JeLabel8(0);
		Jump[0] = RecompPos - 1;
		SetbVariable(&BranchCompare);
		JmpLabel8(0);
		Jump[1] = RecompPos - 1;



		*((uint8_t *)(Jump[0]))=(uint8_t)(RecompPos - Jump[0] - 1);
		CompX86regToVariable(Map_TempReg(Section,x86Reg,Opcode.rs,0),&GPR[Opcode.rt].W[0]);
		SetbVariable(&BranchCompare);


		*((uint8_t *)(Jump[1]))=(uint8_t)(RecompPos - Jump[1] - 1);
		Map_GPR_32bit(Section,Opcode.rd,1, -1);
		MoveVariableToX86reg(&BranchCompare,MipsRegLo(Opcode.rd));
	}
}

void Compile_R4300i_SPECIAL_DADD (BLOCK_SECTION * Section) {

	if (Opcode.rd == 0) { return; }

	if (IsConst(Opcode.rt)  && IsConst(Opcode.rs)) {
		if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }
		MipsReg(Opcode.rd) =
			Is64Bit(Opcode.rs)?MipsReg(Opcode.rs):(int64_t)MipsRegLo_S(Opcode.rs) +
			Is64Bit(Opcode.rt)?MipsReg(Opcode.rt):(int64_t)MipsRegLo_S(Opcode.rt);
		if (MipsRegLo_S(Opcode.rd) < 0 && MipsRegHi_S(Opcode.rd) == -1){
			MipsRegState(Opcode.rd) = STATE_CONST_32;
		} else if (MipsRegLo_S(Opcode.rd) >= 0 && MipsRegHi_S(Opcode.rd) == 0){
			MipsRegState(Opcode.rd) = STATE_CONST_32;
		} else {
			MipsRegState(Opcode.rd) = STATE_CONST_64;
		}
	} else {
		Map_GPR_64bit(Section,Opcode.rd,Opcode.rs);
		if (IsConst(Opcode.rt)) {
			AddConstToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(Opcode.rt));
			AddConstToX86Reg(MipsRegHi(Opcode.rd),MipsRegHi(Opcode.rt));
		} else if (IsMapped(Opcode.rt)) {
			int32_t HiReg = Is64Bit(Opcode.rt)?MipsRegHi(Opcode.rt):Map_TempReg(Section,x86_Any,Opcode.rt,1);
			ProtectGPR(Section,Opcode.rt);
			AddX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(Opcode.rt));
			AdcX86RegToX86Reg(MipsRegHi(Opcode.rd),HiReg);
		} else {
			AddVariableToX86reg(MipsRegLo(Opcode.rd),&GPR[Opcode.rt].W[0]);
			AdcVariableToX86reg(MipsRegHi(Opcode.rd),&GPR[Opcode.rt].W[1]);
		}
	}
}

void Compile_R4300i_SPECIAL_DADDU (BLOCK_SECTION * Section) {

	if (Opcode.rd == 0) { return; }

	if (IsConst(Opcode.rt)  && IsConst(Opcode.rs)) {
		int64_t ValRs = Is64Bit(Opcode.rs)?MipsReg(Opcode.rs):(int64_t)MipsRegLo_S(Opcode.rs);
		int64_t ValRt = Is64Bit(Opcode.rt)?MipsReg(Opcode.rt):(int64_t)MipsRegLo_S(Opcode.rt);
		if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }
		MipsReg(Opcode.rd) = ValRs + ValRt;
		if ((MipsRegHi(Opcode.rd) == 0) && (MipsRegLo(Opcode.rd) & 0x80000000) == 0) {
			MipsRegState(Opcode.rd) = STATE_CONST_32;
		} else if ((MipsRegHi(Opcode.rd) == 0xFFFFFFFF) && (MipsRegLo(Opcode.rd) & 0x80000000) != 0) {
			MipsRegState(Opcode.rd) = STATE_CONST_32;
		} else {
			MipsRegState(Opcode.rd) = STATE_CONST_64;
		}
	} else {
		Map_GPR_64bit(Section,Opcode.rd,Opcode.rs);
		if (IsConst(Opcode.rt)) {
			AddConstToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(Opcode.rt));
			AddConstToX86Reg(MipsRegHi(Opcode.rd),MipsRegHi(Opcode.rt));
		} else if (IsMapped(Opcode.rt)) {
			int32_t HiReg = Is64Bit(Opcode.rt)?MipsRegHi(Opcode.rt):Map_TempReg(Section,x86_Any,Opcode.rt,1);
			ProtectGPR(Section,Opcode.rt);
			AddX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(Opcode.rt));
			AdcX86RegToX86Reg(MipsRegHi(Opcode.rd),HiReg);
		} else {
			AddVariableToX86reg(MipsRegLo(Opcode.rd),&GPR[Opcode.rt].W[0]);
			AdcVariableToX86reg(MipsRegHi(Opcode.rd),&GPR[Opcode.rt].W[1]);
		}
	}
}

void Compile_R4300i_SPECIAL_DSUB (BLOCK_SECTION * Section) {

	if (Opcode.rd == 0) { return; }

	if (IsConst(Opcode.rt)  && IsConst(Opcode.rs)) {
		if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }
		MipsReg(Opcode.rd) =
			Is64Bit(Opcode.rs)?MipsReg(Opcode.rs):(int64_t)MipsRegLo_S(Opcode.rs) -
			Is64Bit(Opcode.rt)?MipsReg(Opcode.rt):(int64_t)MipsRegLo_S(Opcode.rt);
		if (MipsRegLo_S(Opcode.rd) < 0 && MipsRegHi_S(Opcode.rd) == -1){
			MipsRegState(Opcode.rd) = STATE_CONST_32;
		} else if (MipsRegLo_S(Opcode.rd) >= 0 && MipsRegHi_S(Opcode.rd) == 0){
			MipsRegState(Opcode.rd) = STATE_CONST_32;
		} else {
			MipsRegState(Opcode.rd) = STATE_CONST_64;
		}
	} else {
		if (Opcode.rd == Opcode.rt) {
			int32_t HiReg = Map_TempReg(Section,x86_Any,Opcode.rt,1);
			int32_t LoReg = Map_TempReg(Section,x86_Any,Opcode.rt,0);
			Map_GPR_64bit(Section,Opcode.rd,Opcode.rs);
			SubX86RegToX86Reg(MipsRegLo(Opcode.rd),LoReg);
			SbbX86RegToX86Reg(MipsRegHi(Opcode.rd),HiReg);
			return;
		}
		Map_GPR_64bit(Section,Opcode.rd,Opcode.rs);
		if (IsConst(Opcode.rt)) {
			SubConstFromX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(Opcode.rt));
			SbbConstFromX86Reg(MipsRegHi(Opcode.rd),MipsRegHi(Opcode.rt));
		} else if (IsMapped(Opcode.rt)) {
			int32_t HiReg = Is64Bit(Opcode.rt)?MipsRegHi(Opcode.rt):Map_TempReg(Section,x86_Any,Opcode.rt,1);
			ProtectGPR(Section,Opcode.rt);
			SubX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(Opcode.rt));
			SbbX86RegToX86Reg(MipsRegHi(Opcode.rd),HiReg);
		} else {
			SubVariableFromX86reg(MipsRegLo(Opcode.rd),&GPR[Opcode.rt].W[0]);
			SbbVariableFromX86reg(MipsRegHi(Opcode.rd),&GPR[Opcode.rt].W[1]);
		}
	}
}

void Compile_R4300i_SPECIAL_DSUBU (BLOCK_SECTION * Section) {

	if (Opcode.rd == 0) { return; }

	if (IsConst(Opcode.rt)  && IsConst(Opcode.rs)) {
		if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }
		MipsReg(Opcode.rd) =
			Is64Bit(Opcode.rs)?MipsReg(Opcode.rs):(int64_t)MipsRegLo_S(Opcode.rs) -
			Is64Bit(Opcode.rt)?MipsReg(Opcode.rt):(int64_t)MipsRegLo_S(Opcode.rt);
		if (MipsRegLo_S(Opcode.rd) < 0 && MipsRegHi_S(Opcode.rd) == -1){
			MipsRegState(Opcode.rd) = STATE_CONST_32;
		} else if (MipsRegLo_S(Opcode.rd) >= 0 && MipsRegHi_S(Opcode.rd) == 0){
			MipsRegState(Opcode.rd) = STATE_CONST_32;
		} else {
			MipsRegState(Opcode.rd) = STATE_CONST_64;
		}
	} else {
		if (Opcode.rd == Opcode.rt) {
			int32_t HiReg = Map_TempReg(Section,x86_Any,Opcode.rt,1);
			int32_t LoReg = Map_TempReg(Section,x86_Any,Opcode.rt,0);
			Map_GPR_64bit(Section,Opcode.rd,Opcode.rs);
			SubX86RegToX86Reg(MipsRegLo(Opcode.rd),LoReg);
			SbbX86RegToX86Reg(MipsRegHi(Opcode.rd),HiReg);
			return;
		}
		Map_GPR_64bit(Section,Opcode.rd,Opcode.rs);
		if (IsConst(Opcode.rt)) {
			SubConstFromX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(Opcode.rt));
			SbbConstFromX86Reg(MipsRegHi(Opcode.rd),MipsRegHi(Opcode.rt));
		} else if (IsMapped(Opcode.rt)) {
			int32_t HiReg = Is64Bit(Opcode.rt)?MipsRegHi(Opcode.rt):Map_TempReg(Section,x86_Any,Opcode.rt,1);
			ProtectGPR(Section,Opcode.rt);
			SubX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(Opcode.rt));
			SbbX86RegToX86Reg(MipsRegHi(Opcode.rd),HiReg);
		} else {
			SubVariableFromX86reg(MipsRegLo(Opcode.rd),&GPR[Opcode.rt].W[0]);
			SbbVariableFromX86reg(MipsRegHi(Opcode.rd),&GPR[Opcode.rt].W[1]);
		}
	}
}

void Compile_R4300i_SPECIAL_DSLL (BLOCK_SECTION * Section) {


	if (Opcode.rd == 0) { return; }
	if (IsConst(Opcode.rt)) {
		if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }

		MipsReg(Opcode.rd) = Is64Bit(Opcode.rt)?MipsReg(Opcode.rt):(int64_t)MipsRegLo_S(Opcode.rt) << Opcode.sa;
		if (MipsRegLo_S(Opcode.rd) < 0 && MipsRegHi_S(Opcode.rd) == -1){
			MipsRegState(Opcode.rd) = STATE_CONST_32;
		} else if (MipsRegLo_S(Opcode.rd) >= 0 && MipsRegHi_S(Opcode.rd) == 0){
			MipsRegState(Opcode.rd) = STATE_CONST_32;
		} else {
			MipsRegState(Opcode.rd) = STATE_CONST_64;
		}
		return;
	}

	Map_GPR_64bit(Section,Opcode.rd,Opcode.rt);
	ShiftLeftDoubleImmed(MipsRegHi(Opcode.rd),MipsRegLo(Opcode.rd),(uint8_t)Opcode.sa);
	ShiftLeftSignImmed(	MipsRegLo(Opcode.rd),(uint8_t)Opcode.sa);
}

void Compile_R4300i_SPECIAL_DSRL (BLOCK_SECTION * Section) {


	if (Opcode.rd == 0) { return; }
	if (IsConst(Opcode.rt)) {
		if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }

		MipsReg(Opcode.rd) = Is64Bit(Opcode.rt)?MipsReg(Opcode.rt):(QWORD)MipsRegLo_S(Opcode.rt) >> Opcode.sa;
		if (MipsRegLo_S(Opcode.rd) < 0 && MipsRegHi_S(Opcode.rd) == -1){
			MipsRegState(Opcode.rd) = STATE_CONST_32;
		} else if (MipsRegLo_S(Opcode.rd) >= 0 && MipsRegHi_S(Opcode.rd) == 0){
			MipsRegState(Opcode.rd) = STATE_CONST_32;
		} else {
			MipsRegState(Opcode.rd) = STATE_CONST_64;
		}
		return;
	}
	Map_GPR_64bit(Section,Opcode.rd,Opcode.rt);
	ShiftRightDoubleImmed(MipsRegLo(Opcode.rd),MipsRegHi(Opcode.rd),(uint8_t)Opcode.sa);
	ShiftRightUnsignImmed(MipsRegHi(Opcode.rd),(uint8_t)Opcode.sa);
}

void Compile_R4300i_SPECIAL_DSRA (BLOCK_SECTION * Section) {


	if (Opcode.rd == 0) { return; }
	if (IsConst(Opcode.rt)) {
		if (IsMapped(Opcode.rd)) { UnMap_GPR(Section,Opcode.rd, 0); }

		MipsReg_S(Opcode.rd) = Is64Bit(Opcode.rt)?MipsReg_S(Opcode.rt):(int64_t)MipsRegLo_S(Opcode.rt) >> Opcode.sa;
		if (MipsRegLo_S(Opcode.rd) < 0 && MipsRegHi_S(Opcode.rd) == -1){
			MipsRegState(Opcode.rd) = STATE_CONST_32;
		} else if (MipsRegLo_S(Opcode.rd) >= 0 && MipsRegHi_S(Opcode.rd) == 0){
			MipsRegState(Opcode.rd) = STATE_CONST_32;
		} else {
			MipsRegState(Opcode.rd) = STATE_CONST_64;
		}
		return;
	}
	Map_GPR_64bit(Section,Opcode.rd,Opcode.rt);
	ShiftRightDoubleImmed(MipsRegLo(Opcode.rd),MipsRegHi(Opcode.rd),(uint8_t)Opcode.sa);
	ShiftRightSignImmed(MipsRegHi(Opcode.rd),(uint8_t)Opcode.sa);
}

void Compile_R4300i_SPECIAL_DSLL32 (BLOCK_SECTION * Section) {


	if (Opcode.rd == 0) { return; }
	if (IsConst(Opcode.rt)) {
		if (Opcode.rt != Opcode.rd) { UnMap_GPR(Section,Opcode.rd, 0); }
		MipsRegHi(Opcode.rd) = MipsRegLo(Opcode.rt) << Opcode.sa;
		MipsRegLo(Opcode.rd) = 0;
		if (MipsRegLo_S(Opcode.rd) < 0 && MipsRegHi_S(Opcode.rd) == -1){
			MipsRegState(Opcode.rd) = STATE_CONST_32;
		} else if (MipsRegLo_S(Opcode.rd) >= 0 && MipsRegHi_S(Opcode.rd) == 0){
			MipsRegState(Opcode.rd) = STATE_CONST_32;
		} else {
			MipsRegState(Opcode.rd) = STATE_CONST_64;
		}

	} else if (IsMapped(Opcode.rt)) {
		ProtectGPR(Section,Opcode.rt);
		Map_GPR_64bit(Section,Opcode.rd,-1);
		if (Opcode.rt != Opcode.rd) {
			MoveX86RegToX86Reg(MipsRegLo(Opcode.rt),MipsRegHi(Opcode.rd));
		} else {
			int32_t HiReg = MipsRegHi(Opcode.rt);
			MipsRegHi(Opcode.rt) = MipsRegLo(Opcode.rt);
			MipsRegLo(Opcode.rt) = HiReg;
		}
		if ((uint8_t)Opcode.sa != 0) {
			ShiftLeftSignImmed(MipsRegHi(Opcode.rd),(uint8_t)Opcode.sa);
		}
		XorX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(Opcode.rd));
	} else {
		Map_GPR_64bit(Section,Opcode.rd,-1);
		MoveVariableToX86reg(&GPR[Opcode.rt],MipsRegHi(Opcode.rd));
		if ((uint8_t)Opcode.sa != 0) {
			ShiftLeftSignImmed(MipsRegHi(Opcode.rd),(uint8_t)Opcode.sa);
		}
		XorX86RegToX86Reg(MipsRegLo(Opcode.rd),MipsRegLo(Opcode.rd));
	}
}

void Compile_R4300i_SPECIAL_DSRL32 (BLOCK_SECTION * Section) {

	if (IsConst(Opcode.rt)) {
		if (Opcode.rt != Opcode.rd) { UnMap_GPR(Section,Opcode.rd, 0); }
		MipsRegState(Opcode.rd) = STATE_CONST_32;
		MipsRegLo(Opcode.rd) = (uint32_t)(MipsReg(Opcode.rt) >> (Opcode.sa + 32));
	} else if (IsMapped(Opcode.rt)) {
		ProtectGPR(Section,Opcode.rt);
		if (Is64Bit(Opcode.rt)) {
			if (Opcode.rt == Opcode.rd) {
				int32_t HiReg = MipsRegHi(Opcode.rt);
				MipsRegHi(Opcode.rt) = MipsRegLo(Opcode.rt);
				MipsRegLo(Opcode.rt) = HiReg;
				Map_GPR_32bit(Section,Opcode.rd,0,-1);
			} else {
				Map_GPR_32bit(Section,Opcode.rd,0,-1);
				MoveX86RegToX86Reg(MipsRegHi(Opcode.rt),MipsRegLo(Opcode.rd));
			}
			if ((uint8_t)Opcode.sa != 0) {
				ShiftRightUnsignImmed(MipsRegLo(Opcode.rd),(uint8_t)Opcode.sa);
			}
		} else {
			Compile_R4300i_UnknownOpcode(Section);
		}
	} else {
		Map_GPR_32bit(Section,Opcode.rd,0,-1);
		MoveVariableToX86reg(&GPR[Opcode.rt].UW[1],MipsRegLo(Opcode.rd));
		if ((uint8_t)Opcode.sa != 0) {
			ShiftRightUnsignImmed(MipsRegLo(Opcode.rd),(uint8_t)Opcode.sa);
		}
	}
}

void Compile_R4300i_SPECIAL_DSRA32 (BLOCK_SECTION * Section) {

	if (IsConst(Opcode.rt)) {
		if (Opcode.rt != Opcode.rd) { UnMap_GPR(Section,Opcode.rd, 0); }
		MipsRegState(Opcode.rd) = STATE_CONST_32;
		MipsRegLo(Opcode.rd) = (uint32_t)(MipsReg_S(Opcode.rt) >> (Opcode.sa + 32));
	} else if (IsMapped(Opcode.rt)) {
		ProtectGPR(Section,Opcode.rt);
		if (Is64Bit(Opcode.rt)) {
			if (Opcode.rt == Opcode.rd) {
				int32_t HiReg = MipsRegHi(Opcode.rt);
				MipsRegHi(Opcode.rt) = MipsRegLo(Opcode.rt);
				MipsRegLo(Opcode.rt) = HiReg;
				Map_GPR_32bit(Section,Opcode.rd,1,-1);
			} else {
				Map_GPR_32bit(Section,Opcode.rd,1,-1);
				MoveX86RegToX86Reg(MipsRegHi(Opcode.rt),MipsRegLo(Opcode.rd));
			}
			if ((uint8_t)Opcode.sa != 0) {
				ShiftRightSignImmed(MipsRegLo(Opcode.rd),(uint8_t)Opcode.sa);
			}
		} else {
			Compile_R4300i_UnknownOpcode(Section);
		}
	} else {
		Map_GPR_32bit(Section,Opcode.rd,1,-1);
		MoveVariableToX86reg(&GPR[Opcode.rt].UW[1],MipsRegLo(Opcode.rd));
		if ((uint8_t)Opcode.sa != 0) {
			ShiftRightSignImmed(MipsRegLo(Opcode.rd),(uint8_t)Opcode.sa);
		}
	}
}

/************************** COP0 functions **************************/
void Compile_R4300i_COP0_MF(BLOCK_SECTION * Section) {


	switch (Opcode.rd) {
	case 9: //Count
		AddConstToVariable(BlockCycleCount,&CP0[9]);
		SubConstFromVariable(BlockCycleCount,&Timers->Timer);
		BlockCycleCount = 0;
	}
	Map_GPR_32bit(Section,Opcode.rt,1,-1);
	MoveVariableToX86reg(&CP0[Opcode.rd],MipsRegLo(Opcode.rt));
}

void Compile_R4300i_COP0_MT (BLOCK_SECTION * Section) {
	int32_t OldStatusReg;
	uint8_t *Jump;



	switch (Opcode.rd) {
	case 0: //Index
	case 2: //EntryLo0
	case 3: //EntryLo1
	case 4: //Context
	case 5: //PageMask
	case 9: //Count
	case 10: //Entry Hi
	case 11: //Compare
	case 14: //EPC
	case 16: //Config
	case 18: //WatchLo
	case 19: //WatchHi
	case 28: //Tag lo
	case 29: //Tag Hi
	case 30: //ErrEPC
		if (IsConst(Opcode.rt)) {
			MoveConstToVariable(MipsRegLo(Opcode.rt), &CP0[Opcode.rd]);
		} else if (IsMapped(Opcode.rt)) {
			MoveX86regToVariable(MipsRegLo(Opcode.rt), &CP0[Opcode.rd]);
		} else {
			MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.rt,0), &CP0[Opcode.rd]);
		}
		switch (Opcode.rd) {
		case 4: //Context
			AndConstToVariable(0xFF800000,&CP0[Opcode.rd]);
			break;
		case 9: //Count
			BlockCycleCount = 0;
			Pushad();
			Call_Direct(ChangeCompareTimer);
			Popad();
			break;
		case 11: //Compare
			AddConstToVariable(BlockCycleCount,&CP0[9]);
			SubConstFromVariable(BlockCycleCount,&Timers->Timer);
			BlockCycleCount = 0;
			AndConstToVariable(~CAUSE_IP7,&FAKE_CAUSE_REGISTER);
			Pushad();
			Call_Direct(ChangeCompareTimer);
			Popad();
		}
		break;
	case 12: //Status
		OldStatusReg = Map_TempReg(Section,x86_Any,-1,0);
		MoveVariableToX86reg(&CP0[Opcode.rd],OldStatusReg);
		if (IsConst(Opcode.rt)) {
			MoveConstToVariable(MipsRegLo(Opcode.rt), &CP0[Opcode.rd]);
		} else if (IsMapped(Opcode.rt)) {
			MoveX86regToVariable(MipsRegLo(Opcode.rt), &CP0[Opcode.rd]);
		} else {
			MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.rt,0), &CP0[Opcode.rd]);
		}
		XorVariableToX86reg(&CP0[Opcode.rd],OldStatusReg);
		TestConstToX86Reg(STATUS_FR,OldStatusReg);
		JeLabel8(0);
		Jump = RecompPos - 1;
		Pushad();
		Call_Direct(SetFpuLocations);
		Popad();
		*(uint8_t *)(Jump)= (uint8_t )(((uint8_t )(RecompPos)) - (((uint8_t )(Jump)) + 1));

		//TestConstToX86Reg(STATUS_FR,OldStatusReg);
		//CompileExit(Section->CompilePC+4,Section->RegWorking,ExitResetRecompCode,0,JneLabel32);
		Pushad();
		Call_Direct(CheckInterrupts);
		Popad();
		break;
	case 6: //Wired
		Pushad();
		if (BlockRandomModifier != 0) { SubConstFromVariable(BlockRandomModifier,&CP0[1]); }
		BlockRandomModifier = 0;
		Call_Direct(FixRandomReg);
		Popad();
		if (IsConst(Opcode.rt)) {
			MoveConstToVariable(MipsRegLo(Opcode.rt), &CP0[Opcode.rd]);
		} else if (IsMapped(Opcode.rt)) {
			MoveX86regToVariable(MipsRegLo(Opcode.rt), &CP0[Opcode.rd]);
		} else {
			MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.rt,0), &CP0[Opcode.rd]);
		}
		break;
	case 13: //cause
		if (IsConst(Opcode.rt)) {
			AndConstToVariable(0xFFFFCFF,&CP0[Opcode.rd]);
#ifndef EXTERNAL_RELEASE
			if ((MipsRegLo(Opcode.rt) & 0x300) != 0 ){ DisplayError("Set IP0 or IP1"); }
#endif
		} else {
			Compile_R4300i_UnknownOpcode(Section);
		}
		Pushad();
		Call_Direct(CheckInterrupts);
		Popad();
		break;
	default:
		Compile_R4300i_UnknownOpcode(Section);
	}
}

/************************** COP0 CO functions ***********************/
void Compile_R4300i_COP0_CO_TLBR( BLOCK_SECTION * Section) {
	Pushad();
	Call_Direct(TLB_Read);
	Popad();
}

void Compile_R4300i_COP0_CO_TLBWI( BLOCK_SECTION * Section) {
	Pushad();
	MoveVariableToX86reg(&INDEX_REGISTER,x86_EDI);
	AndConstToX86Reg(x86_EDI,0x1F);
#ifdef USEX64
	Call_Direct(WriteTLBEntry);
#else
	Push(x86_ECX);
	Call_Direct(WriteTLBEntry);
	AddConstToX86Reg(x86_ESP, 0x4);
#endif
	Popad();
}

void Compile_R4300i_COP0_CO_TLBWR( BLOCK_SECTION * Section) {
	if (BlockRandomModifier != 0) { SubConstFromVariable(BlockRandomModifier,&CP0[1]); }
	BlockRandomModifier = 0;
	Pushad();
	//Call_Direct(FixRandomReg);
	MoveVariableToX86reg(&RANDOM_REGISTER,x86_EDI);
	AndConstToX86Reg(x86_EDI,0x1F);
	//BreakPoint();
	//Push(x86_ECX);
#ifdef USEX64
	Call_Direct(WriteTLBEntry);
#else
	Push(x86_ECX);
	Call_Direct(WriteTLBEntry);
	AddConstToX86Reg(x86_ESP, 0x4);
#endif

	//AddConstToX86Reg(x86_ESP,4);
	Popad();
}



void Compile_R4300i_COP0_CO_TLBP( BLOCK_SECTION * Section) {
	Pushad();
	Call_Direct(TLB_Probe);
	Popad();
}

void compiler_COP0_CO_ERET (void) {
	if ((STATUS_REGISTER & STATUS_ERL) != 0) {
		PROGRAM_COUNTER = ERROREPC_REGISTER;
		STATUS_REGISTER &= ~STATUS_ERL;
	} else {
		PROGRAM_COUNTER = EPC_REGISTER;
		STATUS_REGISTER &= ~STATUS_EXL;
	}
	LLBit = 0;
	CheckInterrupts();
}

void Compile_R4300i_COP0_CO_ERET( BLOCK_SECTION * Section) {
	WriteBackRegisters(Section);
	Pushad();
	Call_Direct(compiler_COP0_CO_ERET);
	Popad();
	CompileExit((uint32_t)-1,&Section->RegWorking,Normal,1,NULL);
	NextInstruction = END_BLOCK;
}

/************************** Other functions **************************/
void Compile_R4300i_UnknownOpcode (BLOCK_SECTION * Section) {

//	Int3();
	FreeSection(Section->ContinueSection,Section);
	FreeSection(Section->JumpSection,Section);
	BlockCycleCount -= 2;
	BlockRandomModifier -= 1;
	MoveConstToVariable(Section->CompilePC,&PROGRAM_COUNTER);
	WriteBackRegisters(Section);
	AddConstToVariable(BlockCycleCount,&CP0[9]);
	SubConstFromVariable(BlockRandomModifier,&CP0[1]);
	MoveConstToVariable(Opcode.Hex,&Opcode.Hex);
	Pushad();
	Call_Direct(R4300i_UnknownOpcode);
	Popad();
	Ret();
	if (NextInstruction == NORMAL) { NextInstruction = END_BLOCK; }
}

