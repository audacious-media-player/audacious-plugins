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
void CompileReadTLBMiss (BLOCK_SECTION * Section, int32_t AddressReg, int32_t LookUpReg );

/************************** Branch functions  ************************/
void Compile_R4300i_Branch         ( BLOCK_SECTION * Section, void (*CompareFunc)(BLOCK_SECTION * Section), int32_t BranchType, uint32_t Link);
void Compile_R4300i_BranchLikely   ( BLOCK_SECTION * Section, void (*CompareFunc)(BLOCK_SECTION * Section), uint32_t Link);
void BNE_Compare                   ( BLOCK_SECTION * Section );
void BEQ_Compare                   ( BLOCK_SECTION * Section );
void BGTZ_Compare                  ( BLOCK_SECTION * Section );
void BLEZ_Compare                  ( BLOCK_SECTION * Section );
void BLTZ_Compare                  ( BLOCK_SECTION * Section );
void BGEZ_Compare                  ( BLOCK_SECTION * Section );
void COP1_BCF_Compare              ( BLOCK_SECTION * Section );
void COP1_BCT_Compare              ( BLOCK_SECTION * Section );

/*************************  OpCode functions *************************/
void Compile_R4300i_J              ( BLOCK_SECTION * Section );
void Compile_R4300i_JAL            ( BLOCK_SECTION * Section );
void Compile_R4300i_ADDI           ( BLOCK_SECTION * Section );
void Compile_R4300i_ADDIU          ( BLOCK_SECTION * Section );
void Compile_R4300i_SLTI           ( BLOCK_SECTION * Section );
void Compile_R4300i_SLTIU          ( BLOCK_SECTION * Section );
void Compile_R4300i_ANDI           ( BLOCK_SECTION * Section );
void Compile_R4300i_ORI            ( BLOCK_SECTION * Section );
void Compile_R4300i_XORI           ( BLOCK_SECTION * Section );
void Compile_R4300i_LUI            ( BLOCK_SECTION * Section );
void Compile_R4300i_DADDIU         ( BLOCK_SECTION * Section );
void Compile_R4300i_LDL            ( BLOCK_SECTION * Section );
void Compile_R4300i_LDR            ( BLOCK_SECTION * Section );
void Compile_R4300i_LB             ( BLOCK_SECTION * Section );
void Compile_R4300i_LH             ( BLOCK_SECTION * Section );
void Compile_R4300i_LWL            ( BLOCK_SECTION * Section );
void Compile_R4300i_LW             ( BLOCK_SECTION * Section );
void Compile_R4300i_LBU            ( BLOCK_SECTION * Section );
void Compile_R4300i_LHU            ( BLOCK_SECTION * Section );
void Compile_R4300i_LWR            ( BLOCK_SECTION * Section );
void Compile_R4300i_LWU            ( BLOCK_SECTION * Section );		//added by Witten
void Compile_R4300i_SB             ( BLOCK_SECTION * Section );
void Compile_R4300i_SH             ( BLOCK_SECTION * Section );
void Compile_R4300i_SWL            ( BLOCK_SECTION * Section );
void Compile_R4300i_SW             ( BLOCK_SECTION * Section );
void Compile_R4300i_SWR            ( BLOCK_SECTION * Section );
void Compile_R4300i_SDL            ( BLOCK_SECTION * Section );
void Compile_R4300i_SDR            ( BLOCK_SECTION * Section );
void Compile_R4300i_CACHE          ( BLOCK_SECTION * Section );
void Compile_R4300i_LL             ( BLOCK_SECTION * Section );
void Compile_R4300i_LWC1           ( BLOCK_SECTION * Section );
void Compile_R4300i_LDC1           ( BLOCK_SECTION * Section );
void Compile_R4300i_LD             ( BLOCK_SECTION * Section );
void Compile_R4300i_SC             ( BLOCK_SECTION * Section );
void Compile_R4300i_SWC1           ( BLOCK_SECTION * Section );
void Compile_R4300i_SDC1           ( BLOCK_SECTION * Section );
void Compile_R4300i_SD             ( BLOCK_SECTION * Section );

/********************** R4300i OpCodes: Special **********************/
void Compile_R4300i_SPECIAL_SLL    ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_SRL    ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_SRA    ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_SLLV   ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_SRLV   ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_SRAV   ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_JR     ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_JALR   ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_SYSCALL( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_MFLO   ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_MTLO   ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_MFHI   ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_MTHI   ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_DSLLV  ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_DSRLV  ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_DSRAV  ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_MULT   ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_MULTU  ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_DIV    ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_DIVU   ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_DMULT  ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_DMULTU ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_DDIV   ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_DDIVU  ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_ADD    ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_ADDU   ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_SUB    ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_SUBU   ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_AND    ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_OR     ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_XOR    ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_NOR    ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_SLT    ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_SLTU   ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_DADD   ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_DADDU  ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_DSUB   ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_DSUBU  ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_DSLL   ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_DSRL   ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_DSRA   ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_DSLL32 ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_DSRL32 ( BLOCK_SECTION * Section );
void Compile_R4300i_SPECIAL_DSRA32 ( BLOCK_SECTION * Section );

/************************** COP0 functions **************************/
void Compile_R4300i_COP0_MF        ( BLOCK_SECTION * Section );
void Compile_R4300i_COP0_MT        ( BLOCK_SECTION * Section );

/************************** COP0 CO functions ***********************/
void Compile_R4300i_COP0_CO_TLBR   ( BLOCK_SECTION * Section );
void Compile_R4300i_COP0_CO_TLBWI  ( BLOCK_SECTION * Section );
void Compile_R4300i_COP0_CO_TLBWR  ( BLOCK_SECTION * Section );
void Compile_R4300i_COP0_CO_TLBP   ( BLOCK_SECTION * Section );
void Compile_R4300i_COP0_CO_ERET   ( BLOCK_SECTION * Section );

/************************** COP1 functions **************************/
void Compile_R4300i_COP1_MF        ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_DMF       ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_CF        ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_MT        ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_DMT       ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_CT        ( BLOCK_SECTION * Section );

/************************** COP1: S functions ************************/
void Compile_R4300i_COP1_S_ADD     ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_S_SUB     ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_S_MUL     ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_S_DIV     ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_S_ABS     ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_S_NEG     ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_S_SQRT    ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_S_MOV     ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_S_TRUNC_L ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_S_CEIL_L  ( BLOCK_SECTION * Section );			//added by Witten
void Compile_R4300i_COP1_S_FLOOR_L ( BLOCK_SECTION * Section );			//added by Witten
void Compile_R4300i_COP1_S_ROUND_W ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_S_TRUNC_W ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_S_CEIL_W  ( BLOCK_SECTION * Section );			//added by Witten
void Compile_R4300i_COP1_S_FLOOR_W ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_S_CVT_D   ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_S_CVT_W   ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_S_CVT_L   ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_S_CMP     ( BLOCK_SECTION * Section );

/************************** COP1: D functions ************************/
void Compile_R4300i_COP1_D_ADD     ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_D_SUB     ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_D_MUL     ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_D_DIV     ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_D_ABS     ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_D_NEG     ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_D_SQRT    ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_D_MOV     ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_D_TRUNC_L ( BLOCK_SECTION * Section );			//added by Witten
void Compile_R4300i_COP1_D_CEIL_L  ( BLOCK_SECTION * Section );			//added by Witten
void Compile_R4300i_COP1_D_FLOOR_L ( BLOCK_SECTION * Section );			//added by Witten
void Compile_R4300i_COP1_D_ROUND_W ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_D_TRUNC_W ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_D_CEIL_W  ( BLOCK_SECTION * Section );			//added by Witten
void Compile_R4300i_COP1_D_FLOOR_W ( BLOCK_SECTION * Section );			//added by Witten
void Compile_R4300i_COP1_D_CVT_S   ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_D_CVT_W   ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_D_CVT_L   ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_D_CMP     ( BLOCK_SECTION * Section );

/************************** COP1: W functions ************************/
void Compile_R4300i_COP1_W_CVT_S   ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_W_CVT_D   ( BLOCK_SECTION * Section );

/************************** COP1: L functions ************************/
void Compile_R4300i_COP1_L_CVT_S   ( BLOCK_SECTION * Section );
void Compile_R4300i_COP1_L_CVT_D   ( BLOCK_SECTION * Section );

/************************** Other functions **************************/
void Compile_R4300i_UnknownOpcode  ( BLOCK_SECTION * Section );
