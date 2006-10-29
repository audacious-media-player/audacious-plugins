#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sapGlobals.h"

extern void pokeyInit0( void );
extern void pokeyInit1( void );
extern void pokeyReset0( void );
extern void pokeyReset1( void );
extern void pokeyUpdateSound0( int n );
extern void pokeyUpdateSound1( int n );
extern void pokeyUpdateSoundCounters0( void );
extern void pokeyUpdateSoundCounters1( void );

void pokeyInit( void )
{
	pokeyInit0();
	pokeyInit1();
}
void pokeyReset( void )
{
	pokeyReset0();
	pokeyReset1();
}

void pokeyUpdateSound( int n )
{
int oldBufPtr = sndBufPtr;
	pokeyUpdateSound0( n );
	if( isStereo )
	{
		sndBufPtr = (oldBufPtr+1)&16383;
		pokeyUpdateSound1( n );
		sndBufPtr = (sndBufPtr-1)&16383;
	}
}

void pokeyUpdateSoundCounters( void )
{
	pokeyUpdateSoundCounters0();
	pokeyUpdateSoundCounters1();
}

BYTE pokeyReadByte( short unsigned int address)
{
BYTE retVal;

	switch( address&0x0F )
	{
		case 0x09:
			return 0xFF;
		case 0x0A:
			retVal = (BYTE)((255*rand())/RAND_MAX);
			return retVal;
		case 0x0E:
			return 0xFF;
		case 0x0F:
			return 0xFF;
	}
	return 0xFF;
}
