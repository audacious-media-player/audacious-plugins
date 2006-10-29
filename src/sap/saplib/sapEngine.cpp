#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __MAIN_DECLARATIONS__

#include "sapGlobals.h"

#include "sapLib.h"

namespace _SAP_internals_ {

void playerCallSubroutine( WORD address );
void playerProcessOneFrame( void );

volatile int prSbp;
volatile int musicAddress,playerAddress,playerInit,playerType,defSong,fastPlay;
volatile BOOL fileLoadStatus=FALSE;

char inputBuffer[65536];
char commentBuffer[65536];

sapMUSICstrc currentMusic;

BYTE emulEmptyLine[]={
	1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,	0,  0,	0,  0,  0,  0,  0,  0,  0,  1,  0,  0,  0,  1,  0,  0,
	0,  1,  0,  0,  0,  1,  0,  0,  0,  1,  0,  0,  0,  1,  0,  0,	0,  1,  0,  0,  0,  1,  0,  0,  0,  1,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 	0,  0 
};

}

using namespace _SAP_internals_;

sapMUSICstrc *sapLoadMusicFile( char *fname )
{
BOOL formatKnown,binaryFile; int i;
int numOfSongs;

	fileLoadStatus = FALSE;
	if( fname==NULL )
		return NULL;

	cpuInit( );
	pokeyInit( );
	pokeyReset( );
	prSbp = 0;
	sndBufPtr = 0;
	memset( sndBuf, 0, sizeof(sndBuf) );
	commentBuffer[0] = 0;
	memset( atariMem, 0, sizeof(atariMem) );
	atariMem[0x0000] = 0xFF; // CMC bug.
	atariMem[0xFEFF] = 0xFF; // CMC bug.
//	atariMem[0xFFFE] = 0xFF; // CMC bug.
//	atariMem[0xFFFF] = 0xFF; // CMC bug.

	isStereo = false;
	sampleStep = 1;
	formatKnown = FALSE; binaryFile = FALSE;
	musicAddress = playerAddress = playerType = numOfSongs = defSong = fastPlay = -1;
	FILE *inpf;
	inpf = fopen( fname, "rb" );
	if( inpf!=NULL )
	{
		BYTE blkHead[4]; WORD blkBegin = 0,blkEnd = 0; int blk;

		blk = 0; i = 0;
		while( 1 )
		{
			int val; val = fgetc( inpf );
			if( formatKnown==FALSE )
			{
				if( (val==EOF) || (i>=4) )
				{
					// error
					break;
				}
				if( (val==0x0A) || (val==0x0D) )
				{
					inputBuffer[i] = 0;
					if( strcmp( inputBuffer, "SAP" )!=0 )
					{
						// error
						break;
					}
					// headerID is OK
					i = 0;
					formatKnown = TRUE;
					continue;;
				}
				inputBuffer[i++] = (char)val;
			}
			else if( binaryFile==FALSE )
			{
				if( val==EOF )
				{
					// error
					break;
				}
				if( val==0xFF )
				{
					if( i!=0 )
					{
						// error. There wasn't EOL in this line
						break;
					}
					// so this is the end of header
					binaryFile = TRUE;
					blkHead[blk++] = (BYTE)val;
					continue;
				}
				if( (val==0x0A) || (val==0x0D) )
				{
					if( i==0 )
						continue;
					inputBuffer[i++] = 0;
					char *codes[]={ "PLAYER", "MUSIC", "INIT", "TYPE", "SONGS", "DEFSONG", "FASTPLAY", "STEREO", "TIME", NULL };
					int j,k,a;
					for( j=0;; j++ )
					{
						if( codes[j]==NULL )
							break;
						k = 0;
						while( 1 )
						{
							if( k>=i )
							{
								// not this code
								i = 0;
								break;
							}
							if( (codes[j][k]==0) && ((inputBuffer[k]==' ') || (inputBuffer[k]==0)) )
							{
								// found
								switch( j )
								{
									case 0: // player address
										a = strtol( &inputBuffer[k], NULL, 16 );
										if( (a==0) || (a>0xFFFF) )
										{
											// error
											i = 0;
											goto cont;
										}
										playerAddress = a;
										i = 0;
										goto cont;
										break;
									case 1: // music address
										a = strtol( &inputBuffer[k], NULL, 16 );
										if( (a==0) || (a>0xFFFF) )
										{
											// error
											i = 0;
											goto cont;
										}
										musicAddress = a;
										i = 0;
										goto cont;
										break;
									case 2: // binary player init
										a = strtol( &inputBuffer[k], NULL, 16 );
										if( (a==0) || (a>0xFFFF) )
										{
											// error
											i = 0;
											goto cont;
										}
										playerInit = a;
										i = 0;
										goto cont;
										break;
									case 3: // player type
										while( inputBuffer[k]!=0 )
										{
											if( (inputBuffer[k]=='B') || (inputBuffer[k]=='b') )
											{
												playerType = 'b';
												break;
											}
											if( (inputBuffer[k]=='C') || (inputBuffer[k]=='c') )
											{
												playerType = 'c';
												break;
											}
											if( (inputBuffer[k]=='D') || (inputBuffer[k]=='d') )
											{
												playerType = 'd';
												break;
											}
											if( (inputBuffer[k]=='S') || (inputBuffer[k]=='s') )
											{
												playerType = 's';
												break;
											}
											if( !isspace(inputBuffer[k]) )
											{
												i = 0;
												goto cont;
											}
											k++;
										}
										i = 0;
										goto cont;
										break;
									case 4: // num of songs
										a = strtol( &inputBuffer[k], NULL, 10 );
										if( (a==0) || (a>0xFFFF) )
										{
											// error
											i = 0;
											goto cont;
										}
										numOfSongs = a;
										sprintf( &commentBuffer[ strlen(commentBuffer) ], "Number of songs = %d\n", numOfSongs );
										i = 0;
										goto cont;
										break;
									case 5: // default songs
										a = strtol( &inputBuffer[k], NULL, 10 );
										if( (a==0) || (a>0xFFFF) )
										{
											// error
											i = 0;
											goto cont;
										}
										defSong = a;
										i = 0;
										goto cont;
										break;
									case 6: // fastPlay
										a = strtol( &inputBuffer[k], NULL, 10 );
										if( (a==0) || (a>0xFFFF) )
										{
											// error
											i = 0;
											goto cont;
										}
										fastPlay = a;
										i = 0;
										goto cont;
										break;
									case 7: // Stereo
										isStereo = true;
										sampleStep = 2;
										sprintf( &commentBuffer[ strlen(commentBuffer) ], "In Stereo!\n" );
										i = 0;
										goto cont;
										break;
									case 8: // Time
									{
										int min,sec;
										min = sec = -1;
										sscanf( &inputBuffer[k], "%d:%d", &min, &sec );
										if( min==-1 || sec==-1 )
										{
											i = 0;
											goto cont;
										}
										sprintf( &commentBuffer[ strlen(commentBuffer) ], "Time in sec %d\n", min*60+sec );
										i = 0;
										goto cont;
										break;
									}
								}
								i = 0;
								goto cont;
							}
							if( codes[j][k]==0 )
								break; // not this code
							if( codes[j][k]!=inputBuffer[k] )
								break; // not this code
							k++;
						}
					}
					sprintf( &commentBuffer[ strlen(commentBuffer) ], "%s\n", inputBuffer );
					// not found
					i = 0;
					continue;
				}
				inputBuffer[i++] = (char)val;
			}
			else if( binaryFile==TRUE )
			{
				if( val==EOF )
				{
					if( blk==0 )
					{
						fileLoadStatus = TRUE;
						break; // OK
					}
					// error
					break;
				}
				if( blk<4 )
				{
					blkHead[blk++] = (BYTE)val;
					if( blk==2 )
					{
						if( (blkHead[0]&blkHead[1])==255 )
							blk = 0;
					}
					if( blk==4 )
					{
						blkBegin = ((WORD)blkHead[0]) + ((WORD)blkHead[1])*256;
						blkEnd = ((WORD)blkHead[2]) + ((WORD)blkHead[3])*256;
					}
				}
				else
				{
					atariMem[blkBegin] = (BYTE)val;
					if( blkBegin==blkEnd )
					{
						blk = 0;
					}
					blkBegin++;
				}
			}
			cont:;

		}
		fclose( inpf );

	}
	if( playerType==-1 )
		return NULL;
	fileLoadStatus = TRUE;
	
	currentMusic.defSong			= defSong;
	currentMusic.numOfSongs			= numOfSongs;
	

	currentMusic.commentBuffer		= &commentBuffer[0];
	currentMusic.currentTimeIn50Sec	= 0;

//	sapPlaySong( defSong );
	return &currentMusic;
}


void sapPlaySong( int numOfSong )
{

	if( fileLoadStatus==FALSE )
		return;

	if( numOfSong==-1 )
		numOfSong = 0;
	numOfSong &= 0xFF;
	numOfSong = numOfSong % currentMusic.numOfSongs;

	sndBufPtr = prSbp = 0;
	switch( playerType )
	{
		case 'c':
			if( (playerAddress==-1) || (musicAddress==-1) )
			{
				fileLoadStatus = FALSE;
				break;
			}
			cpuReg_S = 0xFF;
			cpuReg_A = 0x70;
			cpuReg_X = (musicAddress&0xFF);
			cpuReg_Y = (musicAddress>>8)&0xFF;
			playerCallSubroutine( playerAddress+3 );
			cpuReg_S = 0xFF;
			cpuReg_A = 0x00;
			cpuReg_X = numOfSong;
			playerCallSubroutine( playerAddress+3 );
			break;
		case 'b':
		case 'm':
			if( (playerInit==-1) || (playerAddress==-1) )
			{
				fileLoadStatus = FALSE;
				break;
			}
			cpuReg_S = 0xFF;
			cpuReg_A = numOfSong;
			playerCallSubroutine( playerInit );
			break;
		case 'd':
			if( (playerInit==-1) || (playerAddress==-1) )
			{
				fileLoadStatus = FALSE;
				break;
			}
			cpuReg_S = 0xFF;
			cpuReg_PC = 0xFFFF;
			cpuReg_PC--;
			atariMem[0x100 + cpuReg_S] = (cpuReg_PC>>8)&0xFF; cpuReg_S--;
			atariMem[0x100 + cpuReg_S] = cpuReg_PC&0xFF; cpuReg_S--;
			cpuReg_PC = playerInit;
			cpuReg_A = numOfSong;
			cpuReg_X = 0;
			cpuReg_Y = 0;
			cpuSetFlags( 0x20 );
			break;
		case 's':
			if( (playerInit==-1) || (playerAddress==-1) )
			{
				fileLoadStatus = FALSE;
				break;
			}
			cpuReg_S = 0xFF;
			cpuReg_PC = playerInit;
			cpuReg_A = 0;
			cpuReg_X = 0;
			cpuReg_Y = 0;
			cpuSetFlags( 0x20 );
			break;
	}
}


void _SAP_internals_::playerCallSubroutine( WORD address )
{
int i,k;

	cpuReg_PC = 0xFFFF;
	cpuReg_PC--;
	atariMem[0x100 + cpuReg_S] = (cpuReg_PC>>8)&0xFF; cpuReg_S--;
	atariMem[0x100 + cpuReg_S] = cpuReg_PC&0xFF; cpuReg_S--;
	cpuReg_PC = address;
	k = 0;
	bool holded;
	while(k<1000000)
	{
		BYTE opcode = atariMem[ cpuReg_PC ];
		i =  opcodes_0x00_0xFF[opcode](holded);
		k+=i;
		if( i>10 )
			return;
		if( cpuReg_PC==0xFFFF )
			break;
	}
	k++;
}

extern bool *generateIRQ0;
int numsPerFrame;

void _SAP_internals_::playerProcessOneFrame( void )
{
int ilp = 0;
BYTE oldFlags = 0,oldA = 0,oldX = 0,oldY = 0,oldS = 0;
WORD oldPC = 0;
BOOL notRestored = 0;
bool holded;
int	cycleInLine,lineInFrame;

	numsPerFrame = 0;

	if( playerType=='d' )
	{
		oldFlags = cpuGetFlags();
		oldA = cpuReg_A;
		oldX = cpuReg_X;
		oldY = cpuReg_Y;
		oldPC = cpuReg_PC;
		oldS = cpuReg_S;
		notRestored = TRUE;
	}
	if( playerType=='s' )
	{

	}

	if( playerType!='s' )
	{
		cpuReg_S = 0x8F;
		cpuReg_PC = 0xFFFF;
		cpuReg_PC--;
		atariMem[0x100 + cpuReg_S] = (cpuReg_PC>>8)&0xFF; cpuReg_S--;
		atariMem[0x100 + cpuReg_S] = cpuReg_PC&0xFF; cpuReg_S--;
	}

	switch( playerType )
	{
		case 'c':
			cpuReg_PC = playerAddress+6;
			break;
		case 'b':
		case 'd':
		case 'm':
			cpuReg_PC = playerAddress;
			break;
	}
	cycleInLine = 0;
	if( fastPlay!=-1 )
		lineInFrame = 0;
	else if( playerType=='s' )
		lineInFrame = 0;
	else
		lineInFrame = 248;
	holded = false;
	pokeyUpdateSoundCounters();

	while(1)
	{
		if( cpuReg_PC!=0xFFFF )
		{
			BYTE opcode = atariMem[ cpuReg_PC ];
			ilp =  opcodes_0x00_0xFF[opcode](holded);
			if( ilp>10 )
				return; // not supported instruction has been executed
			if( (playerType=='b') || (playerType=='c') )
				ilp = 1;
		}
		else
		{
			if( playerType=='d' )
			{
				if( notRestored==TRUE )
				{
					notRestored = FALSE;
					cpuReg_PC = oldPC;
					cpuReg_A = oldA;
					cpuReg_X = oldX;
					cpuReg_Y = oldY;
					cpuSetFlags( oldFlags );
					cpuReg_S = oldS;
				}
			}
			else
			{
				do {
					pokeyUpdateSound(114);
					lineInFrame++;
					currentMusic.currentTimeIn50Sec++;
					if( fastPlay!=-1 )
					{
						if( lineInFrame==fastPlay )
						{
							pokeyUpdateSoundCounters();
							goto bre;
						}
					}
					else
					{
						if( lineInFrame==248 )
						{
							pokeyUpdateSoundCounters();
							goto bre;
						}
						if( lineInFrame==312 )
						{
							lineInFrame=0;
							pokeyUpdateSoundCounters();
						}
					}
				} while(1);
			}
		}
	again:
		for(;ilp>0;ilp--)
		{
			if( cycleInLine>=103-9 )
			{
				if( cycleInLine==103-9 )
				{
					if( holded )
					{
						ilp = 0;
						holded = false;
					}
				}
				if( cycleInLine==113-9 )
				{
					pokeyUpdateSound(114);
					cycleInLine = -1;
					lineInFrame++;
					currentMusic.currentTimeIn50Sec++;
					if( fastPlay!=-1 )
					{
						if( lineInFrame==fastPlay )
						{
							pokeyUpdateSoundCounters();
							goto bre;
						}
					}
					else if( playerType=='s' )
					{
						if( lineInFrame==78 )
						{
							pokeyUpdateSoundCounters();
							atariMem[0x45]--;
							if( atariMem[0x45]==0 )
								atariMem[0xB07B]++;
							goto bre;
						}
					}
					else
					{
						if( lineInFrame==248 )
						{
							pokeyUpdateSoundCounters();
							goto bre;
						}
						if( lineInFrame==312 )
						{
							lineInFrame=0;
							pokeyUpdateSoundCounters();
						}
					}
					ANTIC_VCOUNT_D40B = (BYTE)(lineInFrame / 2);
				}
			}
			cycleInLine++;
		}
		if( holded )
		{
			ilp=114-9;
			goto again;
		}
		if( generateIRQ0[0] )
		{
			generateIRQ0[0] = false;
			pokeyGenerateIRQ(1);
			numsPerFrame++;
		}
	}
	bre:;
}


void sapRenderBuffer( short int *buffer, int number_of_samples )
{
int i;
	
	if( fileLoadStatus==FALSE )
		return;

	i = 0;
	number_of_samples *= sampleStep;
	while( i<number_of_samples )
	{
		if( prSbp==sndBufPtr )
			playerProcessOneFrame();
		for( ;prSbp!=sndBufPtr; prSbp=(prSbp+1)&16383 )
		{
			if( isStereo )
				buffer[i] = sndBuf[prSbp&16383];
			else
			{
				buffer[i*2+0] = sndBuf[prSbp&16383];
				buffer[i*2+1] = sndBuf[prSbp&16383];
			}
			if( i>=number_of_samples )
				break;
			i++;
		}
	}
}

