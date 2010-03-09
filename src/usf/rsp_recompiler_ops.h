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

/************************* OpCode functions *************************/
void RSPCompile_SPECIAL       ( void );
void RSPCompile_REGIMM        ( void );
void RSPCompile_J             ( void );
void RSPCompile_JAL           ( void );
void RSPCompile_BEQ           ( void );
void RSPCompile_BNE           ( void );
void RSPCompile_BLEZ          ( void );
void RSPCompile_BGTZ          ( void );
void RSPCompile_ADDI          ( void );
void RSPCompile_ADDIU         ( void );
void RSPCompile_SLTI          ( void );
void RSPCompile_SLTIU         ( void );
void RSPCompile_ANDI          ( void );
void RSPCompile_ORI           ( void );
void RSPCompile_XORI          ( void );
void RSPCompile_LUI           ( void );
void RSPCompile_COP0          ( void );
void RSPCompile_COP2          ( void );
void RSPCompile_LB            ( void );
void RSPCompile_LH            ( void );
void RSPCompile_LW            ( void );
void RSPCompile_LBU           ( void );
void RSPCompile_LHU           ( void );
void RSPCompile_SB            ( void );
void RSPCompile_SH            ( void );
void RSPCompile_SW            ( void );
void RSPCompile_LC2           ( void );
void RSPCompile_SC2           ( void );
/********************** R4300i OpCodes: Special **********************/
void RSPCompile_Special_SLL   ( void );
void RSPCompile_Special_SRL   ( void );
void RSPCompile_Special_SRA   ( void );
void RSPCompile_Special_SLLV  ( void );
void RSPCompile_Special_SRLV  ( void );
void RSPCompile_Special_SRAV  ( void );
void RSPCompile_Special_JR    ( void );
void RSPCompile_Special_JALR  ( void );
void RSPCompile_Special_BREAK ( void );
void RSPCompile_Special_ADD   ( void );
void RSPCompile_Special_ADDU  ( void );
void RSPCompile_Special_SUB   ( void );
void RSPCompile_Special_SUBU  ( void );
void RSPCompile_Special_AND   ( void );
void RSPCompile_Special_OR    ( void );
void RSPCompile_Special_XOR   ( void );
void RSPCompile_Special_NOR   ( void );
void RSPCompile_Special_SLT   ( void );
void RSPCompile_Special_SLTU  ( void );
/********************** R4300i OpCodes: RegImm **********************/
void RSPCompile_RegImm_BLTZ   ( void );
void RSPCompile_RegImm_BGEZ   ( void );
void RSPCompile_RegImm_BLTZAL ( void );
void RSPCompile_RegImm_BGEZAL ( void );
/************************** Cop0 functions *************************/
void RSPCompile_Cop0_MF       ( void );
void RSPCompile_Cop0_MT       ( void );
/************************** Cop2 functions *************************/
void RSPCompile_Cop2_MF       ( void );
void RSPCompile_Cop2_CF       ( void );
void RSPCompile_Cop2_MT       ( void );
void RSPCompile_Cop2_CT       ( void );
void RSPCompile_COP2_VECTOR  ( void );
/************************** Vect functions **************************/
void RSPCompile_Vector_VMULF  ( void );
void RSPCompile_Vector_VMULU  ( void );
void RSPCompile_Vector_VMUDL  ( void );
void RSPCompile_Vector_VMUDM  ( void );
void RSPCompile_Vector_VMUDN  ( void );
void RSPCompile_Vector_VMUDH  ( void );
void RSPCompile_Vector_VMACF  ( void );
void RSPCompile_Vector_VMACU  ( void );
void RSPCompile_Vector_VMACQ  ( void );
void RSPCompile_Vector_VMADL  ( void );
void RSPCompile_Vector_VMADM  ( void );
void RSPCompile_Vector_VMADN  ( void );
void RSPCompile_Vector_VMADH  ( void );
void RSPCompile_Vector_VADD   ( void );
void RSPCompile_Vector_VSUB   ( void );
void RSPCompile_Vector_VABS   ( void );
void RSPCompile_Vector_VADDC  ( void );
void RSPCompile_Vector_VSUBC  ( void );
void RSPCompile_Vector_VSAW   ( void );
void RSPCompile_Vector_VLT    ( void );
void RSPCompile_Vector_VEQ    ( void );
void RSPCompile_Vector_VNE    ( void );
void RSPCompile_Vector_VGE    ( void );
void RSPCompile_Vector_VCL    ( void );
void RSPCompile_Vector_VCH    ( void );
void RSPCompile_Vector_VCR    ( void );
void RSPCompile_Vector_VMRG   ( void );
void RSPCompile_Vector_VAND   ( void );
void RSPCompile_Vector_VNAND  ( void );
void RSPCompile_Vector_VOR    ( void );
void RSPCompile_Vector_VNOR   ( void );
void RSPCompile_Vector_VXOR   ( void );
void RSPCompile_Vector_VNXOR  ( void );
void RSPCompile_Vector_VRCP   ( void );
void RSPCompile_Vector_VRCPL  ( void );
void RSPCompile_Vector_VRCPH  ( void );
void RSPCompile_Vector_VMOV   ( void );
void RSPCompile_Vector_VRSQ   ( void );
void RSPCompile_Vector_VRSQL  ( void );
void RSPCompile_Vector_VRSQH  ( void );
void RSPCompile_Vector_VNOOP  ( void );
/************************** lc2 functions **************************/
void RSPCompile_Opcode_LBV    ( void );
void RSPCompile_Opcode_LSV    ( void );
void RSPCompile_Opcode_LLV    ( void );
void RSPCompile_Opcode_LDV    ( void );
void RSPCompile_Opcode_LQV    ( void );
void RSPCompile_Opcode_LRV    ( void );
void RSPCompile_Opcode_LPV    ( void );
void RSPCompile_Opcode_LUV    ( void );
void RSPCompile_Opcode_LHV    ( void );
void RSPCompile_Opcode_LFV    ( void );
void RSPCompile_Opcode_LTV    ( void );
/************************** sc2 functions **************************/
void RSPCompile_Opcode_SBV    ( void );
void RSPCompile_Opcode_SSV    ( void );
void RSPCompile_Opcode_SLV    ( void );
void RSPCompile_Opcode_SDV    ( void );
void RSPCompile_Opcode_SQV    ( void );
void RSPCompile_Opcode_SRV    ( void );
void RSPCompile_Opcode_SPV    ( void );
void RSPCompile_Opcode_SUV    ( void );
void RSPCompile_Opcode_SHV    ( void );
void RSPCompile_Opcode_SFV    ( void );
void RSPCompile_Opcode_SWV    ( void );
void RSPCompile_Opcode_STV    ( void );
/************************** Other functions **************************/
void RSPCompile_UnknownOpcode (void);
