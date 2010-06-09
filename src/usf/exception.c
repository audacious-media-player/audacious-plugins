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
#include "recompiler_cpu.h"
#include "x86.h"

void CheckInterrupts ( void ) {

	MI_INTR_REG &= ~MI_INTR_AI;
	MI_INTR_REG |= (AudioIntrReg & MI_INTR_AI);
	if ((MI_INTR_MASK_REG & MI_INTR_REG) != 0) {
		FAKE_CAUSE_REGISTER |= CAUSE_IP2;
	} else  {
		FAKE_CAUSE_REGISTER &= ~CAUSE_IP2;
	}

	if (( STATUS_REGISTER & STATUS_IE   ) == 0 ) { return; }
	if (( STATUS_REGISTER & STATUS_EXL  ) != 0 ) { return; }
	if (( STATUS_REGISTER & STATUS_ERL  ) != 0 ) { return; }

	if (( STATUS_REGISTER & FAKE_CAUSE_REGISTER & 0xFF00) != 0) {
		if (!CPU_Action->DoInterrupt) {
			CPU_Action->DoSomething = 1;
			CPU_Action->DoInterrupt = 1;
		}
	}
}

void DoAddressError ( uint32_t DelaySlot, uint32_t BadVaddr, uint32_t FromRead) {
	if (FromRead) {
		CAUSE_REGISTER = EXC_RADE;
	} else {
		CAUSE_REGISTER = EXC_WADE;
	}
	BAD_VADDR_REGISTER = BadVaddr;
	if (DelaySlot) {
		CAUSE_REGISTER |= CAUSE_BD;
		EPC_REGISTER = PROGRAM_COUNTER - 4;
	} else {
		EPC_REGISTER = PROGRAM_COUNTER;
	}
	STATUS_REGISTER |= STATUS_EXL;
	PROGRAM_COUNTER = 0x80000180;
}

void DoBreakException ( uint32_t DelaySlot) {
	CAUSE_REGISTER = EXC_BREAK;
	if (DelaySlot) {
		CAUSE_REGISTER |= CAUSE_BD;
		EPC_REGISTER = PROGRAM_COUNTER - 4;
	} else {
		EPC_REGISTER = PROGRAM_COUNTER;
	}
	STATUS_REGISTER |= STATUS_EXL;
	PROGRAM_COUNTER = 0x80000180;
}

void DoCopUnusableException ( uint32_t DelaySlot, uint32_t Coprocessor ) {
	CAUSE_REGISTER = EXC_CPU;
	if (Coprocessor == 1) { CAUSE_REGISTER |= 0x10000000; }
	if (DelaySlot) {
		CAUSE_REGISTER |= CAUSE_BD;
		EPC_REGISTER = PROGRAM_COUNTER - 4;
	} else {
		EPC_REGISTER = PROGRAM_COUNTER;
	}
	STATUS_REGISTER |= STATUS_EXL;
	PROGRAM_COUNTER = 0x80000180;
}

void DoIntrException ( uint32_t DelaySlot ) {

	if (( STATUS_REGISTER & STATUS_IE   ) == 0 ) { return; }
	if (( STATUS_REGISTER & STATUS_EXL  ) != 0 ) { return; }
	if (( STATUS_REGISTER & STATUS_ERL  ) != 0 ) { return; }
	CAUSE_REGISTER = FAKE_CAUSE_REGISTER;
	CAUSE_REGISTER |= EXC_INT;
	EPC_REGISTER = PROGRAM_COUNTER;
	if (DelaySlot) {
		CAUSE_REGISTER |= CAUSE_BD;
		EPC_REGISTER -= 4;
	}
	STATUS_REGISTER |= STATUS_EXL;
	PROGRAM_COUNTER = 0x80000180;
}

/*
#ifdef USEX64
void CompileDoIntrException(void) {
	uint8_t * Jump, * Jump2;

	MoveVariableToX86reg(&STATUS_REGISTER, x86_TEMPD);
	AndConstToX86Reg(x86_TEMPD,7);
	CompConstToX86reg(x86_TEMPD,1);
	JneLabel8(0);
	Jump = RecompPos - 1;
	Push(x86_RAX);

	MoveVariableToX86reg(&FAKE_CAUSE_REGISTER, x86_EAX);
	OrConstToX86Reg(EXC_INT, x86_EAX);

	CompConstToX86reg(x86_ECX,0);
	MoveVariableToX86reg(&PROGRAM_COUNTER, x86_ECX);
	JeLabel8(0);
	Jump2 = RecompPos - 1;
	OrConstToX86Reg(CAUSE_BD, x86_EAX);
	SubConstFromX86Reg(x86_ECX,4);

	SetJump8(Jump2, RecompPos);

	MoveX86regToVariable(x86_ECX, &EPC_REGISTER);
	MoveX86regToVariable(x86_EAX, &CAUSE_REGISTER);

	OrConstToVariable(STATUS_EXL, &STATUS_REGISTER);
	MoveConstToVariable(0x80000180, &PROGRAM_COUNTER);
    Pop(x86_RAX);
    SetJump8(Jump, RecompPos);

}
#endif

#ifdef USEX64
void CompileCheckInterrupts(void) {
	uint8_t * Jump, * Jump2, * Jump3;

	BreakPoint();
	Push(x86_EAX);
	Push(x86_EDX);

	MoveVariableToX86reg(&MI_INTR_REG, x86_EAX);
	MoveVariableToX86reg(&FAKE_CAUSE_REGISTER, x86_EDX);
	AndConstToX86Reg(x86_EAX, ~MI_INTR_AI);
	OrConstToX86Reg(CAUSE_IP2, x86_EDX);
	TestVariableToX86Reg(x86_EAX, &MI_INTR_MASK_REG);
	JnzLabel8(0);
	Jump = RecompPos - 1;
	AndConstToX86Reg(x86_EDX, ~CAUSE_IP2);
    SetJump8(Jump, RecompPos);

   	MoveVariableToX86reg(&STATUS_REGISTER, x86_TEMPD);
	AndConstToX86Reg(x86_TEMPD,7);
	CompConstToX86reg(x86_TEMPD,1);
	JneLabel8(0);
	Jump2 = RecompPos - 1;

	MoveVariableToX86reg(&STATUS_REGISTER, x86_TEMPD);
	AddX86RegToX86Reg(x86_TEMPD, x86_EDX);
	AndConstToX86Reg(x86_TEMPD,0xFF00);

	CompConstToVariable(0, &CPU_Action->DoInterrupt);
	JneLabel8(0);
	Jump = RecompPos - 1;

	MoveConstToVariable(1, &CPU_Action->DoInterrupt);
	MoveConstToVariable(1, &CPU_Action->DoSomething);

	SetJump8(Jump, RecompPos);
	SetJump8(Jump2, RecompPos);

	MoveX86regToVariable(x86_EAX, &MI_INTR_REG);
	MoveX86regToVariable(x86_EDX, &FAKE_CAUSE_REGISTER);
	Pop(x86_EDX);
	Pop(x86_EAX);
}
#endif
*/

void DoTLBMiss ( uint32_t DelaySlot, uint32_t BadVaddr ) {

	CAUSE_REGISTER = EXC_RMISS;
	BAD_VADDR_REGISTER = BadVaddr;
	CONTEXT_REGISTER &= 0xFF80000F;
	CONTEXT_REGISTER |= (BadVaddr >> 9) & 0x007FFFF0;
	ENTRYHI_REGISTER = (BadVaddr & 0xFFFFE000);
	if ((STATUS_REGISTER & STATUS_EXL) == 0) {
		if (DelaySlot) {
			CAUSE_REGISTER |= CAUSE_BD;
			EPC_REGISTER = PROGRAM_COUNTER - 4;
		} else {
			EPC_REGISTER = PROGRAM_COUNTER;
		}
		if (AddressDefined(BadVaddr)) {
			PROGRAM_COUNTER = 0x80000180;
		} else {
			PROGRAM_COUNTER = 0x80000000;
		}
		STATUS_REGISTER |= STATUS_EXL;
	} else {
		PROGRAM_COUNTER = 0x80000180;
	}
}

void DoSysCallException ( uint32_t DelaySlot) {
	CAUSE_REGISTER = EXC_SYSCALL;
	if (DelaySlot) {
		CAUSE_REGISTER |= CAUSE_BD;
		EPC_REGISTER = PROGRAM_COUNTER - 4;
	} else {
		EPC_REGISTER = PROGRAM_COUNTER;
	}
	STATUS_REGISTER |= STATUS_EXL;
	PROGRAM_COUNTER = 0x80000180;
}
