#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sapGlobals.h"

#define NAMESPACENAME			POKEY1_NAMESPACE
#define POKEY_INIT_FUNC			void pokeyInit1( void )
#define POKEY_RESET_FUNC		void pokeyReset1( void )
#define POKEY_UPDATESOUND_FUNC		void pokeyUpdateSound1( int n )
#define POKEY_UPDATECOUNT_FUNC		void pokeyUpdateSoundCounters1( void )
#define POKEY_WRITE_FUNC		void pokeyWriteByte1( short unsigned int address, BYTE value )
#define POKEY_WRITE_2FUNC(a,v)		pokeyWriteByte1(a,v)

#include "pokeyNamespace.h"

POKEY_WRITE_FUNC
{
	address&=0x0F;

	switch( address )
	{
		case 0x00:
			divideByN_Latch[0] = value;
			SetupChannels01;
			break;
		case 0x01:
			audioControl_Latch[0] = value;
			audioControl_Latch2[0] = value&15;
			audioControl_Latch_Digi[0] = (value>>4)&1 ? 15:0;
			Channel0Distortion = channelsDistorionTable0[ (value>>4)&15 ];
			if( !(value&0x10) )
				SetupChannels01;
			break;
		case 0x02:
			divideByN_Latch[1] = value;
			SetupChannels01;
			break;
		case 0x03:
			audioControl_Latch[1] = value;
			audioControl_Latch2[1] = value&15;
			audioControl_Latch_Digi[1] = (value>>4)&1 ? 15:0;
			Channel1Distortion = channelsDistorionTable1[ (value>>4)&15 ];
			if( !(value&0x10) )
				SetupChannels01;
			break;
		case 0x04:
			divideByN_Latch[2] = value;
			SetupChannels23;
			break;
		case 0x05:
			audioControl_Latch[2] = value;
			audioControl_Latch2[2] = value&15;
			audioControl_Latch_Digi[2] = (value>>4)&1 ? 15:0;
			Channel2Distortion = channelsDistorionTable2[ (value>>4)&15 ];
			if( !(value&0x10) )
				SetupChannels23;
			break;
		case 0x06:
			divideByN_Latch[3] = value;
			SetupChannels23;
			break;
		case 0x07:
			audioControl_Latch[3] = value;
			audioControl_Latch2[3] = value&15;
			audioControl_Latch_Digi[3] = (value>>4)&1 ? 15:0;
			Channel3Distortion = channelsDistorionTable3[ (value>>4)&15 ];
			if( !(value&0x10) )
				SetupChannels23;
			break;
		case 0x08:
		{
			BYTE prevAUDCTL;
			prevAUDCTL = AUDCTL;
			AUDCTL = value;
			pcc1564 = value & 1 ? 112:28;

			noiseAND = AUDCTL & 128 ? 0x1FF:0x1FFFF;

			switch_J3_Q_stateAND[0] = AUDCTL&4 ? 15:0;
			switch_J3_Q_stateAND[1] = AUDCTL&2 ? 15:0;

			SetupChannels01;
			SetupChannels23;

			if( (prevAUDCTL^AUDCTL)&0x10 )
				divideByN[1] = 2;
			if( (prevAUDCTL^AUDCTL)&0x8 )
				divideByN[3] = 2;

			break;
		}
		case 0x0D:
			break;
		case 0x0E:
			break;
		case 0x0F:
			break;
	}
}
