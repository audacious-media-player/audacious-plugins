#include <stdlib.h>
#include <stdint.h>
#include "usf.h"
#include "cpu.h"
#include "memory.h"
#include "audio.h"
#include "rsp.h"


void (*RSP_Opcode[64])();
void (*RSP_RegImm[32])();
void (*RSP_Special[64])();
void (*RSP_Cop0[32])();
void (*RSP_Cop2[32])();
void (*RSP_Vector[64])();
void (*RSP_Lc2[32])();
void (*RSP_Sc2[32])();

void RSP_Opcode_SPECIAL ( void ) {
	(RSP_Special[ RSPOpC.funct ])();
}

void RSP_Opcode_REGIMM ( void ) {
	(RSP_RegImm[ RSPOpC.rt ])();
}

void RSP_Opcode_J ( void ) {
	RSP_NextInstruction = DELAY_SLOT;
	RSP_JumpTo = (RSPOpC.target << 2) & 0xFFC;
}

void RSP_Opcode_JAL ( void ) {
	RSP_NextInstruction = DELAY_SLOT;
	RSP_GPR[31].UW = ( *PrgCount + 8 ) & 0xFFC;
	RSP_JumpTo = (RSPOpC.target << 2) & 0xFFC;
}

void RSP_Opcode_BEQ ( void ) {
	RSP_NextInstruction = DELAY_SLOT;
	if (RSP_GPR[RSPOpC.rs].W == RSP_GPR[RSPOpC.rt].W) {
		RSP_JumpTo = ( *PrgCount + ((int16_t)RSPOpC.offset << 2) + 4 ) & 0xFFC;
	} else  {
		RSP_JumpTo = ( *PrgCount + 8 ) & 0xFFC;
	}
}

void RSP_Opcode_BNE ( void ) {
	RSP_NextInstruction = DELAY_SLOT;
	if (RSP_GPR[RSPOpC.rs].W != RSP_GPR[RSPOpC.rt].W) {
		RSP_JumpTo = ( *PrgCount + ((int16_t)RSPOpC.offset << 2) + 4 ) & 0xFFC;
	} else  {
		RSP_JumpTo = ( *PrgCount + 8 ) & 0xFFC;
	}
}

void RSP_Opcode_BLEZ ( void ) {
	RSP_NextInstruction = DELAY_SLOT;
	if (RSP_GPR[RSPOpC.rs].W <= 0) {
		RSP_JumpTo = ( *PrgCount + ((int16_t)RSPOpC.offset << 2) + 4 ) & 0xFFC;
	} else  {
		RSP_JumpTo = ( *PrgCount + 8 ) & 0xFFC;
	}
}

void RSP_Opcode_BGTZ ( void ) {
	RSP_NextInstruction = DELAY_SLOT;
	if (RSP_GPR[RSPOpC.rs].W > 0) {
		RSP_JumpTo = ( *PrgCount + ((int16_t)RSPOpC.offset << 2) + 4 ) & 0xFFC;
	} else  {
		RSP_JumpTo = ( *PrgCount + 8 ) & 0xFFC;
	}
}

void RSP_Opcode_ADDI ( void ) {
	if (RSPOpC.rt != 0) {
		RSP_GPR[RSPOpC.rt].W = RSP_GPR[RSPOpC.rs].W + (int16_t)RSPOpC.immediate;
	}
}

void RSP_Opcode_ADDIU ( void ) {
	if (RSPOpC.rt != 0) {
		RSP_GPR[RSPOpC.rt].UW = RSP_GPR[RSPOpC.rs].UW + (uint32_t)((int16_t)RSPOpC.immediate);
	}
}

void RSP_Opcode_SLTI (void) {
	if (RSPOpC.rt == 0) { return; }
	if (RSP_GPR[RSPOpC.rs].W < (int16_t)RSPOpC.immediate) {
		RSP_GPR[RSPOpC.rt].W = 1;
	} else {
		RSP_GPR[RSPOpC.rt].W = 0;
	}
}

void RSP_Opcode_SLTIU (void) {
	if (RSPOpC.rt == 0) { return; }
	if (RSP_GPR[RSPOpC.rs].UW < (uint32_t)(int16_t)RSPOpC.immediate) {
		RSP_GPR[RSPOpC.rt].W = 1;
	} else {
		RSP_GPR[RSPOpC.rt].W = 0;
	}
}

void RSP_Opcode_ANDI ( void ) {
	if (RSPOpC.rt != 0) {
		RSP_GPR[RSPOpC.rt].W = RSP_GPR[RSPOpC.rs].W & RSPOpC.immediate;
	}
}

void RSP_Opcode_ORI ( void ) {
	if (RSPOpC.rt != 0) {
		RSP_GPR[RSPOpC.rt].W = RSP_GPR[RSPOpC.rs].W | RSPOpC.immediate;
	}
}

void RSP_Opcode_XORI ( void ) {
	if (RSPOpC.rt != 0) {
		RSP_GPR[RSPOpC.rt].W = RSP_GPR[RSPOpC.rs].W ^ RSPOpC.immediate;
	}
}

void RSP_Opcode_LUI (void) {
	if (RSPOpC.rt != 0) {
		RSP_GPR[RSPOpC.rt].W = (int16_t)RSPOpC.offset << 16;
	}
}

void RSP_Opcode_COP0 (void) {
	(RSP_Cop0[ RSPOpC.rs ])();
}

void RSP_Opcode_COP2 (void) {
	(RSP_Cop2[ RSPOpC.rs ])();
}

void RSP_Opcode_LB ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (int16_t)RSPOpC.offset) & 0xFFF);
	RSP_LB_DMEM( Address, &RSP_GPR[RSPOpC.rt].UB[0] );
	RSP_GPR[RSPOpC.rt].W = RSP_GPR[RSPOpC.rt].B[0];
}

void RSP_Opcode_LH ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (int16_t)RSPOpC.offset) & 0xFFF);
	RSP_LH_DMEM( Address, &RSP_GPR[RSPOpC.rt].UHW[0] );
	RSP_GPR[RSPOpC.rt].W = RSP_GPR[RSPOpC.rt].HW[0];
}

void RSP_Opcode_LW ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (int16_t)RSPOpC.offset) & 0xFFF);
	RSP_LW_DMEM( Address, &RSP_GPR[RSPOpC.rt].UW );
}

void RSP_Opcode_LBU ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (int16_t)RSPOpC.offset) & 0xFFF);
	RSP_LB_DMEM( Address, &RSP_GPR[RSPOpC.rt].UB[0] );
	RSP_GPR[RSPOpC.rt].UW = RSP_GPR[RSPOpC.rt].UB[0];
}

void RSP_Opcode_LHU ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (int16_t)RSPOpC.offset) & 0xFFF);
	RSP_LH_DMEM( Address, &RSP_GPR[RSPOpC.rt].UHW[0] );
	RSP_GPR[RSPOpC.rt].UW = RSP_GPR[RSPOpC.rt].UHW[0];
}

void RSP_Opcode_SB ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (int16_t)RSPOpC.offset) & 0xFFF);
	RSP_SB_DMEM( Address, RSP_GPR[RSPOpC.rt].UB[0] );
}

void RSP_Opcode_SH ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (int16_t)RSPOpC.offset) & 0xFFF);
	//if(Address == 0x370) {
	//	printf("%08x\nOpcode: %08x\n%08x\nRA: %08x\n", RSPCompilePC, RSPOpC.Hex, (int16_t)RSPOpC.offset,RSP_GPR[31].UW);
	//}

	RSP_SH_DMEM( Address, RSP_GPR[RSPOpC.rt].UHW[0] );
}

void RSP_Opcode_SW ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (int16_t)RSPOpC.offset) & 0xFFF);
	RSP_SW_DMEM( Address, RSP_GPR[RSPOpC.rt].UW );
}

void RSP_Opcode_LC2 (void) {
	(RSP_Lc2 [ RSPOpC.rd ])();
}

void RSP_Opcode_SC2 (void) {
	(RSP_Sc2 [ RSPOpC.rd ])();
}
/********************** R4300i OpCodes: Special **********************/
void RSP_Special_SLL ( void ) {
	if (RSPOpC.rd != 0) {
		RSP_GPR[RSPOpC.rd].W = RSP_GPR[RSPOpC.rt].W << RSPOpC.sa;
	}
}

void RSP_Special_SRL ( void ) {
	if (RSPOpC.rd != 0) {
		RSP_GPR[RSPOpC.rd].UW = RSP_GPR[RSPOpC.rt].UW >> RSPOpC.sa;
	}
}

void RSP_Special_SRA ( void ) {
	if (RSPOpC.rd != 0) {
		RSP_GPR[RSPOpC.rd].W = RSP_GPR[RSPOpC.rt].W >> RSPOpC.sa;
	}
}

void RSP_Special_SLLV (void) {
	if (RSPOpC.rd != 0) {
		RSP_GPR[RSPOpC.rd].W = RSP_GPR[RSPOpC.rt].W << (RSP_GPR[RSPOpC.rs].W & 0x1F);
	}
}

void RSP_Special_SRLV (void) {
	if (RSPOpC.rd != 0) {
		RSP_GPR[RSPOpC.rd].UW = RSP_GPR[RSPOpC.rt].UW >> (RSP_GPR[RSPOpC.rs].W & 0x1F);
	}
}

void RSP_Special_SRAV (void) {
	if (RSPOpC.rd != 0) {
		RSP_GPR[RSPOpC.rd].W = RSP_GPR[RSPOpC.rt].W >> (RSP_GPR[RSPOpC.rs].W & 0x1F);
	}
}

void RSP_Special_JR (void) {
	RSP_NextInstruction = DELAY_SLOT;
	RSP_JumpTo = (RSP_GPR[RSPOpC.rs].W & 0xFFC);
}

void RSP_Special_JALR (void) {
	RSP_NextInstruction = DELAY_SLOT;
	RSP_GPR[RSPOpC.rd].W = (*PrgCount + 8) & 0xFFC;
	RSP_JumpTo = (RSP_GPR[RSPOpC.rs].W & 0xFFC);
}

#define R4300i_SP_Intr			0x1

void RSP_Special_BREAK ( void ) {
	RSP_Running = 0;
	SP_STATUS_REG |= (SP_STATUS_HALT | SP_STATUS_BROKE );
	if ((SP_STATUS_REG & SP_STATUS_INTR_BREAK) != 0 ) {
		MI_INTR_REG |= R4300i_SP_Intr;
		CheckInterrupts();
	}
}

void RSP_Special_ADD (void) {
	if (RSPOpC.rd != 0) {
		RSP_GPR[RSPOpC.rd].W = RSP_GPR[RSPOpC.rs].W + RSP_GPR[RSPOpC.rt].W;
	}
}

void RSP_Special_ADDU (void) {
	if (RSPOpC.rd != 0) {
		RSP_GPR[RSPOpC.rd].UW = RSP_GPR[RSPOpC.rs].UW + RSP_GPR[RSPOpC.rt].UW;
	}
}

void RSP_Special_SUB (void) {
	if (RSPOpC.rd != 0) {
		RSP_GPR[RSPOpC.rd].W = RSP_GPR[RSPOpC.rs].W - RSP_GPR[RSPOpC.rt].W;
	}
}

void RSP_Special_SUBU (void) {
	if (RSPOpC.rd != 0) {
		RSP_GPR[RSPOpC.rd].UW = RSP_GPR[RSPOpC.rs].UW - RSP_GPR[RSPOpC.rt].UW;
	}
}

void RSP_Special_AND (void) {
	if (RSPOpC.rd != 0) {
		RSP_GPR[RSPOpC.rd].UW = RSP_GPR[RSPOpC.rs].UW & RSP_GPR[RSPOpC.rt].UW;
	}
}

void RSP_Special_OR (void) {
	if (RSPOpC.rd != 0) {
		RSP_GPR[RSPOpC.rd].UW = RSP_GPR[RSPOpC.rs].UW | RSP_GPR[RSPOpC.rt].UW;
	}
}

void RSP_Special_XOR (void) {
	if (RSPOpC.rd != 0) {
		RSP_GPR[RSPOpC.rd].UW = RSP_GPR[RSPOpC.rs].UW ^ RSP_GPR[RSPOpC.rt].UW;
	}
}

void RSP_Special_NOR (void) {
	if (RSPOpC.rd != 0) {
		RSP_GPR[RSPOpC.rd].UW = ~(RSP_GPR[RSPOpC.rs].UW | RSP_GPR[RSPOpC.rt].UW);
	}
}

void RSP_Special_SLT (void) {
	if (RSPOpC.rd == 0) { return; }
	if (RSP_GPR[RSPOpC.rs].W < RSP_GPR[RSPOpC.rt].W) {
		RSP_GPR[RSPOpC.rd].UW = 1;
	} else {
		RSP_GPR[RSPOpC.rd].UW = 0;
	}
}

void RSP_Special_SLTU (void) {
	if (RSPOpC.rd == 0) { return; }
	if (RSP_GPR[RSPOpC.rs].UW < RSP_GPR[RSPOpC.rt].UW) {
		RSP_GPR[RSPOpC.rd].UW = 1;
	} else {
		RSP_GPR[RSPOpC.rd].UW = 0;
	}
}
/********************** R4300i OpCodes: RegImm **********************/
void RSP_Opcode_BLTZ ( void ) {
	RSP_NextInstruction = DELAY_SLOT;
	if (RSP_GPR[RSPOpC.rs].W < 0) {
		RSP_JumpTo = ( *PrgCount + ((int16_t)RSPOpC.offset << 2) + 4 ) & 0xFFC;
	} else  {
		RSP_JumpTo = ( *PrgCount + 8 ) & 0xFFC;
	}
}

void RSP_Opcode_BGEZ ( void ) {
	RSP_NextInstruction = DELAY_SLOT;
	if (RSP_GPR[RSPOpC.rs].W >= 0) {
		RSP_JumpTo = ( *PrgCount + ((int16_t)RSPOpC.offset << 2) + 4 ) & 0xFFC;
	} else  {
		RSP_JumpTo = ( *PrgCount + 8 ) & 0xFFC;
	}
}

void RSP_Opcode_BLTZAL ( void ) {
	RSP_NextInstruction = DELAY_SLOT;
	RSP_GPR[31].UW = ( *PrgCount + 8 ) & 0xFFC;
	if (RSP_GPR[RSPOpC.rs].W < 0) {
		RSP_JumpTo = ( *PrgCount + ((int16_t)RSPOpC.offset << 2) + 4 ) & 0xFFC;
	} else  {
		RSP_JumpTo = ( *PrgCount + 8 ) & 0xFFC;
	}
}

void RSP_Opcode_BGEZAL ( void ) {
	RSP_NextInstruction = DELAY_SLOT;
	RSP_GPR[31].UW = ( *PrgCount + 8 ) & 0xFFC;
	if (RSP_GPR[RSPOpC.rs].W >= 0) {
		RSP_JumpTo = ( *PrgCount + ((int16_t)RSPOpC.offset << 2) + 4 ) & 0xFFC;
	} else  {
		RSP_JumpTo = ( *PrgCount + 8 ) & 0xFFC;
	}
}
/************************** Cop0 functions *************************/
void RSP_Cop0_MF (void) {
	switch (RSPOpC.rd) {
	case 4: RSP_GPR[RSPOpC.rt].UW = SP_STATUS_REG; break;
	case 5: RSP_GPR[RSPOpC.rt].UW = SP_DMA_FULL_REG; break;
	case 6: RSP_GPR[RSPOpC.rt].UW = SP_DMA_BUSY_REG; break;
	case 7:
		RSP_GPR[RSPOpC.rt].W = 0;
		//RSP_GPR[RSPOpC.rt].W = SP_SEMAPHORE_REG;
		//SP_SEMAPHORE_REG = 1;
		break;
	case 8: RSP_GPR[RSPOpC.rt].UW = DPC_START_REG ; break;
	case 9: RSP_GPR[RSPOpC.rt].UW = DPC_END_REG ; break;
	case 10: RSP_GPR[RSPOpC.rt].UW = DPC_CURRENT_REG; break;
	case 11: RSP_GPR[RSPOpC.rt].W = DPC_STATUS_REG; break;
	case 12: RSP_GPR[RSPOpC.rt].W = DPC_CLOCK_REG; break;
	default:
		printf("have not implemented RSP MF cpu->CP0 reg (%d)",RSPOpC.rd);
	}
}

void RSP_Cop0_MT (void) {
	switch (RSPOpC.rd) {
	case 0: SP_MEM_ADDR_REG  = RSP_GPR[RSPOpC.rt].UW; break;
	case 1: SP_DRAM_ADDR_REG = RSP_GPR[RSPOpC.rt].UW; break;
	case 2:
		SP_RD_LEN_REG = RSP_GPR[RSPOpC.rt].UW;
		RSP_SP_DMA_READ();
		break;
	case 3:
		SP_WR_LEN_REG = RSP_GPR[RSPOpC.rt].UW;
		RSP_SP_DMA_WRITE();
		break;
	case 4:
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_CLR_HALT ) != 0) { SP_STATUS_REG &= ~SP_STATUS_HALT; }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_SET_HALT ) != 0) { SP_STATUS_REG |= SP_STATUS_HALT;  }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_CLR_BROKE ) != 0) { SP_STATUS_REG &= ~SP_STATUS_BROKE; }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_CLR_INTR ) != 0) { MI_INTR_REG &= ~R4300i_SP_Intr; }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_SET_INTR ) != 0) { printf("SP_SET_INTR");  }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_CLR_SSTEP ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SSTEP; }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_SET_SSTEP ) != 0) { SP_STATUS_REG |= SP_STATUS_SSTEP;  }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_CLR_INTR_BREAK ) != 0) { SP_STATUS_REG &= ~SP_STATUS_INTR_BREAK; }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_SET_INTR_BREAK ) != 0) { SP_STATUS_REG |= SP_STATUS_INTR_BREAK;  }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_CLR_SIG0 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG0; }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_SET_SIG0 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG0;  }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_CLR_SIG1 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG1; }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_SET_SIG1 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG1;  }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_CLR_SIG2 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG2; }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_SET_SIG2 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG2;  }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_CLR_SIG3 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG3; }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_SET_SIG3 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG3;  }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_CLR_SIG4 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG4; }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_SET_SIG4 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG4;  }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_CLR_SIG5 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG5; }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_SET_SIG5 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG5;  }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_CLR_SIG6 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG6; }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_SET_SIG6 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG6;  }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_CLR_SIG7 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG7; }
		if ( ( RSP_GPR[RSPOpC.rt].W & SP_SET_SIG7 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG7;  }
		break;
	case 7: SP_SEMAPHORE_REG = 0; break;
	case 8:
		DPC_START_REG = RSP_GPR[RSPOpC.rt].UW;
		DPC_CURRENT_REG = RSP_GPR[RSPOpC.rt].UW;
		break;
	case 9:
		DPC_END_REG = RSP_GPR[RSPOpC.rt].UW;
		break;
	case 10: DPC_CURRENT_REG = RSP_GPR[RSPOpC.rt].UW; break;
	case 11:
		if ( ( RSP_GPR[RSPOpC.rt].W & DPC_CLR_XBUS_DMEM_DMA ) != 0) { DPC_STATUS_REG &= ~DPC_STATUS_XBUS_DMEM_DMA; }
		if ( ( RSP_GPR[RSPOpC.rt].W & DPC_SET_XBUS_DMEM_DMA ) != 0) { DPC_STATUS_REG |= DPC_STATUS_XBUS_DMEM_DMA;  }
		if ( ( RSP_GPR[RSPOpC.rt].W & DPC_CLR_FREEZE ) != 0) { DPC_STATUS_REG &= ~DPC_STATUS_FREEZE; }
		if ( ( RSP_GPR[RSPOpC.rt].W & DPC_SET_FREEZE ) != 0) { DPC_STATUS_REG |= DPC_STATUS_FREEZE;  }
		if ( ( RSP_GPR[RSPOpC.rt].W & DPC_CLR_FLUSH ) != 0) { DPC_STATUS_REG &= ~DPC_STATUS_FLUSH; }
		if ( ( RSP_GPR[RSPOpC.rt].W & DPC_SET_FLUSH ) != 0) { DPC_STATUS_REG |= DPC_STATUS_FLUSH;  }
		if ( ( RSP_GPR[RSPOpC.rt].W & DPC_CLR_TMEM_CTR ) != 0) { /* printf("RSP: DPC_STATUS_REG: DPC_CLR_TMEM_CTR"); */ }
		if ( ( RSP_GPR[RSPOpC.rt].W & DPC_CLR_PIPE_CTR ) != 0) { printf("RSP: DPC_STATUS_REG: DPC_CLR_PIPE_CTR"); }
		if ( ( RSP_GPR[RSPOpC.rt].W & DPC_CLR_CMD_CTR ) != 0) { printf("RSP: DPC_STATUS_REG: DPC_CLR_CMD_CTR"); }
		if ( ( RSP_GPR[RSPOpC.rt].W & DPC_CLR_CLOCK_CTR ) != 0) { /* printf("RSP: DPC_STATUS_REG: DPC_CLR_CLOCK_CTR"); */ }
		break;
	default:
		printf("have not implemented RSP MT cpu->CP0 reg (%d)",RSPOpC.rd);
	}
}

/************************** Cop2 functions *************************/
void RSP_Cop2_MF (void) {
	int32_t element = (RSPOpC.sa >> 1);
	RSP_GPR[RSPOpC.rt].B[1] = RSP_Vect[RSPOpC.rd].B[15 - element];
	RSP_GPR[RSPOpC.rt].B[0] = RSP_Vect[RSPOpC.rd].B[15 - ((element + 1) % 16)];
	RSP_GPR[RSPOpC.rt].W = RSP_GPR[RSPOpC.rt].HW[0];
}

void RSP_Cop2_CF (void) {
	switch ((RSPOpC.rd & 0x03)) {
	case 0: RSP_GPR[RSPOpC.rt].W = RSP_Flags[0].HW[0]; break;
	case 1: RSP_GPR[RSPOpC.rt].W = RSP_Flags[1].HW[0]; break;
	case 2: RSP_GPR[RSPOpC.rt].W = RSP_Flags[2].HW[0]; break;
	case 3: RSP_GPR[RSPOpC.rt].W = RSP_Flags[2].HW[0]; break;
	}
}

void RSP_Cop2_MT (void) {
	int32_t element = 15 - (RSPOpC.sa >> 1);
	RSP_Vect[RSPOpC.rd].B[element] = RSP_GPR[RSPOpC.rt].B[1];
	if (element != 0) {
		RSP_Vect[RSPOpC.rd].B[element - 1] = RSP_GPR[RSPOpC.rt].B[0];
	}
}

void RSP_Cop2_CT (void) {
	switch ((RSPOpC.rd & 0x03)) {
	case 0: RSP_Flags[0].HW[0] = RSP_GPR[RSPOpC.rt].HW[0]; break;
	case 1: RSP_Flags[1].HW[0] = RSP_GPR[RSPOpC.rt].HW[0]; break;
	case 2: RSP_Flags[2].B[0] = RSP_GPR[RSPOpC.rt].B[0]; break;
	case 3: RSP_Flags[2].B[0] = RSP_GPR[RSPOpC.rt].B[0]; break;
	}
}

void RSP_COP2_VECTOR (void) {
//((void (*)()) RSP_Vector[ RSPOpC.funct ])();
	(RSP_Vector[ RSPOpC.funct ])();
}
/************************** Vect functions **************************/
 /*
 vmulf   $v27, $v31[14]

 Format:  VMULF $v<dest>, $v<s1>, $v<s2>[el]
                     sa      rd       rt  rs
                     */
void RSP_Vector_VMULF (void) {
	int32_t count, el, del;
	REGISTER32 temp;

	for (count = 0; count < 8; count ++ ) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

		if (RSP_Vect[RSPOpC.rd].UHW[el] != 0x8000 || RSP_Vect[RSPOpC.rt].UHW[del] != 0x8000) {
			temp.W = ((int32_t)RSP_Vect[RSPOpC.rd].HW[el] * (int32_t)RSP_Vect[RSPOpC.rt].HW[del]) << 1;
			temp.UW += 0x8000;
			RSP_ACCUM[el].HW[2] = temp.HW[1];
			RSP_ACCUM[el].HW[1] = temp.HW[0];
			if ( RSP_ACCUM[el].HW[2] < 0 ) {
				RSP_ACCUM[el].HW[3] = -1;
			} else {
				RSP_ACCUM[el].HW[3] = 0;
			}
			RSP_Vect[RSPOpC.sa].HW[el] = RSP_ACCUM[el].HW[2];
		} else {
			temp.W = 0x80000000;
			RSP_ACCUM[el].UHW[3] = 0;
			RSP_ACCUM[el].UHW[2] = 0x8000;
			RSP_ACCUM[el].UHW[1] = 0x8000;
			RSP_Vect[RSPOpC.sa].HW[el] = 0x7FFF;
		}
	}
}

void RSP_Vector_VMULU (void) {
	int32_t count, el, del;

	for (count = 0; count < 8; count ++ ) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];
		RSP_ACCUM[el].DW = (int64_t)(RSP_Vect[RSPOpC.rd].HW[el] * RSP_Vect[RSPOpC.rt].HW[del]) << 17;
		RSP_ACCUM[el].DW += 0x80000000;
		if (RSP_ACCUM[el].DW < 0) {
			RSP_Vect[RSPOpC.sa].HW[el] = 0;
		} else if ((int16_t)(RSP_ACCUM[el].UHW[3] ^ RSP_ACCUM[el].UHW[2]) < 0) {
			RSP_Vect[RSPOpC.sa].HW[el] = -1;
		} else {
			RSP_Vect[RSPOpC.sa].HW[el] = RSP_ACCUM[el].HW[2];
		}
	}
}

/*
 vmudl   $v23, $v30, $v24[10]
 rs=10, rt=24, rd=30, sa=23

   Format:  VMUDM $v<dest>, $v<s1>, $v<s2>[el]
 					sa		  rd	  rt  rs
 vmudm   $v23, $v20, $v24[10]
  rs=10, rt=24, rd=20, sa=23

*/

void RSP_Vector_VMUDL (void) {
	int32_t count, el, del;
	REGISTER32 temp;

	/*if(((*PrgCount) & 0xFFC) == 0xC08) {
		for(int32_t i = 0; i < 32; i++) {
			cprintf("i = %d : ", i);
			for (count = 0; count < 8; count ++ ) {
				cprintf("%d,%d ", Indx[i].B[count], EleSpec[i].B[Indx[i].B[count]]);
			}
			cprintf("\n");
		}

	}*/


	for (count = 0; count < 8; count ++ ) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

		temp.UW = (uint32_t)RSP_Vect[RSPOpC.rd].UHW[el] * (uint32_t)RSP_Vect[RSPOpC.rt].UHW[del];

		RSP_ACCUM[el].W[1] = 0;
		RSP_ACCUM[el].HW[1] = temp.HW[1];
		RSP_Vect[RSPOpC.sa].HW[el] = RSP_ACCUM[el].HW[1];

	}

}

void RSP_Vector_VMUDM (void) {
	int32_t count, el, del;
	REGISTER32 temp;

//	cprintf("%08x%08x x %08x%08x = ", RSP_Vect[RSPOpC.rd].DW[0], RSP_Vect[RSPOpC.rd].DW[1],RSP_Vect[RSPOpC.rt].DW[0],RSP_Vect[RSPOpC.rt].DW[1])


	for (count = 0; count < 8; count ++ ) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

		temp.UW = (uint32_t)((int32_t)RSP_Vect[RSPOpC.rd].HW[el]) * (uint32_t)RSP_Vect[RSPOpC.rt].UHW[del];
		if (temp.W < 0) {
			RSP_ACCUM[el].HW[3] = -1;
		} else {
			RSP_ACCUM[el].HW[3] = 0;
		}
		RSP_ACCUM[el].HW[2] = temp.HW[1];
		RSP_ACCUM[el].HW[1] = temp.HW[0];
		RSP_Vect[RSPOpC.sa].HW[el] = RSP_ACCUM[el].HW[2];
	}
	//cprintf("%08x%08x\n", RSP_Vect[RSPOpC.sa].DW[0],RSP_Vect[RSPOpC.sa].DW[1]);
}


void RSP_Vector_VMUDN (void) {
	int32_t count, el, del;
	REGISTER32 temp;

	for (count = 0; count < 8; count ++ ) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];


		temp.UW = (uint32_t)RSP_Vect[RSPOpC.rd].UHW[el] * (uint32_t)(int32_t)(RSP_Vect[RSPOpC.rt].HW[del]);
		if (temp.W < 0) {
			RSP_ACCUM[el].HW[3] = -1;
		} else {
			RSP_ACCUM[el].HW[3] = 0;
		}
		RSP_ACCUM[el].HW[2] = temp.HW[1];
		RSP_ACCUM[el].HW[1] = temp.HW[0];
		RSP_Vect[RSPOpC.sa].HW[el] = RSP_ACCUM[el].HW[1];
	}

}

void RSP_Vector_VMUDH (void) {
	int32_t count, el, del;

	for (count = 0; count < 8; count ++ ) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

		RSP_ACCUM[el].W[1] = (int32_t)RSP_Vect[RSPOpC.rd].HW[el] * (int32_t)RSP_Vect[RSPOpC.rt].HW[del];
		RSP_ACCUM[el].HW[1] = 0;
		if (RSP_ACCUM[el].HW[3] < 0) {
			if (RSP_ACCUM[el].UHW[3] != 0xFFFF) {
				RSP_Vect[RSPOpC.sa].HW[el] = (uint16_t)0x8000;
			} else {
				if (RSP_ACCUM[el].HW[2] >= 0) {
					RSP_Vect[RSPOpC.sa].HW[el] = (uint16_t)0x8000;
				} else {
					RSP_Vect[RSPOpC.sa].HW[el] = RSP_ACCUM[el].HW[2];
				}
			}
		} else {
			if (RSP_ACCUM[el].UHW[3] != 0) {
				RSP_Vect[RSPOpC.sa].HW[el] = 0x7FFF;
			} else {
				if (RSP_ACCUM[el].HW[2] < 0) {
					RSP_Vect[RSPOpC.sa].HW[el] = 0x7FFF;
				} else {
					RSP_Vect[RSPOpC.sa].HW[el] = RSP_ACCUM[el].HW[2];
				}
			}
		}
	}
}

void RSP_Vector_VMACF (void) {
	int32_t count, el, del;
	//UWORD temp, temp2;
	REGISTER32 temp;

	for (count = 0; count < 8; count ++ ) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

		/*temp.W = (int32_t)RSP_Vect[RSPOpC.rd].HW[el] * (int32_t)(uint32_t)RSP_Vect[RSPOpC.rt].HW[del];
		RSP_ACCUM[el].UHW[3] += (uint16_t)(temp.W >> 31);
		temp.UW = temp.UW << 1;
		temp2.UW = temp.UHW[0] + RSP_ACCUM[el].UHW[1];
		RSP_ACCUM[el].HW[1] = temp2.HW[0];
		temp2.UW = temp.UHW[1] + RSP_ACCUM[el].UHW[2] + temp2.UHW[1];
		RSP_ACCUM[el].HW[2] = temp2.HW[0];
		RSP_ACCUM[el].HW[3] += temp2.HW[1];*/
		temp.W = (int32_t)RSP_Vect[RSPOpC.rd].HW[el] * (int32_t)(uint32_t)RSP_Vect[RSPOpC.rt].HW[del];
		RSP_ACCUM[el].DW += ((int64_t)temp.W) << 17;
		if (RSP_ACCUM[el].HW[3] < 0) {
			if (RSP_ACCUM[el].UHW[3] != 0xFFFF) {
				RSP_Vect[RSPOpC.sa].HW[el] = (uint16_t)0x8000;
			} else {
				if (RSP_ACCUM[el].HW[2] >= 0) {
					RSP_Vect[RSPOpC.sa].HW[el] = (uint16_t)0x8000;
				} else {
					RSP_Vect[RSPOpC.sa].HW[el] = RSP_ACCUM[el].HW[2];
				}
			}
		} else {
			if (RSP_ACCUM[el].UHW[3] != 0) {
				RSP_Vect[RSPOpC.sa].HW[el] = 0x7FFF;
			} else {
				if (RSP_ACCUM[el].HW[2] < 0) {
					RSP_Vect[RSPOpC.sa].HW[el] = 0x7FFF;
				} else {
					RSP_Vect[RSPOpC.sa].HW[el] = RSP_ACCUM[el].HW[2];
				}
			}
		}
	}
}

void RSP_Vector_VMACU (void) {
	int32_t count, el, del;
	REGISTER32 temp, temp2;

	for (count = 0; count < 8; count ++ ) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

		temp.W = (int32_t)RSP_Vect[RSPOpC.rd].HW[el] * (int32_t)(uint32_t)RSP_Vect[RSPOpC.rt].HW[del];
		RSP_ACCUM[el].UHW[3] += (uint16_t)(temp.W >> 31);
		temp.UW = temp.UW << 1;
		temp2.UW = temp.UHW[0] + RSP_ACCUM[el].UHW[1];
		RSP_ACCUM[el].HW[1] = temp2.HW[0];
		temp2.UW = temp.UHW[1] + RSP_ACCUM[el].UHW[2] + temp2.UHW[1];
		RSP_ACCUM[el].HW[2] = temp2.HW[0];
		RSP_ACCUM[el].HW[3] += temp2.HW[1];
		if (RSP_ACCUM[el].HW[3] < 0) {
			RSP_Vect[RSPOpC.sa].HW[el] = 0;
		} else {
			if (RSP_ACCUM[el].UHW[3] != 0) {
				RSP_Vect[RSPOpC.sa].UHW[el] = 0xFFFF;
			} else {
				if (RSP_ACCUM[el].HW[2] < 0) {
					RSP_Vect[RSPOpC.sa].UHW[el] = 0xFFFF;
				} else {
					RSP_Vect[RSPOpC.sa].HW[el] = RSP_ACCUM[el].HW[2];
				}
			}
		}
	}
}

void RSP_Vector_VMACQ (void) {
	int32_t count, el, del;
	REGISTER32 temp;

	for (count = 0; count < 8; count ++ ) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

		if (RSP_ACCUM[el].W[1] > 0x20) {
			if ((RSP_ACCUM[el].W[1] & 0x20) == 0) {
				RSP_ACCUM[el].W[1] -= 0x20;
			}
		} else if (RSP_ACCUM[el].W[1] < -0x20) {
			if ((RSP_ACCUM[el].W[1] & 0x20) == 0) {
				RSP_ACCUM[el].W[1] += 0x20;
			}
		}
		temp.W = RSP_ACCUM[el].W[1] >> 1;
		if (temp.HW[1] < 0) {
			if (temp.UHW[1] != 0xFFFF) {
				RSP_Vect[RSPOpC.sa].HW[el] = (uint16_t)0x8000;
			} else {
				if (temp.HW[0] >= 0) {
					RSP_Vect[RSPOpC.sa].HW[el] = (uint16_t)0x8000;
				} else {
					RSP_Vect[RSPOpC.sa].HW[el] = (uint16_t)(temp.UW & 0xFFF0);
				}
			}
		} else {
			if (temp.UHW[1] != 0) {
				RSP_Vect[RSPOpC.sa].HW[el] = 0x7FF0;
			} else {
				if (temp.HW[0] < 0) {
					RSP_Vect[RSPOpC.sa].HW[el] = 0x7FF0;
				} else {
					RSP_Vect[RSPOpC.sa].HW[el] = (uint16_t)(temp.UW & 0xFFF0);
				}
			}
		}
	}
}

void RSP_Vector_VMADL (void) {
	int32_t count, el, del;
	REGISTER32 temp, temp2;



	for (count = 0; count < 8; count ++ ) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

		temp.UW = (uint32_t)RSP_Vect[RSPOpC.rd].UHW[el] * (uint32_t)RSP_Vect[RSPOpC.rt].UHW[del];
		temp2.UW = temp.UHW[1] + RSP_ACCUM[el].UHW[1];
		RSP_ACCUM[el].HW[1] = temp2.HW[0];
		temp2.UW = RSP_ACCUM[el].UHW[2] + temp2.UHW[1];
		RSP_ACCUM[el].HW[2] = temp2.HW[0];
		RSP_ACCUM[el].HW[3] += temp2.HW[1];
		if (RSP_ACCUM[el].HW[3] < 0) {
			if (RSP_ACCUM[el].UHW[3] != 0xFFFF) {
				RSP_Vect[RSPOpC.sa].HW[el] = 0;
			} else {
				if (RSP_ACCUM[el].HW[2] >= 0) {
					RSP_Vect[RSPOpC.sa].HW[el] = 0;
				} else {
					RSP_Vect[RSPOpC.sa].HW[el] = RSP_ACCUM[el].HW[1];
				}
			}
		} else {
			if (RSP_ACCUM[el].UHW[3] != 0) {
				RSP_Vect[RSPOpC.sa].UHW[el] = 0xFFFF;
			} else {
				if (RSP_ACCUM[el].HW[2] < 0) {
					RSP_Vect[RSPOpC.sa].UHW[el] = 0xFFFF;
				} else {
					RSP_Vect[RSPOpC.sa].HW[el] = RSP_ACCUM[el].HW[1];
				}
			}
		}
	}

}

void RSP_Vector_VMADM (void) {
	int32_t count, el, del;
	REGISTER32 temp, temp2;

	int32_t last = -1;

	for (count = 0; count < 8; count ++ ) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

		temp.UW = (uint32_t)((int32_t)RSP_Vect[RSPOpC.rd].HW[el]) * (uint32_t)RSP_Vect[RSPOpC.rt].UHW[del];
		temp2.UW = temp.UHW[0] + RSP_ACCUM[el].UHW[1];
		RSP_ACCUM[el].HW[1] = temp2.HW[0];
		temp2.UW = temp.UHW[1] + RSP_ACCUM[el].UHW[2] + temp2.UHW[1];
		RSP_ACCUM[el].HW[2] = temp2.HW[0];
		RSP_ACCUM[el].HW[3] += temp2.HW[1];
		if (temp.W < 0) {
			RSP_ACCUM[el].HW[3] -= 1;
		}
		if (RSP_ACCUM[el].HW[3] < 0) {
			if (RSP_ACCUM[el].UHW[3] != 0xFFFF) {
				RSP_Vect[RSPOpC.sa].HW[el] = (uint16_t)0x8000;
			} else {
				if (RSP_ACCUM[el].HW[2] >= 0) {
					RSP_Vect[RSPOpC.sa].HW[el] = (uint16_t)0x8000;
				} else {
					RSP_Vect[RSPOpC.sa].HW[el] = RSP_ACCUM[el].HW[2];
				}
			}
		} else {
			if (RSP_ACCUM[el].UHW[3] != 0) {
				RSP_Vect[RSPOpC.sa].HW[el] = 0x7FFF;
			} else {
				if (RSP_ACCUM[el].HW[2] < 0) {
					RSP_Vect[RSPOpC.sa].HW[el] = 0x7FFF;
				} else {
					RSP_Vect[RSPOpC.sa].HW[el] = RSP_ACCUM[el].HW[2];
				}
			}
		}
		//RSP_Vect[RSPOpC.sa].HW[el] = RSP_ACCUM[el].HW[2];
	}
}

void RSP_Vector_VMADN (void) {
	int32_t count, el, del;
	REGISTER32 temp, temp2;

	for (count = 0; count < 8; count ++ ) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

		temp.UW = (uint32_t)RSP_Vect[RSPOpC.rd].UHW[el] * (uint32_t)((int32_t)RSP_Vect[RSPOpC.rt].HW[del]);
		temp2.UW = temp.UHW[0] + RSP_ACCUM[el].UHW[1];
		RSP_ACCUM[el].HW[1] = temp2.HW[0];
		temp2.UW = temp.UHW[1] + RSP_ACCUM[el].UHW[2] + temp2.UHW[1];
		RSP_ACCUM[el].HW[2] = temp2.HW[0];
		RSP_ACCUM[el].HW[3] += temp2.HW[1];
		if (temp.W < 0) {
			RSP_ACCUM[el].HW[3] -= 1;
		}
		if (RSP_ACCUM[el].HW[3] < 0) {
			if (RSP_ACCUM[el].UHW[3] != 0xFFFF) {
				RSP_Vect[RSPOpC.sa].HW[el] = 0;
			} else {
				if (RSP_ACCUM[el].HW[2] >= 0) {
					RSP_Vect[RSPOpC.sa].HW[el] = 0;
				} else {
					RSP_Vect[RSPOpC.sa].HW[el] = RSP_ACCUM[el].HW[1];
				}
			}
		} else {
			if (RSP_ACCUM[el].UHW[3] != 0) {
				RSP_Vect[RSPOpC.sa].UHW[el] = 0xFFFF;
			} else {
				if (RSP_ACCUM[el].HW[2] < 0) {
					RSP_Vect[RSPOpC.sa].UHW[el] = 0xFFFF;
				} else {
					RSP_Vect[RSPOpC.sa].HW[el] = RSP_ACCUM[el].HW[1];
				}
			}
		}
	}
}

void RSP_Vector_VMADH (void) {
	int32_t count, el, del;

	for (count = 0; count < 8; count ++ ) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

		RSP_ACCUM[el].W[1] += (int32_t)RSP_Vect[RSPOpC.rd].HW[el] * (int32_t)RSP_Vect[RSPOpC.rt].HW[del];
		if (RSP_ACCUM[el].HW[3] < 0) {
			if (RSP_ACCUM[el].UHW[3] != 0xFFFF) {
				RSP_Vect[RSPOpC.sa].HW[el] = (uint16_t)0x8000;
			} else {
				if (RSP_ACCUM[el].HW[2] >= 0) {
					RSP_Vect[RSPOpC.sa].HW[el] = (uint16_t)0x8000;
				} else {
					RSP_Vect[RSPOpC.sa].HW[el] = RSP_ACCUM[el].HW[2];
				}
			}
		} else {
			if (RSP_ACCUM[el].UHW[3] != 0) {
				RSP_Vect[RSPOpC.sa].HW[el] = 0x7FFF;
			} else {
				if (RSP_ACCUM[el].HW[2] < 0) {
					RSP_Vect[RSPOpC.sa].HW[el] = 0x7FFF;
				} else {
					RSP_Vect[RSPOpC.sa].HW[el] = RSP_ACCUM[el].HW[2];
				}
			}
		}
	}
}

void RSP_Vector_VADD (void) {
	int32_t count, el, del;
	REGISTER32 temp;

	for ( count = 0; count < 8; count++ ) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

		temp.W = (int32_t)RSP_Vect[RSPOpC.rd].HW[el] + (int32_t)RSP_Vect[RSPOpC.rt].HW[del] +
			 ((RSP_Flags[0].UW >> (7 - el)) & 0x1);
		RSP_ACCUM[el].HW[1] = temp.HW[0];
		if ((temp.HW[0] & 0x8000) == 0) {
			if (temp.HW[1] != 0) {
				RSP_Vect[RSPOpC.sa].HW[el] = (uint16_t)0x8000;
			} else {
				RSP_Vect[RSPOpC.sa].HW[el] = temp.HW[0];
			}
		} else {
			if (temp.HW[1] != -1 ) {
				RSP_Vect[RSPOpC.sa].HW[el] = 0x7FFF;
			} else {
				RSP_Vect[RSPOpC.sa].HW[el] = temp.HW[0];
			}
		}
	}
	RSP_Flags[0].UW = 0;
}

void RSP_Vector_VSUB (void) {
	int32_t count, el, del;
	REGISTER32 temp;

	for ( count = 0; count < 8; count++ ) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

		temp.W = (int32_t)RSP_Vect[RSPOpC.rd].HW[el] - (int32_t)RSP_Vect[RSPOpC.rt].HW[del] -
			 ((RSP_Flags[0].UW >> (7 - el)) & 0x1);
		RSP_ACCUM[el].HW[1] = temp.HW[0];
		if ((temp.HW[0] & 0x8000) == 0) {
			if (temp.HW[1] != 0) {
				RSP_Vect[RSPOpC.sa].HW[el] = (uint16_t)0x8000;
			} else {
				RSP_Vect[RSPOpC.sa].HW[el] = temp.HW[0];
			}
		} else {
			if (temp.HW[1] != -1 ) {
				RSP_Vect[RSPOpC.sa].HW[el] = 0x7FFF;
			} else {
				RSP_Vect[RSPOpC.sa].HW[el] = temp.HW[0];
			}
		}
	}
	RSP_Flags[0].UW = 0;
}

void RSP_Vector_VABS (void) {
	int32_t count, el, del;

	for ( count = 0; count < 8; count++ ) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

		if (RSP_Vect[RSPOpC.rd].HW[el] > 0) {
			RSP_Vect[RSPOpC.sa].HW[el] = RSP_Vect[RSPOpC.rt].UHW[del];
		} else if (RSP_Vect[RSPOpC.rd].HW[el] < 0) {
			if (RSP_Vect[RSPOpC.rt].UHW[EleSpec[RSPOpC.rs].B[el]] == 0x8000) {
				RSP_Vect[RSPOpC.sa].HW[el] = 0x7FFF;
			} else {
				RSP_Vect[RSPOpC.sa].HW[el] = RSP_Vect[RSPOpC.rt].HW[del] * -1;
			}
		} else {
			RSP_Vect[RSPOpC.sa].HW[el] = 0;
		}
		RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.sa].HW[el];
	}
}

void RSP_Vector_VADDC (void) {
	int32_t count, el, del;
	REGISTER32 temp;

	RSP_Flags[0].UW = 0;
	for ( count = 0; count < 8; count++ ) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

		temp.UW = (int32_t)RSP_Vect[RSPOpC.rd].UHW[el] + (int32_t)RSP_Vect[RSPOpC.rt].UHW[del];
		RSP_ACCUM[el].HW[1] = temp.HW[0];
		RSP_Vect[RSPOpC.sa].HW[el] = temp.HW[0];
		if (temp.UW & 0xffff0000) {
			RSP_Flags[0].UW |= ( 1 << (7 - el) );
		}
	}
}

void RSP_Vector_VSUBC (void) {
	int32_t count, el, del;
	REGISTER32 temp;

	RSP_Flags[0].UW = 0x0;
	for ( count = 0; count < 8; count++ ) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

		temp.UW = (int32_t)RSP_Vect[RSPOpC.rd].UHW[el] - (int32_t)RSP_Vect[RSPOpC.rt].UHW[del];
		RSP_ACCUM[el].HW[1] = temp.HW[0];
		RSP_Vect[RSPOpC.sa].HW[el] = temp.HW[0];
		if (temp.HW[0] != 0) {
			RSP_Flags[0].UW |= ( 0x1 << (15 - el) );
		}
		if (temp.UW & 0xffff0000) {
			RSP_Flags[0].UW |= ( 0x1 << (7 - el) );
		}
	}
}

void RSP_Vector_VSAW (void) {
	switch ((RSPOpC.rs & 0xF)) {
	case 8:
		RSP_Vect[RSPOpC.sa].HW[0] = RSP_ACCUM[0].HW[3];
		RSP_Vect[RSPOpC.sa].HW[1] = RSP_ACCUM[1].HW[3];
		RSP_Vect[RSPOpC.sa].HW[2] = RSP_ACCUM[2].HW[3];
		RSP_Vect[RSPOpC.sa].HW[3] = RSP_ACCUM[3].HW[3];
		RSP_Vect[RSPOpC.sa].HW[4] = RSP_ACCUM[4].HW[3];
		RSP_Vect[RSPOpC.sa].HW[5] = RSP_ACCUM[5].HW[3];
		RSP_Vect[RSPOpC.sa].HW[6] = RSP_ACCUM[6].HW[3];
		RSP_Vect[RSPOpC.sa].HW[7] = RSP_ACCUM[7].HW[3];
		break;
	case 9:
		RSP_Vect[RSPOpC.sa].HW[0] = RSP_ACCUM[0].HW[2];
		RSP_Vect[RSPOpC.sa].HW[1] = RSP_ACCUM[1].HW[2];
		RSP_Vect[RSPOpC.sa].HW[2] = RSP_ACCUM[2].HW[2];
		RSP_Vect[RSPOpC.sa].HW[3] = RSP_ACCUM[3].HW[2];
		RSP_Vect[RSPOpC.sa].HW[4] = RSP_ACCUM[4].HW[2];
		RSP_Vect[RSPOpC.sa].HW[5] = RSP_ACCUM[5].HW[2];
		RSP_Vect[RSPOpC.sa].HW[6] = RSP_ACCUM[6].HW[2];
		RSP_Vect[RSPOpC.sa].HW[7] = RSP_ACCUM[7].HW[2];
		break;
	case 10:
		RSP_Vect[RSPOpC.sa].HW[0] = RSP_ACCUM[0].HW[1];
		RSP_Vect[RSPOpC.sa].HW[1] = RSP_ACCUM[1].HW[1];
		RSP_Vect[RSPOpC.sa].HW[2] = RSP_ACCUM[2].HW[1];
		RSP_Vect[RSPOpC.sa].HW[3] = RSP_ACCUM[3].HW[1];
		RSP_Vect[RSPOpC.sa].HW[4] = RSP_ACCUM[4].HW[1];
		RSP_Vect[RSPOpC.sa].HW[5] = RSP_ACCUM[5].HW[1];
		RSP_Vect[RSPOpC.sa].HW[6] = RSP_ACCUM[6].HW[1];
		RSP_Vect[RSPOpC.sa].HW[7] = RSP_ACCUM[7].HW[1];
		break;
	default:
		RSP_Vect[RSPOpC.sa].DW[1] = 0;
		RSP_Vect[RSPOpC.sa].DW[0] = 0;
	}
}

void RSP_Vector_VLT (void) {
	int32_t count, el, del;

	RSP_Flags[1].UW = 0;
	for ( count = 0; count < 8; count++ ) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

		if (RSP_Vect[RSPOpC.rd].HW[el] < RSP_Vect[RSPOpC.rt].HW[del]) {
			RSP_Vect[RSPOpC.sa].HW[el] = RSP_Vect[RSPOpC.rd].UHW[el];
			RSP_Flags[1].UW |= ( 1 << (7 - el) );
		} else if (RSP_Vect[RSPOpC.rd].HW[el] != RSP_Vect[RSPOpC.rt].HW[del]) {
			RSP_Vect[RSPOpC.sa].HW[el] = RSP_Vect[RSPOpC.rt].UHW[del];
			RSP_Flags[1].UW &= ~( 1 << (7 - el) );
		} else {
			RSP_Vect[RSPOpC.sa].HW[el] = RSP_Vect[RSPOpC.rd].UHW[el];
			if ( (RSP_Flags[0].UW & (0x101 << (7 - el))) == (uint16_t)(0x101 << (7 - el))) {
				RSP_Flags[1].UW |= ( 1 << (7 - el) );
			} else {
				RSP_Flags[1].UW &= ~( 1 << (7 - el) );
			}
		}
		RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.sa].HW[el];
	}
	RSP_Flags[0].UW = 0;
}

void RSP_Vector_VEQ (void) {
	int32_t count, el, del;

	RSP_Flags[1].UW = 0;
	for ( count = 0; count < 8; count++ ) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

        RSP_Vect[RSPOpC.sa].HW[el] = RSP_Vect[RSPOpC.rt].UHW[del];
		RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.rt].UHW[del];
		if (RSP_Vect[RSPOpC.rd].UHW[el] == RSP_Vect[RSPOpC.rt].UHW[del]) {
			if ( (RSP_Flags[0].UW & (1 << (15 - el))) == 0) {
				RSP_Flags[1].UW |= ( 1 << (7 - el));
			}
		}
	}
	RSP_Flags[0].UW = 0;
}

void RSP_Vector_VNE (void) {
	int32_t count, el, del;

	RSP_Flags[1].UW = 0;
	for ( count = 0; count < 8; count++ ) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

        RSP_Vect[RSPOpC.sa].HW[el] = RSP_Vect[RSPOpC.rd].UHW[el];
		RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.rd].UHW[el];
		if (RSP_Vect[RSPOpC.rd].UHW[el] != RSP_Vect[RSPOpC.rt].UHW[del]) {
			RSP_Flags[1].UW |= ( 1 << (7 - el) );
		} else {
			if ( (RSP_Flags[0].UW & (1 << (15 - el))) != 0) {
				RSP_Flags[1].UW |= ( 1 << (7 - el) );
			}
		}
	}
	RSP_Flags[0].UW = 0;
}

void RSP_Vector_VGE (void) {
	int32_t count, el, del;

	RSP_Flags[1].UW = 0;
	for ( count = 0; count < 8; count++ ) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

		if (RSP_Vect[RSPOpC.rd].HW[el] == RSP_Vect[RSPOpC.rt].HW[del]) {
			RSP_Vect[RSPOpC.sa].HW[el] = RSP_Vect[RSPOpC.rd].UHW[el];
			if ( (RSP_Flags[0].UW & (0x101 << (7 - el))) == (uint16_t)(0x101 << (7 - el))) {
				RSP_Flags[1].UW &= ~( 1 << (7 - el) );
			} else {
				RSP_Flags[1].UW |= ( 1 << (7 - el) );
			}
		} else if (RSP_Vect[RSPOpC.rd].HW[el] > RSP_Vect[RSPOpC.rt].HW[del]) {
			RSP_Vect[RSPOpC.sa].HW[el] = RSP_Vect[RSPOpC.rd].UHW[el];
			RSP_Flags[1].UW |= ( 1 << (7 - el) );
		} else {
			RSP_Vect[RSPOpC.sa].HW[el] = RSP_Vect[RSPOpC.rt].UHW[del];
			RSP_Flags[1].UW &= ~( 1 << (7 - el) );
		}
		RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.sa].HW[el];
	}
	RSP_Flags[0].UW = 0;
}

void RSP_Vector_VCL (void) {
	int32_t count, el, del;

	for (count = 0;count < 8; count++) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

		if ((RSP_Flags[0].UW & ( 1 << (7 - el))) != 0 ) {
			if ((RSP_Flags[0].UW & ( 1 << (15 - el))) != 0 ) {
				if ((RSP_Flags[1].UW & ( 1 << (7 - el))) != 0 ) {
					RSP_ACCUM[el].HW[1] = -RSP_Vect[RSPOpC.rt].UHW[del];
				} else {
					RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.rd].HW[el];
				}
			} else {
				if ((RSP_Flags[2].UW & ( 1 << (7 - el)))) {
					if ( RSP_Vect[RSPOpC.rd].UHW[el] + RSP_Vect[RSPOpC.rt].UHW[del] > 0x10000) {
						RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.rd].HW[el];
						RSP_Flags[1].UW &= ~(1 << (7 - el));
					} else {
						RSP_ACCUM[el].HW[1] = -RSP_Vect[RSPOpC.rt].UHW[del];
						RSP_Flags[1].UW |= (1 << (7 - el));
					}
				} else {
					if (RSP_Vect[RSPOpC.rt].UHW[del] + RSP_Vect[RSPOpC.rd].UHW[el] != 0) {
						RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.rd].HW[el];
						RSP_Flags[1].UW &= ~(1 << (7 - el));
					} else {
						RSP_ACCUM[el].HW[1] = -RSP_Vect[RSPOpC.rt].UHW[del];
						RSP_Flags[1].UW |= (1 << (7 - el));
					}
				}
			}
		} else {
			if ((RSP_Flags[0].UW & ( 1 << (15 - el))) != 0 ) {
				if ((RSP_Flags[1].UW & ( 1 << (15 - el))) != 0 ) {
					RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.rt].HW[del];
				} else {
					RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.rd].HW[el];
				}
			} else {
				if ( RSP_Vect[RSPOpC.rd].UHW[el] - RSP_Vect[RSPOpC.rt].UHW[del] >= 0) {
					RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.rt].UHW[del];
					RSP_Flags[1].UW |= (1 << (15 - el));
				} else {
					RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.rd].HW[el];
					RSP_Flags[1].UW &= ~(1 << (15 - el));
				}
			}
		}
		RSP_Vect[RSPOpC.sa].HW[el] = RSP_ACCUM[el].HW[1];
	}
	RSP_Flags[0].UW = 0;
	RSP_Flags[2].UW = 0;
}

void RSP_Vector_VCH (void) {
	int32_t count, el, del;

	RSP_Flags[0].UW = 0;
	RSP_Flags[1].UW = 0;
	RSP_Flags[2].UW = 0;

	for (count = 0;count < 8; count++) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

		if ((RSP_Vect[RSPOpC.rd].HW[el] ^ RSP_Vect[RSPOpC.rt].HW[del]) < 0) {
			RSP_Flags[0].UW |= ( 1 << (7 - el));
			if (RSP_Vect[RSPOpC.rt].HW[del] < 0) {
				RSP_Flags[1].UW |= ( 1 << (15 - el));

			}
			if (RSP_Vect[RSPOpC.rd].HW[el] + RSP_Vect[RSPOpC.rt].HW[del] <= 0) {
				if (RSP_Vect[RSPOpC.rd].HW[el] + RSP_Vect[RSPOpC.rt].HW[del] == -1) {
					RSP_Flags[2].UW |= ( 1 << (7 - el));
				}
				RSP_Flags[1].UW |= ( 1 << (7 - el));
				RSP_Vect[RSPOpC.sa].HW[el] = -RSP_Vect[RSPOpC.rt].UHW[del];
			} else {
				RSP_Vect[RSPOpC.sa].HW[el] = RSP_Vect[RSPOpC.rd].HW[el];
			}
			if (RSP_Vect[RSPOpC.rd].HW[el] + RSP_Vect[RSPOpC.rt].HW[del] != 0) {
				if (RSP_Vect[RSPOpC.rd].HW[el] != ~RSP_Vect[RSPOpC.rt].HW[del]) {
					RSP_Flags[0].UW |= ( 1 << (15 - el));
				}
			}
		} else {
			if (RSP_Vect[RSPOpC.rt].HW[del] < 0) {
				RSP_Flags[1].UW |= ( 1 << (7 - el));
			}
			if (RSP_Vect[RSPOpC.rd].HW[el] - RSP_Vect[RSPOpC.rt].HW[del] >= 0) {
				RSP_Flags[1].UW |= ( 1 << (15 - el));
				RSP_Vect[RSPOpC.sa].HW[el] = RSP_Vect[RSPOpC.rt].UHW[del];
			} else {
				RSP_Vect[RSPOpC.sa].HW[el] = RSP_Vect[RSPOpC.rd].HW[el];
			}
			if (RSP_Vect[RSPOpC.rd].HW[el] - RSP_Vect[RSPOpC.rt].HW[del] != 0) {
				if (RSP_Vect[RSPOpC.rd].HW[el] != ~RSP_Vect[RSPOpC.rt].HW[del]) {
					RSP_Flags[0].UW |= ( 1 << (15 - el));
				}
			}
		}
		RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.sa].HW[el];
	}
}

void RSP_Vector_VCR (void) {
	int32_t count, el, del;

	RSP_Flags[0].UW = 0;
	RSP_Flags[1].UW = 0;
	RSP_Flags[2].UW = 0;
	for (count = 0;count < 8; count++) {
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

		if ((RSP_Vect[RSPOpC.rd].HW[el] ^ RSP_Vect[RSPOpC.rt].HW[del]) < 0) {
			if (RSP_Vect[RSPOpC.rt].HW[del] < 0) {
				RSP_Flags[1].UW |= ( 1 << (15 - el));
			}
			if (RSP_Vect[RSPOpC.rd].HW[el] + RSP_Vect[RSPOpC.rt].HW[del] <= 0) {
				RSP_ACCUM[el].HW[1] = ~RSP_Vect[RSPOpC.rt].UHW[del];
				RSP_Flags[1].UW |= ( 1 << (7 - el));
			} else {
				RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.rd].HW[el];
			}
		} else {
			if (RSP_Vect[RSPOpC.rt].HW[del] < 0) {
				RSP_Flags[1].UW |= ( 1 << (7 - el));
			}
			if (RSP_Vect[RSPOpC.rd].HW[el] - RSP_Vect[RSPOpC.rt].HW[del] >= 0) {
				RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.rt].UHW[del];
				RSP_Flags[1].UW |= ( 1 << (15 - el));
			} else {
				RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.rd].HW[el];
			}
		}
		RSP_Vect[RSPOpC.sa].HW[el] = RSP_ACCUM[el].HW[1];
	}
}

void RSP_Vector_VMRG (void) {
	int32_t count, el, del;

	for ( count = 0; count < 8; count ++ ){
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];

		if ((RSP_Flags[1].UW & ( 1 << (7 - el))) != 0) {
			RSP_Vect[RSPOpC.sa].HW[el] = RSP_Vect[RSPOpC.rd].HW[el];
		} else {
			RSP_Vect[RSPOpC.sa].HW[el] = RSP_Vect[RSPOpC.rt].HW[del];
		}
	}
}

void RSP_Vector_VAND (void) {
	int32_t count, el, del;

	for ( count = 0; count < 8; count ++ ){
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];
		RSP_Vect[RSPOpC.sa].HW[el] = RSP_Vect[RSPOpC.rd].HW[el] & RSP_Vect[RSPOpC.rt].HW[del];
		RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.sa].HW[el];
	}
}

void RSP_Vector_VNAND (void) {
	int32_t count, el, del;

	for ( count = 0; count < 8; count ++ ){
		el = 7 - count;
		del = EleSpec[RSPOpC.rs].B[el];
		RSP_Vect[RSPOpC.sa].HW[el] = ~(RSP_Vect[RSPOpC.rd].HW[el] & RSP_Vect[RSPOpC.rt].HW[del]);
		RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.sa].HW[el];
	}
}

void RSP_Vector_VOR (void) {
	int32_t count, el, del;

	for ( count = 0; count < 8; count ++ ){
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];
		RSP_Vect[RSPOpC.sa].HW[el] = RSP_Vect[RSPOpC.rd].HW[el] | RSP_Vect[RSPOpC.rt].HW[del];
		RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.sa].HW[el];
	}
}

void RSP_Vector_VNOR (void) {
	int32_t count, el, del;

	for ( count = 0; count < 8; count ++ ){
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];
		RSP_Vect[RSPOpC.sa].HW[el] = ~(RSP_Vect[RSPOpC.rd].HW[el] | RSP_Vect[RSPOpC.rt].HW[del]);
		RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.sa].HW[el];
	}
}

void RSP_Vector_VXOR (void) {
	int32_t count, el, del;

	for ( count = 0; count < 8; count ++ ){
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];
		RSP_Vect[RSPOpC.sa].HW[el] = RSP_Vect[RSPOpC.rd].HW[el] ^ RSP_Vect[RSPOpC.rt].HW[del];
		RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.sa].HW[el];
	}
}

void RSP_Vector_VNXOR (void) {
	int32_t count, el, del;

	for ( count = 0; count < 8; count ++ ){
		el = Indx[RSPOpC.rs].B[count];
		del = EleSpec[RSPOpC.rs].B[el];
		RSP_Vect[RSPOpC.sa].HW[el] = ~(RSP_Vect[RSPOpC.rd].HW[el] ^ RSP_Vect[RSPOpC.rt].HW[del]);
		RSP_ACCUM[el].HW[1] = RSP_Vect[RSPOpC.sa].HW[el];
	}
}

void RSP_Vector_VRCP (void) {
	int32_t count, neg;

	RecpResult.W = RSP_Vect[RSPOpC.rt].HW[EleSpec[RSPOpC.rs].B[(RSPOpC.rd & 0x7)]];
	if (RecpResult.UW == 0) {
		RecpResult.UW = 0x7FFFFFFF;
	} else {
		if (RecpResult.W < 0) {
			neg = 1;
			RecpResult.W = ~RecpResult.W + 1;
		} else {
			neg = 0;
		}
		for (count = 15; count > 0; count--) {
			if ((RecpResult.W & (1 << count))) {
				RecpResult.W &= (0xFFC0 >> (15 - count) );
				count = 0;
			}
		}
		RecpResult.W = (int32_t)((0x7FFFFFFF / (double)RecpResult.W));
		for (count = 31; count > 0; count--) {
			if ((RecpResult.W & (1 << count))) {
				RecpResult.W &= (0xFFFF8000 >> (31 - count) );
				count = 0;
			}
		}
		if (neg == 1) {
			RecpResult.W = ~RecpResult.W;
		}
	}
	for ( count = 0; count < 8; count++ ) {
		RSP_ACCUM[count].HW[1] = RSP_Vect[RSPOpC.rt].UHW[EleSpec[RSPOpC.rs].B[count]];
	}
	RSP_Vect[RSPOpC.sa].HW[7 - (RSPOpC.rd & 0x7)] = RecpResult.UHW[0];
}

void RSP_Vector_VRCPL (void) {
	int32_t count, neg;

	RecpResult.UW = RSP_Vect[RSPOpC.rt].UHW[EleSpec[RSPOpC.rs].B[(RSPOpC.rd & 0x7)]] | Recp.W;
	if (RecpResult.UW == 0) {
		RecpResult.UW = 0x7FFFFFFF;
	} else {
		if (RecpResult.W < 0) {
			neg = 1;
			if (RecpResult.UHW[1] == 0xFFFF && RecpResult.HW[0] < 0) {
				RecpResult.W = ~RecpResult.W + 1;
			} else {
				RecpResult.W = ~RecpResult.W;
			}
		} else {
			neg = 0;
		}
		for (count = 31; count > 0; count--) {
			if ((RecpResult.W & (1 << count))) {
				RecpResult.W &= (0xFFC00000 >> (31 - count) );
				count = 0;
			}
		}
		RecpResult.W = 0x7FFFFFFF / RecpResult.W;
		for (count = 31; count > 0; count--) {
			if ((RecpResult.W & (1 << count))) {
				RecpResult.W &= (0xFFFF8000 >> (31 - count) );
				count = 0;
			}
		}
		if (neg == 1) {
			RecpResult.W = ~RecpResult.W;
		}
	}
	for ( count = 0; count < 8; count++ ) {
		RSP_ACCUM[count].HW[1] = RSP_Vect[RSPOpC.rt].UHW[EleSpec[RSPOpC.rs].B[count]];
	}
	RSP_Vect[RSPOpC.sa].HW[7 - (RSPOpC.rd & 0x7)] = RecpResult.UHW[0];
}

void RSP_Vector_VRCPH (void) {
	int32_t count;

	Recp.UHW[1] = RSP_Vect[RSPOpC.rt].UHW[EleSpec[RSPOpC.rs].B[(RSPOpC.rd & 0x7)]];
	for ( count = 0; count < 8; count++ ) {
		RSP_ACCUM[count].HW[1] = RSP_Vect[RSPOpC.rt].UHW[EleSpec[RSPOpC.rs].B[count]];
	}
	RSP_Vect[RSPOpC.sa].UHW[7 - (RSPOpC.rd & 0x7)] = RecpResult.UHW[1];
}

void RSP_Vector_VMOV (void) {
	RSP_Vect[RSPOpC.sa].UHW[7 - (RSPOpC.rd & 0x7)] =
		RSP_Vect[RSPOpC.rt].UHW[EleSpec[RSPOpC.rs].B[(RSPOpC.rd & 0x7)]];
}

void RSP_Vector_VRSQ (void) {
	int32_t count, neg;

	SQrootResult.W = RSP_Vect[RSPOpC.rt].HW[EleSpec[RSPOpC.rs].B[(RSPOpC.rd & 0x7)]];
	if (SQrootResult.UW == 0) {
		SQrootResult.UW = 0x7FFFFFFF;
	} else if (SQrootResult.UW == 0xFFFF8000) {
		SQrootResult.UW = 0xFFFF0000;
	} else {
		if (SQrootResult.W < 0) {
			neg = 1;
			SQrootResult.W = ~SQrootResult.W + 1;
		} else {
			neg = 0;
		}
		for (count = 15; count > 0; count--) {
			if ((SQrootResult.W & (1 << count))) {
				SQrootResult.W &= (0xFF80 >> (15 - count) );
				count = 0;
			}
		}
		//SQrootResult.W = sqrt(SQrootResult.W);
		SQrootResult.W = (int32_t)(0x7FFFFFFF / (int32_t) sqrt((double) SQrootResult.W));
		for (count = 31; count > 0; count--) {
			if ((SQrootResult.W & (1 << count))) {
				SQrootResult.W &= (0xFFFF8000 >> (31 - count) );
				count = 0;
			}
		}
		if (neg == 1) {
			SQrootResult.W = ~SQrootResult.W;
		}
	}
	for ( count = 0; count < 8; count++ ) {
		RSP_ACCUM[count].HW[1] = RSP_Vect[RSPOpC.rt].UHW[EleSpec[RSPOpC.rs].B[count]];
	}
	RSP_Vect[RSPOpC.sa].HW[7 - (RSPOpC.rd & 0x7)] = SQrootResult.UHW[0];
}

void RSP_Vector_VRSQL (void) {
	int32_t count, neg;

	SQrootResult.UW = RSP_Vect[RSPOpC.rt].UHW[EleSpec[RSPOpC.rs].B[(RSPOpC.rd & 0x7)]] | SQroot.W;
	if (SQrootResult.UW == 0) {
		SQrootResult.UW = 0x7FFFFFFF;
	} else if (SQrootResult.UW == 0xFFFF8000) {
		SQrootResult.UW = 0xFFFF0000;
	} else {
		if (SQrootResult.W < 0) {
			neg = 1;
			if (SQrootResult.UHW[1] == 0xFFFF && SQrootResult.HW[0] < 0) {
				SQrootResult.W = ~SQrootResult.W + 1;
			} else {
				SQrootResult.W = ~SQrootResult.W;
			}
		} else {
			neg = 0;
		}
		for (count = 31; count > 0; count--) {
			if ((SQrootResult.W & (1 << count))) {
				SQrootResult.W &= (0xFF800000 >> (31 - count) );
				count = 0;
			}
		}
		SQrootResult.W = (int32_t)(0x7FFFFFFF / (int32_t) sqrt((double) SQrootResult.W));

		for (count = 31; count > 0; count--) {
			if ((SQrootResult.W & (1 << count))) {
				SQrootResult.W &= (0xFFFF8000 >> (31 - count) );
				count = 0;
			}
		}
		if (neg == 1) {
			SQrootResult.W = ~SQrootResult.W;
		}
	}
	for ( count = 0; count < 8; count++ ) {
		RSP_ACCUM[count].HW[1] = RSP_Vect[RSPOpC.rt].UHW[EleSpec[RSPOpC.rs].B[count]];
	}
	RSP_Vect[RSPOpC.sa].HW[7 - (RSPOpC.rd & 0x7)] = SQrootResult.UHW[0];
}

void RSP_Vector_VRSQH (void) {
	int32_t count;

	SQroot.UHW[1] = RSP_Vect[RSPOpC.rt].UHW[EleSpec[RSPOpC.rs].B[(RSPOpC.rd & 0x7)]];
	for ( count = 0; count < 8; count++ ) {
		RSP_ACCUM[count].HW[1] = RSP_Vect[RSPOpC.rt].UHW[EleSpec[RSPOpC.rs].B[count]];
	}
	RSP_Vect[RSPOpC.sa].UHW[7 - (RSPOpC.rd & 0x7)] = SQrootResult.UHW[1];
}

void RSP_Vector_VNOOP (void) {}
/************************** lc2 functions **************************/
void RSP_Opcode_LBV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset)) &0xFFF);
	RSP_LBV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

void RSP_Opcode_LSV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset << 1)) &0xFFF);
	RSP_LSV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

void RSP_Opcode_LLV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset << 2)) &0xFFF);
	RSP_LLV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

void RSP_Opcode_LDV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset << 3)) &0xFFF);
	RSP_LDV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

void RSP_Opcode_LQV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset << 4)) &0xFFF);
	RSP_LQV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

void RSP_Opcode_LRV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset << 4)) &0xFFF);
	RSP_LRV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

void RSP_Opcode_LPV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset << 3)) &0xFFF);
	RSP_LPV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

void RSP_Opcode_LUV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset << 3)) &0xFFF);
	RSP_LUV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

void RSP_Opcode_LHV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset << 4)) &0xFFF);
	RSP_LHV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

void RSP_Opcode_LFV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset << 4)) &0xFFF);
	RSP_LFV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

void RSP_Opcode_LTV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset << 4)) &0xFFF);
	RSP_LTV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

/************************** sc2 functions **************************/
void RSP_Opcode_SBV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset)) &0xFFF);
	RSP_SBV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

void RSP_Opcode_SSV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset << 1)) &0xFFF);
	RSP_SSV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

void RSP_Opcode_SLV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset << 2)) &0xFFF);
	RSP_SLV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

void RSP_Opcode_SDV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset << 3)) &0xFFF);
	RSP_SDV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

void RSP_Opcode_SQV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset << 4)) &0xFFF);
	RSP_SQV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

void RSP_Opcode_SRV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset << 4)) &0xFFF);
	RSP_SRV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

void RSP_Opcode_SPV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset << 3)) &0xFFF);
	RSP_SPV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

void RSP_Opcode_SUV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset << 3)) &0xFFF);
	RSP_SUV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

void RSP_Opcode_SHV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset << 4)) &0xFFF);
	RSP_SHV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

void RSP_Opcode_SFV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset << 4)) &0xFFF);
	RSP_SFV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

void RSP_Opcode_STV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset << 4)) &0xFFF);
	RSP_STV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

void RSP_Opcode_SWV ( void ) {
	uint32_t Address = ((RSP_GPR[RSPOpC.base].UW + (uint32_t)(RSPOpC.voffset << 4)) & 0xFFF);
	RSP_SWV_DMEM( Address, RSPOpC.rt, RSPOpC.del);
}

/************************** Other functions **************************/

void rsp_UnknownOpcode (void) {

	printf("Unhandled RSP opcode (%08x)\n", RSPOpC.Hex);
	//ExitThread(0);
	exit(0);
}

