/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   libSIDPlay v1 support

   Programmed and designed by Matti 'ccr' Hamalainen <ccr@tnsp.org>
   (C) Copyright 1999-2005 Tecnic Software productions (TNSP)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/
#include "xmms-sid.h"

#ifdef HAVE_SIDPLAY1

#include "xs_sidplay1.h"
#include <stdio.h>
#include "xs_config.h"
#include "xs_length.h"
#include "xs_title.h"

#include <sidplay/player.h>
#include <sidplay/myendian.h>
#include <sidplay/fformat.h>
#include <audacious/vfs.h>


typedef struct
{
	emuEngine *currEng;
	emuConfig currConfig;
	sidTune *currTune;
} t_xs_sidplay1;


/* We need to 'export' all this pseudo-C++ crap */
extern "C"
{


static gboolean xs_sidplay1_detect_by_content( VFSFile * fp )
{
	gchar magic_bytes[4];
	gboolean res = FALSE;

	if ( fp == NULL )
		return FALSE;

	if ( vfs_fread( magic_bytes , 1 , 4 , fp ) != 4 )
		return FALSE;

	if ( !strncmp( magic_bytes , "PSID" , 4 ) )
		return TRUE;
	else
		return FALSE;
  return FALSE;
}


static gint xs_sidplay1_load_file_in_buffer( gchar * pcFilename , gchar ** buffer , glong * buffer_size )
{
	VFSFile *fp;

	if ( *buffer != NULL ) {
		g_free( *buffer );
		*buffer = NULL;
	}

	fp = vfs_fopen( pcFilename , "rb" );

	if ( fp == NULL )
		return -1;

	vfs_fseek( fp , 0 , SEEK_END );
	*buffer_size = vfs_ftell( fp );

	if ( *buffer_size > 0 )
	{
		glong readsize;
		*buffer = (gchar*)g_malloc(*buffer_size);
		vfs_fseek( fp , 0 , SEEK_SET );
		readsize = vfs_fread( *buffer , 1 , *buffer_size , fp );
		vfs_fclose( fp );
		if ( readsize != *buffer_size )
			return -1;
		else
			return 0;
	}
	else
	{
		vfs_fclose( fp );
		return -1;
	}
}


/* Check if we can play the given file
 */
gboolean xs_sidplay1_isourfile(gchar * pcFilename)
{
	VFSFile * fp;
	gboolean result = FALSE;

	fp = vfs_fopen( pcFilename , "rb" );

	if ( fp == NULL )
		return FALSE;

	result = xs_sidplay1_detect_by_content( fp );
	vfs_fclose( fp );

	return result;
}


/* Initialize SIDPlay1
 */
gboolean xs_sidplay1_init(t_xs_status * myStatus)
{
	gint tmpFreq;
	t_xs_sidplay1 *myEngine;
	assert(myStatus);

	/* Allocate internal structures */
	myEngine = (t_xs_sidplay1 *) g_malloc0(sizeof(t_xs_sidplay1));
	if (!myEngine) return FALSE;

	/* Initialize engine */
	myEngine->currEng = new emuEngine();
	if (!myEngine->currEng) {
		XSERR("Could not initialize libSIDPlay1 emulation engine\n");
		g_free(myEngine);
		return FALSE;
	}

	/* Verify endianess */
	if (!myEngine->currEng->verifyEndianess()) {
		XSERR("Endianess verification failed\n");
		delete myEngine->currEng;
		g_free(myEngine);
		return FALSE;
	}

	myStatus->sidEngine = myEngine;

	/* Get current configuration */
	myEngine->currEng->getConfig(myEngine->currConfig);

	/* Configure channel parameters */
	switch (myStatus->audioChannels) {

	case XS_CHN_AUTOPAN:
		myEngine->currConfig.channels = SIDEMU_STEREO;
		myEngine->currConfig.autoPanning = SIDEMU_CENTEREDAUTOPANNING;
		myEngine->currConfig.volumeControl = SIDEMU_FULLPANNING;
		break;

	case XS_CHN_STEREO:
		myEngine->currConfig.channels = SIDEMU_STEREO;
		myEngine->currConfig.autoPanning = SIDEMU_NONE;
		myEngine->currConfig.volumeControl = SIDEMU_NONE;
		break;

	case XS_CHN_MONO:
	default:
		myEngine->currConfig.channels = SIDEMU_MONO;
		myEngine->currConfig.autoPanning = SIDEMU_NONE;
		myEngine->currConfig.volumeControl = SIDEMU_NONE;
		myStatus->audioChannels = XS_CHN_MONO;
		break;
	}


	/* Memory mode settings */
	switch (xs_cfg.memoryMode) {
	case XS_MPU_TRANSPARENT_ROM:
		myEngine->currConfig.memoryMode = MPU_TRANSPARENT_ROM;
		break;

	case XS_MPU_PLAYSID_ENVIRONMENT:
		myEngine->currConfig.memoryMode = MPU_PLAYSID_ENVIRONMENT;
		break;

	case XS_MPU_BANK_SWITCHING:
	default:
		myEngine->currConfig.memoryMode = MPU_BANK_SWITCHING;
		xs_cfg.memoryMode = XS_MPU_BANK_SWITCHING;
		break;
	}


	/* Audio parameters sanity checking and setup */
	myEngine->currConfig.bitsPerSample = myStatus->audioBitsPerSample;
	tmpFreq = myStatus->audioFrequency;

	if (myStatus->oversampleEnable) {
		if ((tmpFreq * myStatus->oversampleFactor) > SIDPLAY1_MAX_FREQ) {
			myStatus->oversampleEnable = FALSE;
		} else {
			tmpFreq = (tmpFreq * myStatus->oversampleFactor);
		}
	} else {
		if (tmpFreq > SIDPLAY1_MAX_FREQ)
			tmpFreq = SIDPLAY1_MAX_FREQ;
	}

	myEngine->currConfig.frequency = tmpFreq;

	switch (myStatus->audioBitsPerSample) {
	case XS_RES_8BIT:
		switch (myStatus->audioFormat) {
		case FMT_S8:
			myStatus->audioFormat = FMT_S8;
			myEngine->currConfig.sampleFormat = SIDEMU_SIGNED_PCM;
			break;

		case FMT_U8:
		default:
			myStatus->audioFormat = FMT_U8;
			myEngine->currConfig.sampleFormat = SIDEMU_UNSIGNED_PCM;
			break;
		}
		break;

	case XS_RES_16BIT:
	default:
		switch (myStatus->audioFormat) {
		case FMT_U16_NE:
		case FMT_U16_LE:
		case FMT_U16_BE:
			myStatus->audioFormat = FMT_U16_NE;
			myEngine->currConfig.sampleFormat = SIDEMU_UNSIGNED_PCM;
			break;

		case FMT_S16_NE:
		case FMT_S16_LE:
		case FMT_S16_BE:
		default:
			myStatus->audioFormat = FMT_S16_NE;
			myEngine->currConfig.sampleFormat = SIDEMU_SIGNED_PCM;
			break;
		}
		break;
	}

	/* Clockspeed settings */
	switch (xs_cfg.clockSpeed) {
	case XS_CLOCK_NTSC:
		myEngine->currConfig.clockSpeed = SIDTUNE_CLOCK_NTSC;
		break;

	case XS_CLOCK_PAL:
	default:
		myEngine->currConfig.clockSpeed = SIDTUNE_CLOCK_PAL;
		xs_cfg.clockSpeed = XS_CLOCK_PAL;
		break;
	}

	myEngine->currConfig.forceSongSpeed = xs_cfg.forceSpeed;
	
	
	/* Configure rest of the emulation */
	/* if (xs_cfg.forceModel) */
	myEngine->currConfig.mos8580 = xs_cfg.mos8580;
	myEngine->currConfig.emulateFilter = xs_cfg.emulateFilters;
	myEngine->currConfig.filterFs = xs_cfg.filterFs;
	myEngine->currConfig.filterFm = xs_cfg.filterFm;
	myEngine->currConfig.filterFt = xs_cfg.filterFt;


	/* Now set the emulator configuration */
	if (!myEngine->currEng->setConfig(myEngine->currConfig)) {
		XSERR("Emulator engine configuration failed!\n");
		return FALSE;
	}

	return TRUE;
}


/* Close SIDPlay1 engine
 */
void xs_sidplay1_close(t_xs_status * myStatus)
{
	t_xs_sidplay1 *myEngine;
	assert(myStatus);

	myEngine = (t_xs_sidplay1 *) myStatus->sidEngine;

	/* Free internals */
	if (myEngine->currEng) {
		delete myEngine->currEng;
		myEngine->currEng = NULL;
	}

	g_free(myEngine);
	myStatus->sidEngine = NULL;

	if ( myStatus->buffer != NULL ) {
		g_free(myStatus->buffer);
		myStatus->buffer = NULL;
	}
}


/* Initialize current song and sub-tune
 */
gboolean xs_sidplay1_initsong(t_xs_status * myStatus)
{
	t_xs_sidplay1 *myEngine;
	assert(myStatus);

	myEngine = (t_xs_sidplay1 *) myStatus->sidEngine;
	if (!myEngine) return FALSE;

	if (!myEngine->currTune) {
		XSERR("Tune was NULL\n");
		return FALSE;
	}

	if (!myEngine->currTune->getStatus()) {
		XSERR("Tune status check failed\n");
		return FALSE;
	}

	return sidEmuInitializeSong(*myEngine->currEng, *myEngine->currTune, myStatus->currSong);
}


/* Emulate and render audio data to given buffer
 */
guint xs_sidplay1_fillbuffer(t_xs_status * myStatus, gchar * audioBuffer, guint audioBufSize)
{
	t_xs_sidplay1 *myEngine;
	assert(myStatus);

	myEngine = (t_xs_sidplay1 *) myStatus->sidEngine;
	if (!myEngine) return 0;

	sidEmuFillBuffer(*myEngine->currEng, *myEngine->currTune, audioBuffer, audioBufSize);

	return audioBufSize;
}


/* Load a given SID-tune file
 */
gboolean xs_sidplay1_loadsid(t_xs_status * myStatus, gchar * pcFilename)
{
	t_xs_sidplay1 *myEngine;
	sidTune *newTune;
	assert(myStatus);
	glong buffer_size = 0;

	myEngine = (t_xs_sidplay1 *) myStatus->sidEngine;
	if (!myEngine) return FALSE;

	/* Try to load the file/tune */
	if (!pcFilename) return FALSE;

	if ( xs_sidplay1_load_file_in_buffer( pcFilename , &myStatus->buffer , &buffer_size ) != 0 )
		return FALSE;

	newTune = new sidTune((ubyte*)myStatus->buffer,(udword)buffer_size);
	if (!newTune) return FALSE;

	myEngine->currTune = newTune;

	return TRUE;
}


/* Delete INTERNAL information
 */
void xs_sidplay1_deletesid(t_xs_status * myStatus)
{
	t_xs_sidplay1 *myEngine;
	assert(myStatus);

	myEngine = (t_xs_sidplay1 *) myStatus->sidEngine;
	if (!myEngine) return;

	if (myEngine->currTune) {
		delete myEngine->currTune;
		myEngine->currTune = NULL;
	}
}


/* Return song information
 */
#define TFUNCTION	xs_sidplay1_getsidinfo
#define TTUNEINFO	sidTuneInfo
#define TTUNE		sidTune
#define TBUFFGETFUNC	xs_sidplay1_load_file_in_buffer
#define TBUFFTYPEMEM	ubyte*
#define TBUFFTYPESIZE	udword
#include "xs_sidplay.h"

}				/* extern "C" */
#endif				/* HAVE_SIDPLAY1 */
