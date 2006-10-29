// This is very funny piece of code :)). This is a "fake" pokey template.

namespace NAMESPACENAME {

BYTE IRQ_EN;
BYTE IRQ_ST;
BYTE IRQ_line;
bool generateIRQ0;

const double cutFreq = 24000.0 / 44100.0; // cutoff frequency = 20000, sampling frequency = 44100
const int samplerRate = 0x2836D6; // (1773447 / 44100) * 65536;
const int samplerRateOver = 0xA0DB5/2; // (1773447 / 176400) * 65536;

//const double cutFreq2 = 26000.0 / 1773447.0; // cutoff frequency = 20000, sampling frequency = 44100
const double cutFreq2 = 28000.0 / 176400; // cutoff frequency = 20000, sampling frequency = 44100

const int cutFreq2i = 650;

#define MAKESAMPLE\
	{\
		ss-=0x10000;\
		if( ss<0 )\
		{\
			ss += samplerRateOver;\
			DWORD pomd; int pomi;\
			pomd  = ((DWORD*)&switch_J3_Q_state[0])[0];\
			pomd &= ((DWORD*)&switch_J3_Q_stateAND[0])[0];\
			pomd ^= ((DWORD*)&signal_state_out[0])[0];\
			pomd &= ((DWORD*)&freq_sequre[0])[0];\
			pomd |= ((DWORD*)&audioControl_Latch_Digi[0])[0];\
			pomd &= ((DWORD*)&audioControl_Latch2[0])[0];\
			pomd = pomd + (pomd>>16);\
			pomi = (int)( pomd + (pomd>>8) ) & 255;\
			oldValI = oldValI + ((cutFreq2i*((pomi<<12) - oldValI))>>12);\
			delay++; if( (delay&7)==0 )\
			{\
				pomi = oldValI>>3;\
				if( pomi<-16384 ) pomi = -16384; else if( pomi>32767 ) pomi = 32767;\
				sndBuf[sndBufPtr] = (WORD)pomi;\
				sndBufPtr = (sndBufPtr+sampleStep)&16383;\
			}\
		}\
	}

int sndBufPtrUpp;

int oldValI = 0;
double oldVal=0.0;
int delay = 0;

BYTE AUDCTL;
int pcc1564,noiseAND,pokeyClockCounter64k;

DWORD pokeyClockCounter;
DWORD poly4Counter,poly5Counter,poly4_5Counter;

BYTE poly17[0x20000];
BYTE poly4_b[36000]; // minimum size is 312*114+15
BYTE poly5_b[36000]; // minimum size is 312*114+31
BYTE poly4_5_b[37000]; // minimum size is 312*114+465

BYTE poly4[15] = {
15, 15, 15, 15, 0, 0, 0, 15, 0, 0, 15, 15, 0, 15, 0
};

BYTE poly5[31] = {
15, 15, 15, 15, 0, 15, 15, 0, 15, 0, 0, 15, 15, 0, 0, 0, 0, 0, 15, 15, 15, 0, 0, 15, 0, 0, 0, 15, 0, 15, 0
};

int divideByN[4];
int divideByN_Latch[4];
int divideByN_Latch2[4];
BYTE switch_J2_signal_Q[4];
BYTE signal_state_out[4];
BYTE switch_J3_Q_state[4];
BYTE switch_J3_Q_stateAND[4];
BYTE audioControl_Latch[4];
BYTE audioControl_Latch2[4];
BYTE audioControl_Latch_Digi[4];
BYTE freq_sequre[4];

void (*Channel0Distortion)(void);
void (*Channel1Distortion)(void);
void (*Channel2Distortion)(void);
void (*Channel3Distortion)(void);

//---
void channel0_0( void )
{
	if( poly5_b[ poly5Counter + pokeyClockCounter ]!=0 )
		signal_state_out[0] = poly17[ pokeyClockCounter & noiseAND ];
}
void channel0_2( void )
{
	signal_state_out[0] = signal_state_out[0] ^ poly5_b[ poly5Counter + pokeyClockCounter ];
}
void channel0_4( void )
{
	if( poly5_b[ poly5Counter + pokeyClockCounter ]!=0 )
		signal_state_out[0] = poly4_b[ poly4Counter + pokeyClockCounter ];
}
void channel0_8( void )
{
	signal_state_out[0] = poly17[ pokeyClockCounter & noiseAND ];
}
void channel0_A( void )
{
	signal_state_out[0] = signal_state_out[0] ^ 15;
}
void channel0_C( void )
{
	signal_state_out[0] = poly4_b[ pokeyClockCounter + poly4Counter ];
}
//---
void channel1_0( void )
{
	if( poly5_b[ poly5Counter + pokeyClockCounter ]!=0 )
		signal_state_out[1] = poly17[ pokeyClockCounter & noiseAND ];
}
void channel1_2( void )
{
	signal_state_out[1] = signal_state_out[1] ^ poly5_b[ poly5Counter + pokeyClockCounter ];
}
void channel1_4( void )
{
	if( poly5_b[ poly5Counter + pokeyClockCounter ]!=0 )
		signal_state_out[1] = poly4_b[ poly4Counter + pokeyClockCounter ];
}
void channel1_8( void )
{
	signal_state_out[1] = poly17[ pokeyClockCounter & noiseAND ];
}
void channel1_A( void )
{
	signal_state_out[1] = signal_state_out[1] ^ 15;
}
void channel1_C( void )
{
	signal_state_out[1] = poly4_b[ pokeyClockCounter + poly4Counter ];
}
//---
void channel2_0( void )
{
	if( poly5_b[ poly5Counter + pokeyClockCounter ]!=0 )
		signal_state_out[2] = poly17[ pokeyClockCounter & noiseAND ];
}
void channel2_2( void )
{
	signal_state_out[2] = signal_state_out[2] ^ poly5_b[ poly5Counter + pokeyClockCounter ];
}
void channel2_4( void )
{
	if( poly5_b[ poly5Counter + pokeyClockCounter ]!=0 )
		signal_state_out[2] = poly4_b[ poly4Counter + pokeyClockCounter ];
}
void channel2_8( void )
{
	signal_state_out[2] = poly17[ pokeyClockCounter & noiseAND ];
}
void channel2_A( void )
{
	signal_state_out[2] = signal_state_out[2] ^ 15;
}
void channel2_C( void )
{
	signal_state_out[2] = poly4_b[ pokeyClockCounter + poly4Counter ];
}
//---
void channel3_0( void )
{
	if( poly5_b[ poly5Counter + pokeyClockCounter ]!=0 )
		signal_state_out[3] = poly17[ pokeyClockCounter & noiseAND ];
}
void channel3_2( void )
{
	signal_state_out[3] = signal_state_out[3] ^ poly5_b[ poly5Counter + pokeyClockCounter ];
}
void channel3_4( void )
{
	if( poly5_b[ poly5Counter + pokeyClockCounter ]!=0 )
		signal_state_out[3] = poly4_b[ poly4Counter + pokeyClockCounter ];
}
void channel3_8( void )
{
	signal_state_out[3] = poly17[ pokeyClockCounter & noiseAND ];
}
void channel3_A( void )
{
	signal_state_out[3] = signal_state_out[3] ^ 15;
}
void channel3_C( void )
{
	signal_state_out[3] = poly4_b[ pokeyClockCounter + poly4Counter ];
}

void channel0_1( void )
{
	signal_state_out[0] = 15;
}
void channel1_1( void )
{
	signal_state_out[1] = 15;
}
void channel2_1( void )
{
	signal_state_out[2] = 15;
}
void channel3_1( void )
{
	signal_state_out[3] = 15;
}

typedef void (*funcPoint)(void);
funcPoint channelsDistorionTable0[16]=
	{ &channel0_0, &channel0_1, &channel0_2, &channel0_1, &channel0_4, &channel0_1, &channel0_2, &channel0_1, &channel0_8, &channel0_1, &channel0_A, &channel0_1, &channel0_C, &channel0_1, &channel0_A, &channel0_1 };
funcPoint channelsDistorionTable1[16]=
	{ &channel1_0, &channel1_1, &channel1_2, &channel1_1, &channel1_4, &channel1_1, &channel1_2, &channel1_1, &channel1_8, &channel1_1, &channel1_A, &channel1_1, &channel1_C, &channel1_1, &channel1_A, &channel1_1 };
funcPoint channelsDistorionTable2[16]=
	{ &channel2_0, &channel2_1, &channel2_2, &channel2_1, &channel2_4, &channel2_1, &channel2_2, &channel2_1, &channel2_8, &channel2_1, &channel2_A, &channel2_1, &channel2_C, &channel2_1, &channel2_A, &channel2_1 };
funcPoint channelsDistorionTable3[16]=
	{ &channel3_0, &channel3_1, &channel3_2, &channel3_1, &channel3_4, &channel3_1, &channel3_2, &channel3_1, &channel3_8, &channel3_1, &channel3_A, &channel3_1, &channel3_C, &channel3_1, &channel3_A, &channel3_1 };

void pus_zero( int n )
{
unsigned int i;
unsigned int ch;
DWORD pp;
int ss;

	pp = pokeyClockCounter + (pcc1564 - pokeyClockCounter64k);
	ss = sndBufPtrUpp; 

	i = n;
	do
	{
		pokeyClockCounter++;
		if( pokeyClockCounter>=pp )
		{
			pp = pokeyClockCounter + pcc1564;
			ch = 0; { if( --divideByN[ch]==0 ) { generateIRQ0=true; divideByN[ch] = divideByN_Latch2[ch]; Channel0Distortion(); } }
			ch = 1; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel1Distortion(); } }
			ch = 2; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel2Distortion(); switch_J3_Q_state[0] = signal_state_out[0]; } }
			ch = 3; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel3Distortion(); switch_J3_Q_state[1] = signal_state_out[1]; } }
		}
		MAKESAMPLE
	} while(--i);
	sndBufPtrUpp = ss;
	pokeyClockCounter64k = pcc1564 + pokeyClockCounter - pp ;

}

void pus_2h( int n )
{
unsigned int i;
unsigned int ch;
DWORD pp; int ss;

	pp = pokeyClockCounter + (pcc1564 - pokeyClockCounter64k);
	ss = sndBufPtrUpp; 
	i = n;
	do
	{
		pokeyClockCounter++;
		if( pokeyClockCounter>=pp )
		{
			pp = pokeyClockCounter + pcc1564;
			ch = 0; { if( --divideByN[ch]==0 ) { generateIRQ0=true; divideByN[ch] = divideByN_Latch2[ch]; Channel0Distortion(); } }
			ch = 1; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel1Distortion(); } }
			ch = 3; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel3Distortion(); switch_J3_Q_state[1] = signal_state_out[1]; } }
		}
		ch = 2; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel2Distortion(); switch_J3_Q_state[0] = signal_state_out[0]; } }
		MAKESAMPLE
	} while(--i);
	sndBufPtrUpp = ss;
	pokeyClockCounter64k = pcc1564 + pokeyClockCounter - pp ;
}

void pus_23h( int n )
{
unsigned int i;
unsigned int ch;
DWORD pp; 
int ss;

	pp = pokeyClockCounter + (pcc1564 - pokeyClockCounter64k);
	ss = sndBufPtrUpp; 
	i = n;
	do
	{
		pokeyClockCounter++;
		if( pokeyClockCounter>=pp )
		{
			pp = pokeyClockCounter + pcc1564;
			ch = 0; { if( --divideByN[ch]==0 ) { generateIRQ0=true; divideByN[ch] = divideByN_Latch2[ch]; Channel0Distortion(); } }
			ch = 1; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel1Distortion(); } }
		}
		ch = 2; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel2Distortion(); switch_J3_Q_state[0] = signal_state_out[0]; } }
		ch = 3; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel3Distortion(); switch_J3_Q_state[1] = signal_state_out[1]; } }
		MAKESAMPLE
	} while(--i);
	sndBufPtrUpp = ss;
	pokeyClockCounter64k = pcc1564 + pokeyClockCounter - pp ;
}

void pus_0h( int n )
{
unsigned int i;
unsigned int ch;
DWORD pp;
int ss;

	pp = pokeyClockCounter + (pcc1564 - pokeyClockCounter64k);
	ss = sndBufPtrUpp; 
	i = n;
	do
	{
		pokeyClockCounter++;
		ch = 0; { if( --divideByN[ch]==0 ) { generateIRQ0=true; divideByN[ch] = divideByN_Latch2[ch]; Channel0Distortion(); } }
		if( pokeyClockCounter>=pp )
		{
			pp = pokeyClockCounter + pcc1564;
			ch = 1; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel1Distortion(); } }
			ch = 2; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel2Distortion(); switch_J3_Q_state[0] = signal_state_out[0]; } }
			ch = 3; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel3Distortion(); switch_J3_Q_state[1] = signal_state_out[1]; } }
		}
		MAKESAMPLE
	} while(--i);
	sndBufPtrUpp = ss;
	pokeyClockCounter64k = pcc1564 + pokeyClockCounter - pp ;
}

void pus_01h( int n )
{
unsigned int i;
unsigned int ch;
DWORD  pp;
int ss;

	pp = pokeyClockCounter + (pcc1564 - pokeyClockCounter64k);
	ss = sndBufPtrUpp; 
	i = n;
	do
	{
		pokeyClockCounter++;
		ch = 0; { if( --divideByN[ch]==0 ) { generateIRQ0=true; divideByN[ch] = divideByN_Latch2[ch]; Channel0Distortion(); } }
		ch = 1; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel1Distortion(); } }
		if( pokeyClockCounter>=pp )
		{
			pp = pokeyClockCounter + pcc1564;
			ch = 2; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel2Distortion(); switch_J3_Q_state[0] = signal_state_out[0]; } }
			ch = 3; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel3Distortion(); switch_J3_Q_state[1] = signal_state_out[1]; } }
		}
		MAKESAMPLE
	} while(--i);
	sndBufPtrUpp = ss;
	pokeyClockCounter64k = pcc1564 + pokeyClockCounter - pp ;

}

void pus_02h( int n )
{
unsigned int i;
unsigned int ch;
DWORD pp; 
int ss;

	pp = pokeyClockCounter + (pcc1564 - pokeyClockCounter64k);
	ss = sndBufPtrUpp; 
	i = n;
	do
	{
		pokeyClockCounter++;
		ch = 0; { if( --divideByN[ch]==0 ) { generateIRQ0=true; divideByN[ch] = divideByN_Latch2[ch]; Channel0Distortion(); } }
		if( pokeyClockCounter>=pp )
		{
			pp = pokeyClockCounter + pcc1564;
			ch = 1; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel1Distortion(); } }
			ch = 3; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel3Distortion(); switch_J3_Q_state[1] = signal_state_out[1]; } }
		}
		ch = 2; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel2Distortion(); switch_J3_Q_state[0] = signal_state_out[0]; } }
		MAKESAMPLE
	} while(--i);
	sndBufPtrUpp = ss;
	pokeyClockCounter64k = pcc1564 + pokeyClockCounter - pp ;
}

void pus_012h( int n )
{
unsigned int i;
unsigned int ch;
DWORD pp;
int ss;

	pp = pokeyClockCounter + (pcc1564 - pokeyClockCounter64k);
	ss = sndBufPtrUpp; 
	i = n;
	do
	{
		pokeyClockCounter++;
		ch = 0; { if( --divideByN[ch]==0 ) { generateIRQ0=true; divideByN[ch] = divideByN_Latch2[ch]; Channel0Distortion(); } }
		ch = 1; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel1Distortion(); } }
		if( pokeyClockCounter>=pp )
		{
			pp = pokeyClockCounter + pcc1564;
			ch = 3; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel3Distortion(); switch_J3_Q_state[1] = signal_state_out[1]; } }
		}
		ch = 2; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel2Distortion(); switch_J3_Q_state[0] = signal_state_out[0]; } }
		MAKESAMPLE
	} while(--i);
	sndBufPtrUpp = ss;
	pokeyClockCounter64k = pcc1564 + pokeyClockCounter - pp ;
}

void pus_023h( int n )
{
unsigned int i;
unsigned int ch;
DWORD  pp;
int ss;

	pp = pokeyClockCounter + (pcc1564 - pokeyClockCounter64k);
	ss = sndBufPtrUpp; 
	i = n;
	do
	{
		pokeyClockCounter++;
		ch = 0; { if( --divideByN[ch]==0 ) { generateIRQ0=true; divideByN[ch] = divideByN_Latch2[ch]; Channel0Distortion(); } }
		if( pokeyClockCounter>=pp )
		{
			pp = pokeyClockCounter + pcc1564;
			ch = 1; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel1Distortion(); } }
		}
		ch = 2; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel2Distortion(); switch_J3_Q_state[0] = signal_state_out[0]; } }
		ch = 3; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel3Distortion(); switch_J3_Q_state[1] = signal_state_out[1]; } }
		MAKESAMPLE
	} while(--i);
	sndBufPtrUpp = ss;
	pokeyClockCounter64k = pcc1564 + pokeyClockCounter - pp ;
}

void pus_0123h( int n )
{
unsigned int i;
unsigned int ch;
int ss;

	ss = sndBufPtrUpp; 
	i = n;
	do
	{
		pokeyClockCounter++;
		ch = 0; { if( --divideByN[ch]==0 ) { generateIRQ0=true; divideByN[ch] = divideByN_Latch2[ch]; Channel0Distortion(); } }
		ch = 1; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel1Distortion(); } }
		ch = 2; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel2Distortion(); switch_J3_Q_state[0] = signal_state_out[0]; } }
		ch = 3; { if( --divideByN[ch]==0 ) { divideByN[ch] = divideByN_Latch2[ch]; Channel3Distortion(); switch_J3_Q_state[1] = signal_state_out[1]; } }
		MAKESAMPLE
	} while(--i);
	sndBufPtrUpp = ss;
}

typedef void (*funcPoint2)(int n);

funcPoint2 channelsGeneration[16]={
	&pus_zero,	&pus_zero,	&pus_zero,	&pus_zero,
	&pus_2h,	&pus_23h,	&pus_2h,	&pus_23h,
	&pus_0h,	&pus_0h,	&pus_01h,	&pus_01h,
	&pus_02h,	&pus_023h,	&pus_012h,	&pus_0123h
};

}; // end of namespace POKEY_NAMESPACE

using namespace NAMESPACENAME;

POKEY_INIT_FUNC
{
unsigned int i;
DWORD pol;
BYTE old;

	pol = 0x1FFFF;
	for( i=0; i<0x20000; i++ )
	{
		poly17[i] = (BYTE)(pol&1 ? 15:0);
		pol = pol | (((pol&1) ^ ((pol>>5)&1)) << 17);
		pol>>=1;
	}

	for( i=0; i<sizeof(poly4_b); i++ ) // this table repeats after 15
		poly4_b[i] = poly4[i % 15];

	for( i=0; i<sizeof(poly5_b); i++ ) // this table repeats after 31
		poly5_b[i] = poly5[i % 31];

	old = 0;
	for( i=0; i<sizeof(poly4_5_b); i++ ) // this table repeats after 465
	{
		if( poly5[i % 31] )
			old = poly4[i % 15];
		poly4_5_b[i] = old;
	}

}

POKEY_RESET_FUNC
{
int i;

	IRQ_EN		= 0x00;
	IRQ_ST		= 0x00;
	IRQ_line	= 0x00;

	poly4Counter = 0;
	poly5Counter = 0;
	poly4_5Counter = 0;
	pokeyClockCounter = 0;
	pokeyClockCounter64k = 0;
	pcc1564 = 28;
	oldValI = 0;
	oldVal	=0.0;

	for( i=0; i<4; i++ )
	{
		divideByN[i] = 1;
		divideByN_Latch[i] = 0;
		divideByN_Latch2[i] = 0;
		switch_J2_signal_Q[i] = 0;
		signal_state_out[i] = 0;
		switch_J3_Q_state[i] = 0;
		switch_J3_Q_stateAND[i] = 0;
		audioControl_Latch[i] = 0;
		audioControl_Latch2[i] = 0;
		audioControl_Latch_Digi[i] = 0;
		freq_sequre[i] = 0;
	}

	for( i=0; i<16; i++ )
		POKEY_WRITE_2FUNC( 0xD200+i, 0 );
	AUDCTL = 0;
	POKEY_WRITE_2FUNC( 0xD208, 0x28 );

	sndBufPtrUpp = 0;
	delay = 0;

}

POKEY_UPDATESOUND_FUNC
{

	channelsGeneration[ (AUDCTL>>3)&15 ](n);

}


POKEY_UPDATECOUNT_FUNC
{

	pokeyClockCounter = pokeyClockCounter & 0x7FFFFFFF;
	poly4Counter = (int)(((DWORD)(poly4Counter+pokeyClockCounter)) % 15) - pokeyClockCounter;
	poly5Counter = (int)(((DWORD)(poly5Counter+pokeyClockCounter)) % 31) - pokeyClockCounter;
	poly4_5Counter = (int)(((DWORD)(poly4_5Counter+pokeyClockCounter)) % 465) - pokeyClockCounter;

}

#undef SetupChannels01
#undef SetupChannels23

#define SetupChannels01\
			switch( AUDCTL&0x50 )\
			{\
				case 0x00:\
					divideByN_Latch2[0] = divideByN_Latch[0]+1;\
					if( ((audioControl_Latch[0]&0xA0)==0xA0) && (divideByN_Latch2[0]<3) )\
						freq_sequre[0] = 0;\
					else\
						freq_sequre[0] = 15;\
					divideByN_Latch2[1] = divideByN_Latch[1]+1;\
					if( ((audioControl_Latch[1]&0xA0)==0xA0) && (divideByN_Latch2[1]<3) )\
						freq_sequre[1] = 0;\
					else\
						freq_sequre[1] = 15;\
					break;\
				case 0x10:\
					divideByN_Latch2[0] = divideByN_Latch[0]+1;\
					if( ((audioControl_Latch[0]&0xA0)==0xA0) && (divideByN_Latch2[0]<3) )\
						freq_sequre[0] = 0;\
					else\
						freq_sequre[0] = 15;\
					divideByN_Latch2[1] = divideByN_Latch[1]*256+divideByN_Latch2[0];\
					if( ((audioControl_Latch[1]&0xA0)==0xA0) && (divideByN_Latch2[1]<3) )\
						freq_sequre[1] = 0;\
					else\
						freq_sequre[1] = 15;\
					break;\
				case 0x40:\
					divideByN_Latch2[0] = divideByN_Latch[0]+4;\
					if( ((audioControl_Latch[0]&0xA0)==0xA0) && (divideByN_Latch2[0]<0x50) )\
						freq_sequre[0] = 0;\
					else\
						freq_sequre[0] = 15;\
					divideByN_Latch2[1] = divideByN_Latch[1]+1;\
					if( ((audioControl_Latch[1]&0xA0)==0xA0) && (divideByN_Latch2[1]<3) )\
						freq_sequre[1] = 0;\
					else\
						freq_sequre[1] = 15;\
					break;\
				case 0x50:\
					divideByN_Latch2[0] = divideByN_Latch[0]+7;\
					freq_sequre[0] = 0;\
					divideByN_Latch2[1] = divideByN_Latch[1]*256+divideByN_Latch2[0];\
					if( ((audioControl_Latch[1]&0xA0)==0xA0) && (divideByN_Latch2[1]<0x50) )\
						freq_sequre[1] = 0;\
					else\
						freq_sequre[1] = 15;\
					break;\
			}

#define SetupChannels23\
			switch( AUDCTL&0x28 )\
			{\
				case 0x00:\
					divideByN_Latch2[2] = divideByN_Latch[2]+1;\
					if( ((audioControl_Latch[2]&0xA0)==0xA0) && (divideByN_Latch2[2]<3) )\
						freq_sequre[2] = 0;\
					else\
						freq_sequre[2] = 15;\
					divideByN_Latch2[3] = divideByN_Latch[3]+1;\
					if( ((audioControl_Latch[3]&0xA0)==0xA0) && (divideByN_Latch2[3]<3) )\
						freq_sequre[3] = 0;\
					else\
						freq_sequre[3] = 15;\
					break;\
				case 0x08:\
					divideByN_Latch2[2] = divideByN_Latch[2]+1;\
					if( ((audioControl_Latch[2]&0xA0)==0xA0) && (divideByN_Latch2[2]<3) )\
						freq_sequre[2] = 0;\
					else\
						freq_sequre[2] = 15;\
					divideByN_Latch2[3] = divideByN_Latch[3]*256+divideByN_Latch2[2];\
					if( ((audioControl_Latch[3]&0xA0)==0xA0) && (divideByN_Latch2[3]<3) )\
						freq_sequre[3] = 0;\
					else\
						freq_sequre[3] = 15;\
					break;\
				case 0x20:\
					divideByN_Latch2[2] = divideByN_Latch[2]+4;\
					if( ((audioControl_Latch[2]&0xA0)==0xA0) && (divideByN_Latch2[2]<0x50) )\
						freq_sequre[2] = 0;\
					else\
						freq_sequre[2] = 15;\
					divideByN_Latch2[3] = divideByN_Latch[3]+1;\
					if( ((audioControl_Latch[3]&0xA0)==0xA0) && (divideByN_Latch2[3]<3) )\
						freq_sequre[3] = 0;\
					else\
						freq_sequre[3] = 15;\
					break;\
				case 0x28:\
					divideByN_Latch2[2] = divideByN_Latch[2]+7;\
					freq_sequre[2] = 0;\
					divideByN_Latch2[3] = divideByN_Latch[3]*256+divideByN_Latch2[2];\
					if( ((audioControl_Latch[3]&0xA0)==0xA0) && (divideByN_Latch2[3]<0x50) )\
						freq_sequre[3] = 0;\
					else\
						freq_sequre[3] = 15;\
					break;\
			}
