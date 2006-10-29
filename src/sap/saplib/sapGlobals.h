#ifdef __MAIN_DECLARATIONS__
#define axeEXTERN
#else
#define axeEXTERN extern
#endif

#ifndef BYTE
#define BYTE unsigned char
#endif
#ifndef WORD
#define WORD unsigned short int
#endif
#ifndef DWORD
#define DWORD unsigned long int
#endif
#ifndef BOOL
#define BOOL int
#endif
#define TRUE 1
#define FALSE 0

axeEXTERN bool	isStereo;
axeEXTERN int	sampleStep;

extern void pokeyGenerateCheckIRQline(void);
extern void pokeyGenerateIRQ( BYTE irqMask );

axeEXTERN BYTE	atariMem[ 0x10000 ];
axeEXTERN WORD	sndBuf[16384];
axeEXTERN int	sndBufPtr;

axeEXTERN BYTE	ANTIC_VCOUNT_D40B;

axeEXTERN WORD cpuReg_PC;
axeEXTERN BYTE cpuFlag_N,cpuFlag_Z,cpuFlag_C,cpuFlag_D,cpuFlag_B,cpuFlag_I,cpuFlag_V,cpuReg_A,cpuReg_X,cpuReg_Y,cpuReg_S;
// cpuFlag_N is valid only in last bit-7
// cpuFlag_Z is set if whole cpuFlag_Z is not zero
// cpuFlag_C is valid only in first bit-0
// cpuFlag_D is valid only in first bit-0
// cpuFlag_B is valid only in first bit-0
// cpuFlag_I is valid only in first bit-0
// cpuFlag_V is valid only in first bit-0


extern void cpuInit( void );
extern int cpuExecuteOneOpcode( void );
extern BYTE cpuGetFlags( void );
extern void cpuSetFlags( BYTE flags );

extern void pokeyInit( void );
extern void pokeyReset( void );
extern BYTE pokeyReadByte( short unsigned int address );
extern void pokeyWriteByte0( short unsigned int address, BYTE value );
extern void pokeyWriteByte1( short unsigned int address, BYTE value );
extern void pokeyUpdateSound( int n );
extern void pokeyUpdateSoundCounters( void );

inline BYTE freddieReadByte( WORD ad )
{
	if( (ad&0xF800)==0xD000 )
	{
		if( (ad&0xFF00)==0xD200 )
		{
			return pokeyReadByte( ad );
		}
		if( (ad&0xFF0F)==0xD40B )
			return ANTIC_VCOUNT_D40B;
	}
	return atariMem[ad];
}
inline BYTE freddieCPUReadByte( WORD ad )
{
	return atariMem[ad];
}

inline void freddieWriteByte( WORD ad, BYTE val )
{
	if( (ad&0xFF00)==0xD200 )
	{
		if( ((ad&0x10)==0) || (isStereo==false) ) pokeyWriteByte0( ad, val );
		else pokeyWriteByte1( ad, val );
		return;
	}
	atariMem[ad] = val;
}

typedef int (*opcodeFunc)(bool &holded);

extern opcodeFunc opcodes_0x00_0xFF[256];
