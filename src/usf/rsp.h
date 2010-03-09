#ifndef _RSP_H_
#define _RSP_H_

#include <stdint.h>
#include "types.h"

#define CPU_Message(...)
//#define DisplayError printf

#pragma pack(push,1)

typedef struct tagOPCODE {
	union {

		uint32_t Hex;
		uint8_t Ascii[4];

		struct {
			unsigned offset : 16;
			unsigned rt : 5;
			unsigned rs : 5;
			unsigned op : 6;
		};

		struct {
			unsigned immediate : 16;
			unsigned : 5;
			unsigned base : 5;
			unsigned : 6;
		};

		struct {
			unsigned target : 26;
			unsigned : 6;
		};

		struct {
			unsigned funct : 6;
			unsigned sa : 5;
			unsigned rd : 5;
			unsigned : 5;
			unsigned : 5;
			unsigned : 6;
		};

		struct {
			signed   voffset : 7;
			unsigned del    : 4;
			unsigned : 5;
			unsigned dest   : 5;
			unsigned : 5;
			unsigned : 6;
		};

	};
} RSPOPCODE;

#pragma pack(pop)

#define REGISTER32	MIPS_WORD
#define REGISTER	MIPS_DWORD


//RSP OpCodes
#define	RSP_SPECIAL				 0
#define	RSP_REGIMM				 1
#define RSP_J					 2
#define RSP_JAL					 3
#define RSP_BEQ					 4
#define RSP_BNE					 5
#define RSP_BLEZ				 6
#define RSP_BGTZ				 7
#define RSP_ADDI				 8
#define RSP_ADDIU				 9
#define RSP_SLTI				10
#define RSP_SLTIU				11
#define RSP_ANDI				12
#define RSP_ORI					13
#define RSP_XORI				14
#define RSP_LUI					15
#define	RSP_CP0					16
#define	RSP_CP2					18
#define RSP_LB					32
#define RSP_LH					33
#define RSP_LW					35
#define RSP_LBU					36
#define RSP_LHU					37
#define RSP_SB					40
#define RSP_SH					41
#define RSP_SW					43
#define RSP_LC2					50
#define RSP_SC2					58

/* RSP Special opcodes */
#define RSP_SPECIAL_SLL			 0
#define RSP_SPECIAL_SRL			 2
#define RSP_SPECIAL_SRA			 3
#define RSP_SPECIAL_SLLV		 4
#define RSP_SPECIAL_SRLV		 6
#define RSP_SPECIAL_SRAV		 7
#define RSP_SPECIAL_JR			 8
#define RSP_SPECIAL_JALR		 9
#define RSP_SPECIAL_BREAK		13
#define RSP_SPECIAL_ADD			32
#define RSP_SPECIAL_ADDU		33
#define RSP_SPECIAL_SUB			34
#define RSP_SPECIAL_SUBU		35
#define RSP_SPECIAL_AND			36
#define RSP_SPECIAL_OR			37
#define RSP_SPECIAL_XOR			38
#define RSP_SPECIAL_NOR			39
#define RSP_SPECIAL_SLT			42
#define RSP_SPECIAL_SLTU		43

/* RSP RegImm opcodes */
#define RSP_REGIMM_BLTZ			 0
#define RSP_REGIMM_BGEZ			 1
#define RSP_REGIMM_BLTZAL		16
#define RSP_REGIMM_BGEZAL		17

/* RSP COP0 opcodes */
#define	RSP_COP0_MF				 0
#define	RSP_COP0_MT				 4

/* RSP COP2 opcodes */
#define	RSP_COP2_MF				 0
#define	RSP_COP2_CF				 2
#define	RSP_COP2_MT				 4
#define	RSP_COP2_CT				 6

/* RSP Vector opcodes */
#define	RSP_VECTOR_VMULF		 0
#define	RSP_VECTOR_VMULU		 1
#define	RSP_VECTOR_VRNDP		 2
#define	RSP_VECTOR_VMULQ		 3
#define	RSP_VECTOR_VMUDL		 4
#define	RSP_VECTOR_VMUDM		 5
#define	RSP_VECTOR_VMUDN		 6
#define	RSP_VECTOR_VMUDH		 7
#define	RSP_VECTOR_VMACF		 8
#define	RSP_VECTOR_VMACU		 9
#define	RSP_VECTOR_VRNDN		10
#define	RSP_VECTOR_VMACQ		11
#define	RSP_VECTOR_VMADL		12
#define	RSP_VECTOR_VMADM		13
#define	RSP_VECTOR_VMADN		14
#define	RSP_VECTOR_VMADH		15
#define	RSP_VECTOR_VADD			16
#define	RSP_VECTOR_VSUB			17
#define	RSP_VECTOR_VABS			19
#define	RSP_VECTOR_VADDC		20
#define	RSP_VECTOR_VSUBC		21
#define	RSP_VECTOR_VSAW			29
#define	RSP_VECTOR_VLT			32
#define	RSP_VECTOR_VEQ			33
#define	RSP_VECTOR_VNE			34
#define	RSP_VECTOR_VGE			35
#define	RSP_VECTOR_VCL			36
#define	RSP_VECTOR_VCH			37
#define	RSP_VECTOR_VCR			38
#define	RSP_VECTOR_VMRG			39
#define	RSP_VECTOR_VAND			40
#define	RSP_VECTOR_VNAND		41
#define	RSP_VECTOR_VOR			42
#define	RSP_VECTOR_VNOR			43
#define	RSP_VECTOR_VXOR			44
#define	RSP_VECTOR_VNXOR		45
#define	RSP_VECTOR_VRCP			48
#define	RSP_VECTOR_VRCPL		49
#define	RSP_VECTOR_VRCPH		50
#define	RSP_VECTOR_VMOV			51
#define	RSP_VECTOR_VRSQ			52
#define	RSP_VECTOR_VRSQL		53
#define	RSP_VECTOR_VRSQH		54
#define	RSP_VECTOR_VNOOP		55

/* RSP LSC2 opcodes */
#define RSP_LSC2_BV				 0
#define RSP_LSC2_SV				 1
#define RSP_LSC2_LV				 2
#define RSP_LSC2_DV				 3
#define RSP_LSC2_QV				 4
#define RSP_LSC2_RV				 5
#define RSP_LSC2_PV				 6
#define RSP_LSC2_UV				 7
#define RSP_LSC2_HV				 8
#define RSP_LSC2_FV				 9
#define RSP_LSC2_WV				10
#define	RSP_LSC2_TV				11


/************************* OpCode functions *************************/
void RSP_Opcode_SPECIAL ( void );
void RSP_Opcode_REGIMM  ( void );
void RSP_Opcode_J       ( void );
void RSP_Opcode_JAL     ( void );
void RSP_Opcode_BEQ     ( void );
void RSP_Opcode_BNE     ( void );
void RSP_Opcode_BLEZ    ( void );
void RSP_Opcode_BGTZ    ( void );
void RSP_Opcode_ADDI    ( void );
void RSP_Opcode_ADDIU   ( void );
void RSP_Opcode_SLTI    ( void );
void RSP_Opcode_SLTIU   ( void );
void RSP_Opcode_ANDI    ( void );
void RSP_Opcode_ORI     ( void );
void RSP_Opcode_XORI    ( void );
void RSP_Opcode_LUI     ( void );
void RSP_Opcode_COP0    ( void );
void RSP_Opcode_COP2    ( void );
void RSP_Opcode_LB      ( void );
void RSP_Opcode_LH      ( void );
void RSP_Opcode_LW      ( void );
void RSP_Opcode_LBU     ( void );
void RSP_Opcode_LHU     ( void );
void RSP_Opcode_SB      ( void );
void RSP_Opcode_SH      ( void );
void RSP_Opcode_SW      ( void );
void RSP_Opcode_LC2     ( void );
void RSP_Opcode_SC2     ( void );
/********************** R4300i OpCodes: Special **********************/
void RSP_Special_SLL    ( void );
void RSP_Special_SRL    ( void );
void RSP_Special_SRA    ( void );
void RSP_Special_SLLV   ( void );
void RSP_Special_SRLV   ( void );
void RSP_Special_SRAV   ( void );
void RSP_Special_JR     ( void );
void RSP_Special_JALR   ( void );
void RSP_Special_BREAK  ( void );
void RSP_Special_ADD    ( void );
void RSP_Special_ADDU   ( void );
void RSP_Special_SUB    ( void );
void RSP_Special_SUBU   ( void );
void RSP_Special_AND    ( void );
void RSP_Special_OR     ( void );
void RSP_Special_XOR    ( void );
void RSP_Special_NOR    ( void );
void RSP_Special_SLT    ( void );
void RSP_Special_SLTU   ( void );
/********************** R4300i OpCodes: RegImm **********************/
void RSP_Opcode_BLTZ    ( void );
void RSP_Opcode_BGEZ    ( void );
void RSP_Opcode_BLTZAL  ( void );
void RSP_Opcode_BGEZAL  ( void );
/************************** Cop0 functions *************************/
void RSP_Cop0_MF        ( void );
void RSP_Cop0_MT        ( void );
/************************** Cop2 functions *************************/
void RSP_Cop2_MF        ( void );
void RSP_Cop2_CF        ( void );
void RSP_Cop2_MT        ( void );
void RSP_Cop2_CT        ( void );
void RSP_COP2_VECTOR    ( void );
/************************** Vect functions **************************/
void RSP_Vector_VMULF   ( void );
void RSP_Vector_VMULU	( void );
void RSP_Vector_VMUDL   ( void );
void RSP_Vector_VMUDM   ( void );
void RSP_Vector_VMUDN   ( void );
void RSP_Vector_VMUDH   ( void );
void RSP_Vector_VMACF   ( void );
void RSP_Vector_VMACU   ( void );
void RSP_Vector_VMACQ   ( void );
void RSP_Vector_VMADL   ( void );
void RSP_Vector_VMADM   ( void );
void RSP_Vector_VMADN   ( void );
void RSP_Vector_VMADH   ( void );
void RSP_Vector_VADD    ( void );
void RSP_Vector_VSUB    ( void );
void RSP_Vector_VABS    ( void );
void RSP_Vector_VADDC   ( void );
void RSP_Vector_VSUBC   ( void );
void RSP_Vector_VSAW    ( void );
void RSP_Vector_VLT     ( void );
void RSP_Vector_VEQ     ( void );
void RSP_Vector_VNE     ( void );
void RSP_Vector_VGE     ( void );
void RSP_Vector_VCL     ( void );
void RSP_Vector_VCH     ( void );
void RSP_Vector_VCR     ( void );
void RSP_Vector_VMRG    ( void );
void RSP_Vector_VAND    ( void );
void RSP_Vector_VNAND   ( void );
void RSP_Vector_VOR     ( void );
void RSP_Vector_VNOR    ( void );
void RSP_Vector_VXOR    ( void );
void RSP_Vector_VNXOR   ( void );
void RSP_Vector_VRCP    ( void );
void RSP_Vector_VRCPL   ( void );
void RSP_Vector_VRCPH   ( void );
void RSP_Vector_VMOV    ( void );
void RSP_Vector_VRSQ    ( void );
void RSP_Vector_VRSQL   ( void );
void RSP_Vector_VRSQH   ( void );
void RSP_Vector_VNOOP   ( void );
/************************** lc2 functions **************************/
void RSP_Opcode_LBV     ( void );
void RSP_Opcode_LSV     ( void );
void RSP_Opcode_LLV     ( void );
void RSP_Opcode_LDV     ( void );
void RSP_Opcode_LQV     ( void );
void RSP_Opcode_LRV     ( void );
void RSP_Opcode_LPV     ( void );
void RSP_Opcode_LUV     ( void );
void RSP_Opcode_LHV     ( void );
void RSP_Opcode_LFV     ( void );
void RSP_Opcode_LTV     ( void );
/************************** lc2 functions **************************/
void RSP_Opcode_SBV     ( void );
void RSP_Opcode_SSV     ( void );
void RSP_Opcode_SLV     ( void );
void RSP_Opcode_SDV     ( void );
void RSP_Opcode_SQV     ( void );
void RSP_Opcode_SRV     ( void );
void RSP_Opcode_SPV     ( void );
void RSP_Opcode_SUV     ( void );
void RSP_Opcode_SHV     ( void );
void RSP_Opcode_SFV     ( void );
void RSP_Opcode_STV     ( void );
void RSP_Opcode_SWV     ( void );
/************************** Other functions **************************/
void rsp_UnknownOpcode  ( void );

extern void (*RSP_Opcode[64])();
extern void (*RSP_RegImm[32])();
extern void (*RSP_Special[64])();
extern void (*RSP_Cop0[32])();
extern void (*RSP_Cop2[32])();
extern void (*RSP_Vector[64])();
extern void (*RSP_Lc2[32])();
extern void (*RSP_Sc2[32])();


void RSP_SP_DMA_READ (void);
void RSP_SP_DMA_WRITE(void);

extern REGISTER32 *RSP_GPR, RSP_Flags[4];
extern REGISTER *RSP_ACCUM;
extern VECTOR *RSP_Vect;

extern REGISTER EleSpec[32], Indx[32];
extern RSPOPCODE RSPOpC;
extern uint32_t *PrgCount, RSPNextInstruction;
extern uint32_t RSP_NextInstruction;
extern REGISTER32 Recp, RecpResult, SQroot, SQrootResult;

extern uint32_t RSP_NextInstruction, RSP_JumpTo;


extern uint32_t RSP_Running;


//extern uint8_t * RSPRecompPos, * RSPRecompCode, * RSPRecompCodeSecondary;
//extern uint8_t * RSPJumpTable;
#define MaxMaps	32

extern uint32_t NoOfMaps, MapsCRC[MaxMaps], Table, ConditionalMove;
extern uint8_t * RSPRecompCode, * RSPRecompCodeSecondary, * RSPRecompPos, *RSPJumpTables;
extern void ** RSPJumpTable;


#define NORMAL					0
#define DO_DELAY_SLOT			1
#define DO_END_DELAY_SLOT		2
#define DELAY_SLOT				3
#define END_DELAY_SLOT			4
#define LIKELY_DELAY_SLOT		5
#define JUMP	 				6
#define DELAY_SLOT_DONE			7
#define LIKELY_DELAY_SLOT_DONE	8
#define END_BLOCK 				9
#define FINISH_BLOCK			10 // from RSP Recompiler CPU
#define FINISH_SUB_BLOCK		11 // from RSP Recompiler CPU

void real_run_rsp(uint32_t cycles);
int32_t init_rsp(void);


#endif
