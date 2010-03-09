
#include "usf.h"
#include "audio_hle.h"
#include "memory.h"
#include "audio.h"
// "Mupen64 HLE RSP plugin v0.2 with Azimers code by Hacktarux"


static int audio_ucode_detect ( OSTask_t *task )
{

	if ( (*(uint8_t*) ( N64MEM + task->ucode_data )) != 0x1 )
	{
		if ( (*(uint8_t*) ( N64MEM + task->ucode_data )) == 0xF )
			return 4;
		else
			return 3;
	}
	else
	{
		if ( (*(uint32_t*) (N64MEM + task->ucode_data + 0x30 )) == 0xF0000F00 ) {
			if ((*(uint32_t*) (N64MEM + task->ucode_data + 0x28 )) == 0x1dc8138c)
				return 5; //goldeneye
			else
				return 1;

		}
		else
			return 2;
	}
}

extern void ( *ABI1[0x20] ) ();
extern void ( *ABI2[0x20] ) ();
extern void ( *ABI3[0x20] ) ();

void ( *ABI[0x20] ) ();

u32 inst1, inst2;

int audio_ucode ( OSTask_t *task )
{

	unsigned int *p_alist = ( u32* )( task->data_ptr + N64MEM );
	unsigned int i;

	goldeneye = 0;

	switch ( audio_ucode_detect ( task ) )
	{
		case 1: // mario ucode
			memcpy ( ABI, ABI1, sizeof ( ABI[0] ) *0x20 );
			//cprintf("Ucode1\n");
			break;
		case 2: // banjo kazooie ucode
			memcpy ( ABI, ABI2, sizeof ( ABI[0] ) *0x20 );
			//cprintf("Ucode2\n");
			break;
		case 3: // zelda ucode
			memcpy ( ABI, ABI3, sizeof ( ABI[0] ) *0x20 );
			//cprintf("Ucode3\n");
			break;
		case 5:
			goldeneye = 1;
			memcpy ( ABI, ABI1, sizeof ( ABI[0] ) *0x20 );
			break;
		default:
		{

			return -1;
		}
	}

	for ( i = 0; i < ( task->data_size/4 ); i += 2 )
	{
		inst1 = (*(uint32_t*) ( N64MEM + task->data_ptr + ( i*4 ) ));
		inst2 = (*(uint32_t*) ( N64MEM + task->data_ptr + ( ( i+1 ) *4 ) ));
		//printf("%x\t%x\n",inst1 >> 24,inst1);
		ABI[inst1 >> 24]();

	}

	return 0;
}
