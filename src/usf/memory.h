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
#define LargeCompileBufferSize	0x03200000
#define NormalCompileBufferSize	0x01500000

#define RSP_RECOMPMEM_SIZE		0x400000
#define RSP_SECRECOMPMEM_SIZE	0x200000

#define ROM_IN_MAPSPACE

extern uint32_t RdramSize, SystemRdramSize, RomFileSize;
extern uintptr_t *TLB_Map;
extern uint8_t * MemChunk;

extern uint8_t *N64MEM, *RDRAM, *DMEM, *IMEM, * ROMPages[0x400], *savestatespace, * NOMEM;
extern void ** JumpTable, ** DelaySlotTable;
extern uint8_t *RecompCode, *RecompPos;
extern uint32_t WrittenToRom, MemoryState;

/* Memory Control */
int  Allocate_ROM                ( void );
int  Allocate_Memory             ( void );
void Release_Memory              ( void );
int PreAllocate_Memory(void);

void *malloc_exec(uint32_t bytes);
void *jmalloc(uint32_t bytes);

/* CPU memory functions */
//int  r4300i_Command_MemoryFilter ( uint32_t dwExptCode, LPEXCEPTION_POINTERS lpEP );
//int  r4300i_CPU_MemoryFilter     ( uint32_t dwExptCode, LPEXCEPTION_POINTERS lpEP );
int32_t  r4300i_LB_NonMemory         ( uint32_t PAddr, uint32_t * Value, uint32_t SignExtend );
uint32_t r4300i_LB_VAddr             ( uint32_t VAddr, uint8_t * Value );
uint32_t r4300i_LD_VAddr             ( uint32_t VAddr, uint64_t * Value );
int32_t  r4300i_LH_NonMemory         ( uint32_t PAddr, uint32_t * Value, int32_t SignExtend );
uint32_t r4300i_LH_VAddr             ( uint32_t VAddr, uint16_t * Value );
int32_t  r4300i_LW_NonMemory         ( uint32_t PAddr, uint32_t * Value );
void r4300i_LW_PAddr             ( uint32_t PAddr, uint32_t * Value );
uint32_t r4300i_LW_VAddr             ( uint32_t VAddr, uint32_t * Value );
int32_t  r4300i_SB_NonMemory         ( uint32_t PAddr, uint8_t Value );
uint32_t r4300i_SB_VAddr             ( uint32_t VAddr, uint8_t Value );
uint32_t r4300i_SD_VAddr            ( uint32_t VAddr, uint64_t Value );
int32_t  r4300i_SH_NonMemory         ( uint32_t PAddr, uint16_t Value );
uint32_t r4300i_SH_VAddr             ( uint32_t VAddr, uint16_t Value );
int32_t  r4300i_SW_NonMemory         ( uint32_t PAddr, uint32_t Value );
uint32_t r4300i_SW_VAddr             ( uint32_t VAddr, uint32_t Value );

/* Recompiler Memory Functions */
void Compile_LB                  ( int32_t Reg, uint32_t Addr, uint32_t SignExtend );
void Compile_LH                  ( int32_t Reg, uint32_t Addr, uint32_t SignExtend );
void Compile_LW                  ( int32_t Reg, uint32_t Addr );
void Compile_SB_Const            ( uint8_t Value, uint32_t Addr );
void Compile_SB_Register         ( int32_t x86Reg, uint32_t Addr );
void Compile_SH_Const            ( uint16_t Value, uint32_t Addr );
void Compile_SH_Register         ( int32_t x86Reg, uint32_t Addr );
void Compile_SW_Const            ( uint32_t Value, uint32_t Addr );
void Compile_SW_Register         ( int32_t x86Reg, uint32_t Addr );
void ResetRecompCode             ( void );

uint8_t * PageROM(uint32_t addr);
