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

#include <stdint.h>
#include "main.h"
#include "cpu.h"
#include "usf.h"
#include "audio.h"
#include "audio_hle.h"
#include "recompiler_cpu.h"
#include "x86.h"
#include "registers.h"
#include "rsp.h"

#include <audacious/plugin.h>

#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
extern InputPlayback * pcontext;
extern GThread * decode_thread;

uint32_t NextInstruction = 0, JumpToLocation = 0, AudioIntrReg = 0;
CPU_ACTION * CPU_Action = 0;
SYSTEM_TIMERS * Timers = 0;
OPCODE Opcode;
uint32_t CPURunning = 0, SPHack = 0;
uint32_t * WaitMode = 0, CPU_Type = CPU_Recompiler;


void ChangeCompareTimer(void) {
	uint32_t NextCompare = COMPARE_REGISTER - COUNT_REGISTER;
	if ((NextCompare & 0x80000000) != 0) {  NextCompare = 0x7FFFFFFF; }
	if (NextCompare == 0) { NextCompare = 0x1; }
	ChangeTimer(CompareTimer,NextCompare);
}

void ChangeTimer(int32_t Type, int32_t Value) {
	if (Value == 0) {
		Timers->NextTimer[Type] = 0;
		Timers->Active[Type] = 0;
		return;
	}
	Timers->NextTimer[Type] = Value - Timers->Timer;
	Timers->Active[Type] = 1;
	CheckTimer();
}

void CheckTimer (void) {
	int32_t count;

	for (count = 0; count < MaxTimers; count++) {
		if (!Timers->Active[count]) { continue; }
		if (!(count == CompareTimer && Timers->NextTimer[count] == 0x7FFFFFFF)) {
			Timers->NextTimer[count] += Timers->Timer;
		}
	}
	Timers->CurrentTimerType = -1;
	Timers->Timer = 0x7FFFFFFF;
	for (count = 0; count < MaxTimers; count++) {
		if (!Timers->Active[count]) { continue; }
		if (Timers->NextTimer[count] >= Timers->Timer) { continue; }
		Timers->Timer = Timers->NextTimer[count];
		Timers->CurrentTimerType = count;
	}
	if (Timers->CurrentTimerType == -1) {
		DisplayError("No active timers ???\nEmulation Stoped");
		StopEmulation();
	}
	for (count = 0; count < MaxTimers; count++) {
		if (!Timers->Active[count]) { continue; }
		if (!(count == CompareTimer && Timers->NextTimer[count] == 0x7FFFFFFF)) {
			Timers->NextTimer[count] -= Timers->Timer;
		}
	}

	if (Timers->NextTimer[CompareTimer] == 0x7FFFFFFF) {
		uint32_t NextCompare = COMPARE_REGISTER - COUNT_REGISTER;
		if ((NextCompare & 0x80000000) == 0 && NextCompare != 0x7FFFFFFF) {
			ChangeCompareTimer();
		}
	}
}



void CloseCpu (void) {
	uint32_t count = 0;

	if(!MemChunk) return;
	if (!cpu_running) { return; }

	cpu_running = 0;
	g_print("cpu_running = %d\n", cpu_running);

	for (count = 0; count < 3; count ++ ) {
		CPU_Action->CloseCPU = 1;
		CPU_Action->DoSomething = 1;
	}

	CPURunning = 0;
}

int32_t DelaySlotEffectsCompare (uint32_t PC, uint32_t Reg1, uint32_t Reg2) {
	OPCODE Command;

	if (!r4300i_LW_VAddr(PC + 4, (uint32_t*)&Command.Hex)) {
		return 1;
	}

	switch (Command.op) {
	case R4300i_SPECIAL:
		switch (Command.funct) {
		case R4300i_SPECIAL_SLL:
		case R4300i_SPECIAL_SRL:
		case R4300i_SPECIAL_SRA:
		case R4300i_SPECIAL_SLLV:
		case R4300i_SPECIAL_SRLV:
		case R4300i_SPECIAL_SRAV:
		case R4300i_SPECIAL_MFHI:
		case R4300i_SPECIAL_MTHI:
		case R4300i_SPECIAL_MFLO:
		case R4300i_SPECIAL_MTLO:
		case R4300i_SPECIAL_DSLLV:
		case R4300i_SPECIAL_DSRLV:
		case R4300i_SPECIAL_DSRAV:
		case R4300i_SPECIAL_ADD:
		case R4300i_SPECIAL_ADDU:
		case R4300i_SPECIAL_SUB:
		case R4300i_SPECIAL_SUBU:
		case R4300i_SPECIAL_AND:
		case R4300i_SPECIAL_OR:
		case R4300i_SPECIAL_XOR:
		case R4300i_SPECIAL_NOR:
		case R4300i_SPECIAL_SLT:
		case R4300i_SPECIAL_SLTU:
		case R4300i_SPECIAL_DADD:
		case R4300i_SPECIAL_DADDU:
		case R4300i_SPECIAL_DSUB:
		case R4300i_SPECIAL_DSUBU:
		case R4300i_SPECIAL_DSLL:
		case R4300i_SPECIAL_DSRL:
		case R4300i_SPECIAL_DSRA:
		case R4300i_SPECIAL_DSLL32:
		case R4300i_SPECIAL_DSRL32:
		case R4300i_SPECIAL_DSRA32:
			if (Command.rd == 0) { return 0; }
			if (Command.rd == Reg1) { return 1; }
			if (Command.rd == Reg2) { return 1; }
			break;
		case R4300i_SPECIAL_MULT:
		case R4300i_SPECIAL_MULTU:
		case R4300i_SPECIAL_DIV:
		case R4300i_SPECIAL_DIVU:
		case R4300i_SPECIAL_DMULT:
		case R4300i_SPECIAL_DMULTU:
		case R4300i_SPECIAL_DDIV:
		case R4300i_SPECIAL_DDIVU:
			break;
		default:
			return 1;
		}
		break;
	case R4300i_CP0:
		switch (Command.rs) {
		case R4300i_COP0_MT: break;
		case R4300i_COP0_MF:
			if (Command.rt == 0) { return 0; }
			if (Command.rt == Reg1) { return 1; }
			if (Command.rt == Reg2) { return 1; }
			break;
		default:
			if ( (Command.rs & 0x10 ) != 0 ) {
				switch( Opcode.funct ) {
				case R4300i_COP0_CO_TLBR: break;
				case R4300i_COP0_CO_TLBWI: break;
				case R4300i_COP0_CO_TLBWR: break;
				case R4300i_COP0_CO_TLBP: break;
				default:
					return 1;
				}
				return 1;
			}
		}
		break;
	case R4300i_CP1:
		switch (Command.fmt) {
		case R4300i_COP1_MF:
			if (Command.rt == 0) { return 0; }
			if (Command.rt == Reg1) { return 1; }
			if (Command.rt == Reg2) { return 1; }
			break;
		case R4300i_COP1_CF: break;
		case R4300i_COP1_MT: break;
		case R4300i_COP1_CT: break;
		case R4300i_COP1_S: break;
		case R4300i_COP1_D: break;
		case R4300i_COP1_W: break;
		case R4300i_COP1_L: break;
			return 1;
		}
		break;
	case R4300i_ANDI:
	case R4300i_ORI:
	case R4300i_XORI:
	case R4300i_LUI:
	case R4300i_ADDI:
	case R4300i_ADDIU:
	case R4300i_SLTI:
	case R4300i_SLTIU:
	case R4300i_DADDI:
	case R4300i_DADDIU:
	case R4300i_LB:
	case R4300i_LH:
	case R4300i_LW:
	case R4300i_LWL:
	case R4300i_LWR:
	case R4300i_LDL:
	case R4300i_LDR:
	case R4300i_LBU:
	case R4300i_LHU:
	case R4300i_LD:
	case R4300i_LWC1:
	case R4300i_LDC1:
		if (Command.rt == 0) { return 0; }
		if (Command.rt == Reg1) { return 1; }
		if (Command.rt == Reg2) { return 1; }
		break;
	case R4300i_CACHE: break;
	case R4300i_SB: break;
	case R4300i_SH: break;
	case R4300i_SW: break;
	case R4300i_SWR: break;
	case R4300i_SWL: break;
	case R4300i_SWC1: break;
	case R4300i_SDC1: break;
	case R4300i_SD: break;
	default:

		return 1;
	}
	return 0;
}

int32_t DelaySlotEffectsJump (uint32_t JumpPC) {
	OPCODE Command;

	if (!r4300i_LW_VAddr(JumpPC, &Command.Hex)) { return 1; }

	switch (Command.op) {
	case R4300i_SPECIAL:
		switch (Command.funct) {
		case R4300i_SPECIAL_JR:	return DelaySlotEffectsCompare(JumpPC,Command.rs,0);
		case R4300i_SPECIAL_JALR: return DelaySlotEffectsCompare(JumpPC,Command.rs,31);
		}
		break;
	case R4300i_REGIMM:
		switch (Command.rt) {
		case R4300i_REGIMM_BLTZ:
		case R4300i_REGIMM_BGEZ:
		case R4300i_REGIMM_BLTZL:
		case R4300i_REGIMM_BGEZL:
		case R4300i_REGIMM_BLTZAL:
		case R4300i_REGIMM_BGEZAL:
			return DelaySlotEffectsCompare(JumpPC,Command.rs,0);
		}
		break;
	case R4300i_JAL:
	case R4300i_SPECIAL_JALR: return DelaySlotEffectsCompare(JumpPC,31,0); break;
	case R4300i_J: return 0;
	case R4300i_BEQ:
	case R4300i_BNE:
	case R4300i_BLEZ:
	case R4300i_BGTZ:
		return DelaySlotEffectsCompare(JumpPC,Command.rs,Command.rt);
	case R4300i_CP1:
		switch (Command.fmt) {
		case R4300i_COP1_BC:
			switch (Command.ft) {
			case R4300i_COP1_BC_BCF:
			case R4300i_COP1_BC_BCT:
			case R4300i_COP1_BC_BCFL:
			case R4300i_COP1_BC_BCTL:
				{
					int32_t EffectDelaySlot;
					OPCODE NewCommand;

					if (!r4300i_LW_VAddr(JumpPC + 4, &NewCommand.Hex)) { return 1; }

					EffectDelaySlot = 0;
					if (NewCommand.op == R4300i_CP1) {
						if (NewCommand.fmt == R4300i_COP1_S && (NewCommand.funct & 0x30) == 0x30 ) {
							EffectDelaySlot = 1;
						}
						if (NewCommand.fmt == R4300i_COP1_D && (NewCommand.funct & 0x30) == 0x30 ) {
							EffectDelaySlot = 1;
						}
					}
					return EffectDelaySlot;
				}
				break;
			}
			break;
		}
		break;
	case R4300i_BEQL:
	case R4300i_BNEL:
	case R4300i_BLEZL:
	case R4300i_BGTZL:
		return DelaySlotEffectsCompare(JumpPC,Command.rs,Command.rt);
	}
	return 1;
}

void DoSomething ( void ) {
	if (CPU_Action->CloseCPU) {
		//StopEmulation();
		cpu_running = 0;
		//printf("Stopping?\n");
		if(!(fake_seek_stopping&3))
			g_thread_exit(NULL);
	}
	if (CPU_Action->CheckInterrupts) {
		CPU_Action->CheckInterrupts = 0;
		CheckInterrupts();
	}
	if (CPU_Action->DoInterrupt) {
		CPU_Action->DoInterrupt = 0;
		DoIntrException(0);
	}


	CPU_Action->DoSomething = 0;

	if (CPU_Action->DoInterrupt) { CPU_Action->DoSomething = 1; }
}

void InPermLoop (void) {
	// *** Changed ***/
	if (CPU_Action->DoInterrupt) { return; }

	/* Interrupts enabled */
	if (( STATUS_REGISTER & STATUS_IE  ) == 0 ) { goto InterruptsDisabled; }
	if (( STATUS_REGISTER & STATUS_EXL ) != 0 ) { goto InterruptsDisabled; }
	if (( STATUS_REGISTER & STATUS_ERL ) != 0 ) { goto InterruptsDisabled; }
	if (( STATUS_REGISTER & 0xFF00) == 0) { goto InterruptsDisabled; }

	/* check sound playing */

	/* check RSP running */
	/* check RDP running */
	if (Timers->Timer >= 0) {
		COUNT_REGISTER += Timers->Timer + 1;
		Timers->Timer = -1;
	}
	return;

InterruptsDisabled:
	DisplayError("Stuck in Permanent Loop");
	StopEmulation();
}

void ReadFromMem(const void * source, void * target, uint32_t length, uint32_t *offset) {
	memcpy((uint8_t*)target,((uint8_t*)source)+*offset,length);
	*offset+=length;
}


uint32_t Machine_LoadStateFromRAM(void * savestatespace) {
	uint8_t LoadHeader[0x40];
	uint32_t Value, count, SaveRDRAMSize, offset=0;

	ReadFromMem( savestatespace,&Value,sizeof(Value),&offset);
	if (Value != 0x23D8A6C8) { return 0; }
	ReadFromMem( savestatespace,&SaveRDRAMSize,sizeof(SaveRDRAMSize),&offset);
	ReadFromMem( savestatespace,&LoadHeader,0x40,&offset);

	if (CPU_Type != CPU_Interpreter) {
		ResetRecompCode();
	}

	Timers->CurrentTimerType = -1;
	Timers->Timer = 0;
	for (count = 0; count < MaxTimers; count ++) { Timers->Active[count] = 0; }

	//fix rdram size
	if (SaveRDRAMSize != RdramSize) {
		// dothis :)
	}		

	RdramSize = SaveRDRAMSize;

	ReadFromMem( savestatespace,&Value,sizeof(Value),&offset);
	ChangeTimer(ViTimer,Value);
	ReadFromMem( savestatespace,&PROGRAM_COUNTER,sizeof(PROGRAM_COUNTER),&offset);
	ReadFromMem( savestatespace,GPR,sizeof(int64_t)*32,&offset);
	ReadFromMem( savestatespace,FPR,sizeof(int64_t)*32,&offset);
	ReadFromMem( savestatespace,CP0,sizeof(uint32_t)*32,&offset);
	ReadFromMem( savestatespace,FPCR,sizeof(uint32_t)*32,&offset);
	ReadFromMem( savestatespace,&HI,sizeof(int64_t),&offset);
	ReadFromMem( savestatespace,&LO,sizeof(int64_t),&offset);
	ReadFromMem( savestatespace,RegRDRAM,sizeof(uint32_t)*10,&offset);
	ReadFromMem( savestatespace,RegSP,sizeof(uint32_t)*10,&offset);
	ReadFromMem( savestatespace,RegDPC,sizeof(uint32_t)*10,&offset);
	ReadFromMem( savestatespace,RegMI,sizeof(uint32_t)*4,&offset);
	ReadFromMem( savestatespace,RegVI,sizeof(uint32_t)*14,&offset);
	ReadFromMem( savestatespace,RegAI,sizeof(uint32_t)*6,&offset);
	ReadFromMem( savestatespace,RegPI,sizeof(uint32_t)*13,&offset);
	ReadFromMem( savestatespace,RegRI,sizeof(uint32_t)*8,&offset);
	ReadFromMem( savestatespace,RegSI,sizeof(uint32_t)*4,&offset);
	ReadFromMem( savestatespace,tlb,sizeof(TLB)*32,&offset);
	ReadFromMem( savestatespace,(uint8_t*)PIF_Ram,0x40,&offset);
	ReadFromMem( savestatespace,RDRAM,SaveRDRAMSize,&offset);
	ReadFromMem( savestatespace,DMEM,0x1000,&offset);
	ReadFromMem( savestatespace,IMEM,0x1000,&offset);

	CP0[32] = 0;

	SetupTLB();
	ChangeCompareTimer();
	AI_STATUS_REG = 0;
	AiDacrateChanged(AI_DACRATE_REG);

//	StartAiInterrupt();

	SetFpuLocations(); // important if FR=1

	return 1;
}
extern int32_t RSP_Cpu;
extern int32_t SampleRate;
void StartEmulationFromSave ( void * savestate ) {
	uint32_t count = 0;
	if(use_interpreter)
		CPU_Type = CPU_Interpreter;
	
	//printf("Starting generic Cpu\n");

	//CloseCpu();
	memset(N64MEM, 0, RdramSize);	
	
	memset(DMEM, 0, 0x1000);
	memset(IMEM, 0, 0x1000);
	memset(TLB_Map, 0, 0x100000 * sizeof(uintptr_t) + 0x10000);
	if(!use_interpreter) {
		memset(JumpTable, 0, 0x200000 * sizeof(uintptr_t));
		memset(RecompCode, 0xcc, NormalCompileBufferSize);	// fill with Breakpoints
		memset(DelaySlotTable, 0, ((0x1000000) >> 0xA));
	}

	memset(CPU_Action,0,sizeof(CPU_Action));
	WrittenToRom = 0;

	InitilizeTLB();

	SetupRegisters(Registers);

	BuildInterpreter();
	RecompPos = RecompCode;

	Timers->CurrentTimerType = -1;
	Timers->Timer = 0;

	for (count = 0; count < MaxTimers; count ++) { Timers->Active[count] = 0; }
	ChangeTimer(ViTimer,5000);
	ChangeCompareTimer();
	ViFieldNumber = 0;
	CPURunning = 1;
	*WaitMode = 0;

	init_rsp();

	Machine_LoadStateFromRAM(savestate);

	SampleRate = 48681812 / (AI_DACRATE_REG + 1);

	OpenSound();
	
	pcontext->set_params(pcontext, SampleRate * 4, SampleRate, 2);	

	if(enableFIFOfull) {
		const float VSyncTiming = 789000.0f;
		double BytesPerSecond = 48681812.0 / (AI_DACRATE_REG + 1) * 4;
		double CountsPerSecond = (double)(((double)VSyncTiming) * (double)60.0);
		double CountsPerByte = (double)CountsPerSecond / (double)BytesPerSecond;
		uint32_t IntScheduled = (uint32_t)((double)AI_LEN_REG * CountsPerByte);

		ChangeTimer(AiTimer,IntScheduled);
		AI_STATUS_REG|=0x40000000;
	}

    cpu_stopped = 0;
	cpu_running = 1;
	fake_seek_stopping = 0;

	switch (CPU_Type) {
		case CPU_Interpreter: StartInterpreterCPU(); break;
		case CPU_Recompiler: StartRecompilerCPU(); break;
	default:
		DisplayError("Unhandled CPU %d",CPU_Type);
	}

}


void RefreshScreen (void ){
	ChangeTimer(ViTimer, 300000);

}

void RunRsp (void) {
	if ( ( SP_STATUS_REG & SP_STATUS_HALT ) == 0) {
		if ( ( SP_STATUS_REG & SP_STATUS_BROKE ) == 0 ) {

			uint32_t Task = *( uint32_t *)(DMEM + 0xFC0);

			switch (Task) {
			case 1:	{
					MI_INTR_REG |= 0x20;

					SP_STATUS_REG |= (0x0203 );
					if ((SP_STATUS_REG & SP_STATUS_INTR_BREAK) != 0 )
						MI_INTR_REG |= 1;

					CheckInterrupts();

					DPC_STATUS_REG &= ~0x0002;
					return;

				}
				break;
			case 2: {

					if(use_audiohle && !is_seeking) {
						OSTask_t *task = (OSTask_t*)(DMEM + 0xFC0);
						if(audio_ucode(task))
							break;

					} else
						break;

					SP_STATUS_REG |= (0x0203 );
					if ((SP_STATUS_REG & SP_STATUS_INTR_BREAK) != 0 ) {
						MI_INTR_REG |= 1;
						CheckInterrupts();
					}

                    return;

				}
				break;
			default:

				break;
			}

			if(!is_seeking)
				real_run_rsp(100);
			SP_STATUS_REG |= (0x0203 );
			if ((SP_STATUS_REG & SP_STATUS_INTR_BREAK) != 0 ) {
				MI_INTR_REG |= 1;
				CheckInterrupts();
			}

		}
	}
}

void TimerDone (void) {
	switch (Timers->CurrentTimerType) {
	case CompareTimer:
		if(enablecompare)
			FAKE_CAUSE_REGISTER |= CAUSE_IP7;
		//CheckInterrupts();
		ChangeCompareTimer();
		break;
	case ViTimer:
		RefreshScreen();
		MI_INTR_REG |= MI_INTR_VI;
		CheckInterrupts();
		//CompileCheckInterrupts();
		*WaitMode=0;
		break;
	case AiTimer:
		ChangeTimer(AiTimer,0);
		AI_STATUS_REG=0;
        AudioIntrReg|=4;
		//CheckInterrupts();
		break;
	}
	CheckTimer();
}

void Int3() {
	asm("int $3");
}

void _Emms() {
	asm("emms");
}



#include <fpu_control.h>

void controlfp(uint32_t control) {
	uint32_t OldControl = 0;

	_FPU_GETCW(OldControl);
	OldControl &= ~(_FPU_RC_ZERO | _FPU_RC_UP | _FPU_RC_DOWN | _FPU_RC_NEAREST);

	OldControl |= control;

	_FPU_SETCW(OldControl);

}

