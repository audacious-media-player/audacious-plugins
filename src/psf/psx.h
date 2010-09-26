#ifndef _MIPS_H
#define _MIPS_H

#include "ao.h"
//#include "driver.h"

typedef void genf(void);
typedef int offs_t;

#define cpu_readop32(pc) program_read_dword_32le(pc)
#define change_pc(pc)																	\


#ifdef __GNUC__
#if (__GNUC__ < 2) || ((__GNUC__ == 2) && (__GNUC_MINOR__ <= 7))
#define UNUSEDARG
#else
#define UNUSEDARG __attribute__((__unused__))
#endif
#else
#define UNUSEDARG
#endif

typedef int8		(*read8_handler)  (UNUSEDARG offs_t offset);
typedef void		(*write8_handler) (UNUSEDARG offs_t offset, UNUSEDARG int8 data);
typedef int16		(*read16_handler) (UNUSEDARG offs_t offset, UNUSEDARG int16 mem_mask);
typedef void		(*write16_handler)(UNUSEDARG offs_t offset, UNUSEDARG int16 data, UNUSEDARG int16 mem_mask);
typedef int32		(*read32_handler) (UNUSEDARG offs_t offset, UNUSEDARG int32 mem_mask);
typedef void		(*write32_handler)(UNUSEDARG offs_t offset, UNUSEDARG int32 data, UNUSEDARG int32 mem_mask);
typedef int64		(*read64_handler) (UNUSEDARG offs_t offset, UNUSEDARG int64 mem_mask);
typedef void		(*write64_handler)(UNUSEDARG offs_t offset, UNUSEDARG int64 data, UNUSEDARG int64 mem_mask);

union read_handlers_t
{
	genf *				handler;
	read8_handler		handler8;
	read16_handler		handler16;
	read32_handler		handler32;
	read64_handler		handler64;
};

union write_handlers_t
{
	genf *				handler;
	write8_handler		handler8;
	write16_handler		handler16;
	write32_handler		handler32;
	write64_handler		handler64;
};

struct address_map_t
{
	uint32				flags;				/* flags and additional info about this entry */
	offs_t				start, end;			/* start/end (or mask/match) values */
	offs_t				mirror;				/* mirror bits */
	offs_t				mask;				/* mask bits */
	union read_handlers_t read;				/* read handler callback */
	union write_handlers_t write;			/* write handler callback */
	void *				memory;				/* pointer to memory backing this entry */
	uint32				share;				/* index of a shared memory block */
	void **				base;				/* receives pointer to memory (optional) */
	size_t *			size;				/* receives size of area in bytes (optional) */
};
typedef struct address_map_t *(*construct_map_t)(struct address_map_t *map);

union cpuinfo
{
	int64	i;											/* generic integers */
	void *	p;											/* generic pointers */
	genf *  f;											/* generic function pointers */
	char *	s;											/* generic strings */

	void	(*setinfo)(UINT32 state, union cpuinfo *info);/* CPUINFO_PTR_SET_INFO */
	void	(*getcontext)(void *context);				/* CPUINFO_PTR_GET_CONTEXT */
	void	(*setcontext)(void *context);				/* CPUINFO_PTR_SET_CONTEXT */
	void	(*init)(void);								/* CPUINFO_PTR_INIT */
	void	(*reset)(void *param);						/* CPUINFO_PTR_RESET */
	void	(*exit)(void);								/* CPUINFO_PTR_EXIT */
	int		(*execute)(int cycles);						/* CPUINFO_PTR_EXECUTE */
	void	(*burn)(int cycles);						/* CPUINFO_PTR_BURN */
	offs_t	(*disassemble)(char *buffer, offs_t pc);	/* CPUINFO_PTR_DISASSEMBLE */
	int		(*irqcallback)(int state);					/* CPUINFO_PTR_IRQCALLBACK */
	int *	icount;										/* CPUINFO_PTR_INSTRUCTION_COUNTER */
	construct_map_t internal_map;						/* CPUINFO_PTR_INTERNAL_MEMORY_MAP */
};

enum
{
	MIPS_PC = 1,
	MIPS_DELAYV, MIPS_DELAYR,
	MIPS_HI, MIPS_LO,
	MIPS_R0, MIPS_R1,
	MIPS_R2, MIPS_R3,
	MIPS_R4, MIPS_R5,
	MIPS_R6, MIPS_R7,
	MIPS_R8, MIPS_R9,
	MIPS_R10, MIPS_R11,
	MIPS_R12, MIPS_R13,
	MIPS_R14, MIPS_R15,
	MIPS_R16, MIPS_R17,
	MIPS_R18, MIPS_R19,
	MIPS_R20, MIPS_R21,
	MIPS_R22, MIPS_R23,
	MIPS_R24, MIPS_R25,
	MIPS_R26, MIPS_R27,
	MIPS_R28, MIPS_R29,
	MIPS_R30, MIPS_R31,
	MIPS_CP0R0, MIPS_CP0R1,
	MIPS_CP0R2, MIPS_CP0R3,
	MIPS_CP0R4, MIPS_CP0R5,
	MIPS_CP0R6, MIPS_CP0R7,
	MIPS_CP0R8, MIPS_CP0R9,
	MIPS_CP0R10, MIPS_CP0R11,
	MIPS_CP0R12, MIPS_CP0R13,
	MIPS_CP0R14, MIPS_CP0R15,
	MIPS_CP0R16, MIPS_CP0R17,
	MIPS_CP0R18, MIPS_CP0R19,
	MIPS_CP0R20, MIPS_CP0R21,
	MIPS_CP0R22, MIPS_CP0R23,
	MIPS_CP0R24, MIPS_CP0R25,
	MIPS_CP0R26, MIPS_CP0R27,
	MIPS_CP0R28, MIPS_CP0R29,
	MIPS_CP0R30, MIPS_CP0R31,
	MIPS_CP2DR0, MIPS_CP2DR1,
	MIPS_CP2DR2, MIPS_CP2DR3,
	MIPS_CP2DR4, MIPS_CP2DR5,
	MIPS_CP2DR6, MIPS_CP2DR7,
	MIPS_CP2DR8, MIPS_CP2DR9,
	MIPS_CP2DR10, MIPS_CP2DR11,
	MIPS_CP2DR12, MIPS_CP2DR13,
	MIPS_CP2DR14, MIPS_CP2DR15,
	MIPS_CP2DR16, MIPS_CP2DR17,
	MIPS_CP2DR18, MIPS_CP2DR19,
	MIPS_CP2DR20, MIPS_CP2DR21,
	MIPS_CP2DR22, MIPS_CP2DR23,
	MIPS_CP2DR24, MIPS_CP2DR25,
	MIPS_CP2DR26, MIPS_CP2DR27,
	MIPS_CP2DR28, MIPS_CP2DR29,
	MIPS_CP2DR30, MIPS_CP2DR31,
	MIPS_CP2CR0, MIPS_CP2CR1,
	MIPS_CP2CR2, MIPS_CP2CR3,
	MIPS_CP2CR4, MIPS_CP2CR5,
	MIPS_CP2CR6, MIPS_CP2CR7,
	MIPS_CP2CR8, MIPS_CP2CR9,
	MIPS_CP2CR10, MIPS_CP2CR11,
	MIPS_CP2CR12, MIPS_CP2CR13,
	MIPS_CP2CR14, MIPS_CP2CR15,
	MIPS_CP2CR16, MIPS_CP2CR17,
	MIPS_CP2CR18, MIPS_CP2CR19,
	MIPS_CP2CR20, MIPS_CP2CR21,
	MIPS_CP2CR22, MIPS_CP2CR23,
	MIPS_CP2CR24, MIPS_CP2CR25,
	MIPS_CP2CR26, MIPS_CP2CR27,
	MIPS_CP2CR28, MIPS_CP2CR29,
	MIPS_CP2CR30, MIPS_CP2CR31
};

#define CLEAR_LINE	(0)
#define ASSERT_LINE	(1)

#define MIPS_INT_NONE	( -1 )

#define MIPS_IRQ0	( 0 )
#define MIPS_IRQ1	( 1 )
#define MIPS_IRQ2	( 2 )
#define MIPS_IRQ3	( 3 )
#define MIPS_IRQ4	( 4 )
#define MIPS_IRQ5	( 5 )

#define MIPS_BYTE_EXTEND( a ) ( (INT32)(INT8)a )
#define MIPS_WORD_EXTEND( a ) ( (INT32)(INT16)a )

#define INS_OP( op ) ( ( op >> 26 ) & 63 )
#define INS_RS( op ) ( ( op >> 21 ) & 31 )
#define INS_RT( op ) ( ( op >> 16 ) & 31 )
#define INS_IMMEDIATE( op ) ( op & 0xffff )
#define INS_TARGET( op ) ( op & 0x3ffffff )
#define INS_RD( op ) ( ( op >> 11 ) & 31 )
#define INS_SHAMT( op ) ( ( op >> 6 ) & 31 )
#define INS_FUNCT( op ) ( op & 63 )
#define INS_CODE( op ) ( ( op >> 6 ) & 0xfffff )
#define INS_CO( op ) ( ( op >> 25 ) & 1 )
#define INS_COFUN( op ) ( op & 0x1ffffff )
#define INS_CF( op ) ( op & 63 )

#define GTE_OP( op ) ( ( op >> 20 ) & 31 )
#define GTE_SF( op ) ( ( op >> 19 ) & 1 )
#define GTE_MX( op ) ( ( op >> 17 ) & 3 )
#define GTE_V( op ) ( ( op >> 15 ) & 3 )
#define GTE_CV( op ) ( ( op >> 13 ) & 3 )
#define GTE_CD( op ) ( ( op >> 11 ) & 3 ) /* not used */
#define GTE_LM( op ) ( ( op >> 10 ) & 1 )
#define GTE_CT( op ) ( ( op >> 6 ) & 15 ) /* not used */
#define GTE_FUNCT( op ) ( op & 63 )

#define OP_SPECIAL ( 0 )
#define OP_REGIMM ( 1 )
#define OP_J ( 2 )
#define OP_JAL ( 3 )
#define OP_BEQ ( 4 )
#define OP_BNE ( 5 )
#define OP_BLEZ ( 6 )
#define OP_BGTZ ( 7 )
#define OP_ADDI ( 8 )
#define OP_ADDIU ( 9 )
#define OP_SLTI ( 10 )
#define OP_SLTIU ( 11 )
#define OP_ANDI ( 12 )
#define OP_ORI ( 13 )
#define OP_XORI ( 14 )
#define OP_LUI ( 15 )
#define OP_COP0 ( 16 )
#define OP_COP1 ( 17 )
#define OP_COP2 ( 18 )
#define OP_LB ( 32 )
#define OP_LH ( 33 )
#define OP_LWL ( 34 )
#define OP_LW ( 35 )
#define OP_LBU ( 36 )
#define OP_LHU ( 37 )
#define OP_LWR ( 38 )
#define OP_SB ( 40 )
#define OP_SH ( 41 )
#define OP_SWL ( 42 )
#define OP_SW ( 43 )
#define OP_SWR ( 46 )
#define OP_LWC1 ( 49 )
#define OP_LWC2 ( 50 )
#define OP_SWC1 ( 57 )
#define OP_SWC2 ( 58 )

/* OP_SPECIAL */
#define FUNCT_SLL ( 0 )
#define FUNCT_SRL ( 2 )
#define FUNCT_SRA ( 3 )
#define FUNCT_SLLV ( 4 )
#define FUNCT_SRLV ( 6 )
#define FUNCT_SRAV ( 7 )
#define FUNCT_JR ( 8 )
#define FUNCT_JALR ( 9 )	   
#define FUNCT_HLECALL ( 11 )
#define FUNCT_SYSCALL ( 12 )
#define FUNCT_BREAK ( 13 )
#define FUNCT_MFHI ( 16 )
#define FUNCT_MTHI ( 17 )
#define FUNCT_MFLO ( 18 )
#define FUNCT_MTLO ( 19 )
#define FUNCT_MULT ( 24 )
#define FUNCT_MULTU ( 25 )
#define FUNCT_DIV ( 26 )
#define FUNCT_DIVU ( 27 )
#define FUNCT_ADD ( 32 )
#define FUNCT_ADDU ( 33 )
#define FUNCT_SUB ( 34 )
#define FUNCT_SUBU ( 35 )
#define FUNCT_AND ( 36 )
#define FUNCT_OR ( 37 )
#define FUNCT_XOR ( 38 )
#define FUNCT_NOR ( 39 )
#define FUNCT_SLT ( 42 )
#define FUNCT_SLTU ( 43 )

/* OP_REGIMM */
#define RT_BLTZ ( 0 )
#define RT_BGEZ ( 1 )
#define RT_BLTZAL ( 16 )
#define RT_BGEZAL ( 17 )

/* OP_COP0/OP_COP1/OP_COP2 */
#define RS_MFC ( 0 )
#define RS_CFC ( 2 )
#define RS_MTC ( 4 )
#define RS_CTC ( 6 )
#define RS_BC ( 8 )

/* RS_BC */
#define RT_BCF ( 0 )
#define RT_BCT ( 1 )

/* OP_COP0 */
#define CF_RFE ( 16 )

#ifdef MAME_DEBUG
extern unsigned DasmMIPS(char *buff, unsigned _pc);
#endif

#if (HAS_PSXCPU)
extern void psxcpu_get_info(UINT32 state, union cpuinfo *info);
#endif

#define MAX_REGS                64              /* maximum number of register of any CPU */

#define ADDRESS_SPACES                  3                                               /* maximum number of address spaces */
#define ADDRESS_SPACE_PROGRAM   0                                               /* program address space */
#define ADDRESS_SPACE_DATA              1                                               /* data address space */
#define ADDRESS_SPACE_IO                2                                               /* I/O address space */

enum
{
        /* internal flags (not for use by drivers!) */
        INTERNAL_CLEAR_LINE = 100 + CLEAR_LINE,
        INTERNAL_ASSERT_LINE = 100 + ASSERT_LINE,

        /* input lines */
        MAX_INPUT_LINES = 32+3,
        INPUT_LINE_IRQ0 = 0,
        INPUT_LINE_IRQ1 = 1,
        INPUT_LINE_IRQ2 = 2,
        INPUT_LINE_IRQ3 = 3,
        INPUT_LINE_IRQ4 = 4,
        INPUT_LINE_IRQ5 = 5,
        INPUT_LINE_IRQ6 = 6,
        INPUT_LINE_IRQ7 = 7,
        INPUT_LINE_IRQ8 = 8,
        INPUT_LINE_IRQ9 = 9,
        INPUT_LINE_NMI = MAX_INPUT_LINES - 3,

        /* special input lines that are implemented in the core */
        INPUT_LINE_RESET = MAX_INPUT_LINES - 2,
        INPUT_LINE_HALT = MAX_INPUT_LINES - 1,

        /* output lines */
        MAX_OUTPUT_LINES = 32
};

enum
{
	/* --- the following bits of info are returned as 64-bit signed integers --- */
	CPUINFO_INT_FIRST = 0x00000,

	CPUINFO_INT_CONTEXT_SIZE = CPUINFO_INT_FIRST,		/* R/O: size of CPU context in bytes */
	CPUINFO_INT_INPUT_LINES,							/* R/O: number of input lines */
	CPUINFO_INT_OUTPUT_LINES,							/* R/O: number of output lines */
	CPUINFO_INT_DEFAULT_IRQ_VECTOR,						/* R/O: default IRQ vector */
	CPUINFO_INT_ENDIANNESS,								/* R/O: either CPU_IS_BE or CPU_IS_LE */
	CPUINFO_INT_CLOCK_DIVIDER,							/* R/O: internal clock divider */
	CPUINFO_INT_MIN_INSTRUCTION_BYTES,					/* R/O: minimum bytes per instruction */
	CPUINFO_INT_MAX_INSTRUCTION_BYTES,					/* R/O: maximum bytes per instruction */
	CPUINFO_INT_MIN_CYCLES,								/* R/O: minimum cycles for a single instruction */
	CPUINFO_INT_MAX_CYCLES,								/* R/O: maximum cycles for a single instruction */

	CPUINFO_INT_DATABUS_WIDTH,							/* R/O: data bus size for each address space (8,16,32,64) */
	CPUINFO_INT_DATABUS_WIDTH_LAST = CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACES - 1,
	CPUINFO_INT_ADDRBUS_WIDTH,							/* R/O: address bus size for each address space (12-32) */
	CPUINFO_INT_ADDRBUS_WIDTH_LAST = CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACES - 1,
	CPUINFO_INT_ADDRBUS_SHIFT,							/* R/O: shift applied to addresses each address space (+3 means >>3, -1 means <<1) */
	CPUINFO_INT_ADDRBUS_SHIFT_LAST = CPUINFO_INT_ADDRBUS_SHIFT + ADDRESS_SPACES - 1,

	CPUINFO_INT_SP,										/* R/W: the current stack pointer value */
	CPUINFO_INT_PC,										/* R/W: the current PC value */
	CPUINFO_INT_PREVIOUSPC,								/* R/W: the previous PC value */
	CPUINFO_INT_INPUT_STATE,							/* R/W: states for each input line */
	CPUINFO_INT_INPUT_STATE_LAST = CPUINFO_INT_INPUT_STATE + MAX_INPUT_LINES - 1,
	CPUINFO_INT_OUTPUT_STATE,							/* R/W: states for each output line */
	CPUINFO_INT_OUTPUT_STATE_LAST = CPUINFO_INT_OUTPUT_STATE + MAX_OUTPUT_LINES - 1,
	CPUINFO_INT_REGISTER,								/* R/W: values of up to MAX_REGs registers */
	CPUINFO_INT_REGISTER_LAST = CPUINFO_INT_REGISTER + MAX_REGS - 1,

	CPUINFO_INT_CPU_SPECIFIC = 0x08000,					/* R/W: CPU-specific values start here */

	/* --- the following bits of info are returned as pointers to data or functions --- */
	CPUINFO_PTR_FIRST = 0x10000,

	CPUINFO_PTR_SET_INFO = CPUINFO_PTR_FIRST,			/* R/O: void (*set_info)(UINT32 state, INT64 data, void *ptr) */
	CPUINFO_PTR_GET_CONTEXT,							/* R/O: void (*get_context)(void *buffer) */
	CPUINFO_PTR_SET_CONTEXT,							/* R/O: void (*set_context)(void *buffer) */
	CPUINFO_PTR_INIT,									/* R/O: void (*init)(void) */
	CPUINFO_PTR_RESET,									/* R/O: void (*reset)(void *param) */
	CPUINFO_PTR_EXIT,									/* R/O: void (*exit)(void) */
	CPUINFO_PTR_EXECUTE,								/* R/O: int (*execute)(int cycles) */
	CPUINFO_PTR_BURN,									/* R/O: void (*burn)(int cycles) */
	CPUINFO_PTR_DISASSEMBLE,							/* R/O: void (*disassemble)(char *buffer, offs_t pc) */
	CPUINFO_PTR_IRQ_CALLBACK,							/* R/W: int (*irqcallback)(int state) */
	CPUINFO_PTR_INSTRUCTION_COUNTER,					/* R/O: int *icount */
	CPUINFO_PTR_REGISTER_LAYOUT,						/* R/O: struct debug_register_layout *layout */
	CPUINFO_PTR_WINDOW_LAYOUT,							/* R/O: struct debug_window_layout *layout */
	CPUINFO_PTR_INTERNAL_MEMORY_MAP,					/* R/O: construct_map_t map */
	CPUINFO_PTR_INTERNAL_MEMORY_MAP_LAST = CPUINFO_PTR_INTERNAL_MEMORY_MAP + ADDRESS_SPACES - 1,
	CPUINFO_PTR_DEBUG_REGISTER_LIST,					/* R/O: int *list: list of registers for NEW_DEBUGGER */

	CPUINFO_PTR_CPU_SPECIFIC = 0x18000,					/* R/W: CPU-specific values start here */

	/* --- the following bits of info are returned as NULL-terminated strings --- */
	CPUINFO_STR_FIRST = 0x20000,

	CPUINFO_STR_NAME = CPUINFO_STR_FIRST,				/* R/O: name of the CPU */
	CPUINFO_STR_CORE_FAMILY,							/* R/O: family of the CPU */
	CPUINFO_STR_CORE_VERSION,							/* R/O: version of the CPU core */
	CPUINFO_STR_CORE_FILE,								/* R/O: file containing the CPU core */
	CPUINFO_STR_CORE_CREDITS,							/* R/O: credits for the CPU core */
	CPUINFO_STR_FLAGS,									/* R/O: string representation of the main flags value */
	CPUINFO_STR_REGISTER,								/* R/O: string representation of up to MAX_REGs registers */
	CPUINFO_STR_REGISTER_LAST = CPUINFO_STR_REGISTER + MAX_REGS - 1,

	CPUINFO_STR_CPU_SPECIFIC = 0x28000					/* R/W: CPU-specific values start here */
};

#endif
