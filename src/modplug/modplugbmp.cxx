/* Modplug XMMS Plugin
 * Authors: Kenton Varda <temporal@gauge3d.org>
 *
 * This source code is public domain.
 */

#include <fstream>
#include <unistd.h>
#include <math.h>

extern "C" {
#include <audacious/debug.h>
#include <audacious/misc.h>
}

#include "modplugbmp.h"
#include "stdafx.h"
#include "sndfile.h"
#include "stddefs.h"
#include "archive/open.h"

static gboolean stop_flag = FALSE;

// ModplugXMMS member functions ===============================

// operations ----------------------------------------
ModplugXMMS::ModplugXMMS()
{
	mSoundFile = new CSoundFile;
    control_mutex = g_mutex_new ();
    control_cond = g_cond_new ();
}
ModplugXMMS::~ModplugXMMS()
{
	delete mSoundFile;
    g_mutex_free (control_mutex);
    g_cond_free (control_cond);
}

static const gchar * const modplug_defaults[] = {
 "Surround", "TRUE",
 "Oversampling", "TRUE",
 "Megabass", "FALSE",
 "NoiseReduction", "TRUE",
 "VolumeRamp", "TRUE",
 "Reverb", "FALSE",
 "Fastinfo", "TRUE",
 "UseFileName", "FALSE",
 "GrabAmigaMOD", "TRUE",
 "PreAmp", "FALSE",
 "PreAmpLevel", "0",
 "Channels", "2",
 "Bits", "16",
 "Frequency", "44100",
 "ResamplingMode", "3", /* SRCMODE_POLYPHASE */
 "ReverbDepth", "30",
 "ReverbDelay", "100",
 "BassAmount", "40",
 "BassRange", "30",
 "SurroundDepth", "20",
 "SurroundDelay", "20",
 "LoopCount", "0",
 NULL};

void ModplugXMMS::Init(void)
{
	aud_config_set_defaults (MODPLUG_CFGID, modplug_defaults);

	mModProps.mSurround = aud_get_bool (MODPLUG_CFGID, "Surround");
	mModProps.mOversamp = aud_get_bool (MODPLUG_CFGID, "Oversampling");
	mModProps.mMegabass = aud_get_bool (MODPLUG_CFGID, "Megabass");
	mModProps.mNoiseReduction = aud_get_bool (MODPLUG_CFGID, "NoiseReduction");
	mModProps.mVolumeRamp = aud_get_bool (MODPLUG_CFGID, "VolumeRamp");
	mModProps.mReverb = aud_get_bool (MODPLUG_CFGID, "Reverb");
	mModProps.mFastinfo = aud_get_bool (MODPLUG_CFGID, "FastInfo");
	mModProps.mUseFilename = aud_get_bool (MODPLUG_CFGID, "UseFileName");
	mModProps.mGrabAmigaMOD = aud_get_bool (MODPLUG_CFGID, "GrabAmigaMOD");
	mModProps.mPreamp = aud_get_bool (MODPLUG_CFGID, "PreAmp");
	mModProps.mPreampLevel = aud_get_double (MODPLUG_CFGID, "PreAmpLevel");
	mModProps.mChannels = aud_get_int (MODPLUG_CFGID, "Channels");
	mModProps.mBits = aud_get_int (MODPLUG_CFGID, "Bits");
	mModProps.mFrequency = aud_get_int (MODPLUG_CFGID, "Frequency");
	mModProps.mResamplingMode = aud_get_int (MODPLUG_CFGID, "ResamplineMode");
	mModProps.mReverbDepth = aud_get_int (MODPLUG_CFGID, "ReverbDepth");
	mModProps.mReverbDelay = aud_get_int (MODPLUG_CFGID, "ReverbDelay");
	mModProps.mBassAmount = aud_get_int (MODPLUG_CFGID, "BassAmount");
	mModProps.mBassRange = aud_get_int (MODPLUG_CFGID, "BassRange");
	mModProps.mSurroundDepth = aud_get_int (MODPLUG_CFGID, "SurroundDepth");
	mModProps.mSurroundDelay = aud_get_int (MODPLUG_CFGID, "SurroundDelay");
	mModProps.mLoopCount = aud_get_int (MODPLUG_CFGID, "LoopCount");
}

bool ModplugXMMS::CanPlayFileFromVFS(const string& aFilename, VFSFile *file)
{
	string lExt;
	uint32 lPos;
	const int magicSize = 32;
	char magic[magicSize];

	if (vfs_fread(magic, 1, magicSize, file) < magicSize)
		return false;
	if (!memcmp(magic, UMX_MAGIC, 4))
		return true;
	if (!memcmp(magic, "Extended Module:", 16))
		return true;
	if (!memcmp(magic, M669_MAGIC, 2))
		return true;
	if (!memcmp(magic, IT_MAGIC, 4))
		return true;
	if (!memcmp(magic, MTM_MAGIC, 4))
		return true;
	if (!memcmp(magic, PSM_MAGIC, 4))
		return true;

	if (vfs_fseek(file, 44, SEEK_SET))
		return false;
	if (vfs_fread(magic, 1, 4, file) < 4)
		return false;
	if (!memcmp(magic, S3M_MAGIC, 4))
		return true;

	if (vfs_fseek(file, 1080, SEEK_SET))
		return false;
	if (vfs_fread(magic, 1, 4, file) < 4)
		return false;

	// Check for Fast Tracker multichannel modules (xCHN, xxCH)
	if (magic[1] == 'C' && magic[2] == 'H' && magic[3] == 'N') {
		if (magic[0] == '6' || magic[0] == '8')
			return true;
	}
	if (magic[2] == 'C' && magic[3] == 'H' && isdigit(magic[0]) && isdigit(magic[1])) {
		int nch = (magic[0] - '0') * 10 + (magic[1] - '0');
		if ((nch % 2 == 0) && nch >= 10)
			return true;
	}

	// Check for Amiga MOD module formats
	if(mModProps.mGrabAmigaMOD) {
	if (!memcmp(magic, MOD_MAGIC_PROTRACKER4, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_PROTRACKER4X, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_NOISETRACKER, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_STARTRACKER4, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_STARTRACKER8, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_STARTRACKER4X, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_STARTRACKER8X, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_FASTTRACKER4, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_OKTALYZER8, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_OKTALYZER8X, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_TAKETRACKER16, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_TAKETRACKER32, 4))
		return true;
	} /* end of if(mModProps.mGrabAmigaMOD) */

	/* We didn't find the magic bytes, fall back to extension check */
	lPos = aFilename.find_last_of('.');
	if((int)lPos == -1)
		return false;
	lExt = aFilename.substr(lPos);
	for(uint32 i = 0; i < lExt.length(); i++)
		lExt[i] = tolower(lExt[i]);

	if (lExt == ".amf")
		return true;
	if (lExt == ".ams")
		return true;
	if (lExt == ".dbm")
		return true;
	if (lExt == ".dbf")
		return true;
	if (lExt == ".dmf")
		return true;
	if (lExt == ".dsm")
		return true;
	if (lExt == ".far")
		return true;
	if (lExt == ".mdl")
		return true;
	if (lExt == ".stm")
		return true;
	if (lExt == ".ult")
		return true;
	if (lExt == ".mt2")
		return true;

	return false;
}

void ModplugXMMS::PlayLoop(InputPlayback *playback)
{
	uint32 lLength;

    g_mutex_lock (control_mutex);
    seek_time = -1;
    stop_flag = FALSE;
    playback->set_pb_ready (playback);
    g_mutex_unlock (control_mutex);

    while (1)
    {
        g_mutex_lock (control_mutex);

        if (stop_flag)
        {
            g_mutex_unlock (control_mutex);
            break;
        }

        if (seek_time != -1)
        {
            mSoundFile->SetCurrentPos (seek_time * (gint64)
             mSoundFile->GetMaxPosition () / (mSoundFile->GetSongTime () * 1000));
            playback->output->flush (seek_time);
            seek_time = -1;
            g_cond_signal (control_cond);
        }

        g_mutex_unlock (control_mutex);

        lLength = mSoundFile->Read (mBuffer, mBufSize);

        if (! lLength)
            break;

		if(mModProps.mPreamp)
		{
			//apply preamp
			if(mModProps.mBits == 16)
			{
				uint n = mBufSize >> 1;
				for(uint i = 0; i < n; i++) {
					short old = ((short*)mBuffer)[i];
					((short*)mBuffer)[i] *= (short int)mPreampFactor;
					// detect overflow and clip!
					if ((old & 0x8000) !=
					 (((short*)mBuffer)[i] & 0x8000))
					  ((short*)mBuffer)[i] = old | 0x7FFF;

				}
			}
			else
			{
				for(uint i = 0; i < mBufSize; i++) {
					uchar old = ((uchar*)mBuffer)[i];
					((uchar*)mBuffer)[i] *= (short int)mPreampFactor;
					// detect overflow and clip!
					if ((old & 0x80) !=
					 (((uchar*)mBuffer)[i] & 0x80))
					  ((uchar*)mBuffer)[i] = old | 0x7F;
				}
			}
		}

        playback->output->write_audio (mBuffer, mBufSize);
	}

    g_mutex_lock (control_mutex);

    while (!stop_flag && playback->output->buffer_playing ())
        g_usleep (10000);

    stop_flag = TRUE;
    g_cond_signal (control_cond); /* wake up any waiting request */
    g_mutex_unlock (control_mutex);

	//Unload the file
	mSoundFile->Destroy();
	delete mArchive;

	if (mBuffer)
	{
		delete [] mBuffer;
		mBuffer = NULL;
	}
}

bool ModplugXMMS::PlayFile(const string& aFilename, InputPlayback *ipb)
{
	//open and mmap the file
	mArchive = OpenArchive(aFilename);
	if(mArchive->Size() == 0)
	{
		delete mArchive;
		return true;
	}

	if (mBuffer)
		delete [] mBuffer;

	//find buftime to get approx. 512 samples/block
	mBufTime = 512000 / mModProps.mFrequency + 1;

	mBufSize = mBufTime;
	mBufSize *= mModProps.mFrequency;
	mBufSize /= 1000;    //milliseconds
	mBufSize *= mModProps.mChannels;
	mBufSize *= mModProps.mBits / 8;

	mBuffer = new uchar[mBufSize];
	if(!mBuffer)
		return true;        //out of memory!

	CSoundFile::SetWaveConfig
	(
		mModProps.mFrequency,
		mModProps.mBits,
		mModProps.mChannels
	);
	CSoundFile::SetWaveConfigEx
	(
		mModProps.mSurround,
		!mModProps.mOversamp,
		mModProps.mReverb,
		true,
		mModProps.mMegabass,
		mModProps.mNoiseReduction,
		false
	);

	// [Reverb level 0(quiet)-100(loud)], [delay in ms, usually 40-200ms]
	if(mModProps.mReverb)
	{
		CSoundFile::SetReverbParameters
		(
			mModProps.mReverbDepth,
			mModProps.mReverbDelay
		);
	}
	// [XBass level 0(quiet)-100(loud)], [cutoff in Hz 10-100]
	if(mModProps.mMegabass)
	{
		CSoundFile::SetXBassParameters
		(
			mModProps.mBassAmount,
			mModProps.mBassRange
		);
	}
	// [Surround level 0(quiet)-100(heavy)] [delay in ms, usually 5-40ms]
	if(mModProps.mSurround)
	{
		CSoundFile::SetSurroundParameters
		(
			mModProps.mSurroundDepth,
			mModProps.mSurroundDelay
		);
	}
	CSoundFile::SetResamplingMode(mModProps.mResamplingMode);
	mSoundFile->SetRepeatCount(mModProps.mLoopCount);
	mPreampFactor = exp(mModProps.mPreampLevel);

	mSoundFile->Create
	(
		(uchar*)mArchive->Map(),
		mArchive->Size()
	);

    Tuple* ti = GetSongTuple( aFilename );
    if ( ti ) {
        ipb->set_tuple(ipb,ti);
    }

	ipb->set_params(ipb, mSoundFile->GetNumChannels() * 1000, mModProps.mFrequency,	mModProps.mChannels);

	if(mModProps.mBits == 16)
		mFormat = FMT_S16_NE;
	else
		mFormat = FMT_U8;

    if (! ipb->output->open_audio (mFormat, mModProps.mFrequency,
     mModProps.mChannels))
        return true;

	this->PlayLoop(ipb);
	ipb->output->close_audio ();

    return false;
}

void ModplugXMMS::Stop (InputPlayback * playback)
{
	g_mutex_lock(control_mutex);

	if (!stop_flag)
	{
		stop_flag = TRUE;
		playback->output->abort_write();
		g_cond_signal(control_cond);
	}

	g_mutex_unlock(control_mutex);
}

void ModplugXMMS::pause (InputPlayback * playback, gboolean pause)
{
	g_mutex_lock(control_mutex);

	if (!stop_flag)
		playback->output->pause(pause);

	g_mutex_unlock(control_mutex);
}

void ModplugXMMS::mseek (InputPlayback * playback, gint time)
{
    g_mutex_lock (control_mutex);

    if (!stop_flag)
    {
        seek_time = time;
        playback->output->abort_write();
        g_cond_signal (control_cond);
        g_cond_wait (control_cond, control_mutex);
    }

    g_mutex_unlock (control_mutex);
}

Tuple* ModplugXMMS::GetSongTuple(const string& aFilename)
{
	CSoundFile* lSoundFile;
	Archive* lArchive;
	const gchar *tmps;

	//open and mmap the file
        lArchive = OpenArchive(aFilename);
        if(lArchive->Size() == 0)
        {
                delete lArchive;
                return NULL;
        }

	Tuple *ti = tuple_new_from_filename(aFilename.c_str());
	lSoundFile = new CSoundFile;
	lSoundFile->Create((uchar*)lArchive->Map(), lArchive->Size());

	switch(lSoundFile->GetType())
        {
	case MOD_TYPE_MOD:	tmps = "ProTracker"; break;
	case MOD_TYPE_S3M:	tmps = "Scream Tracker 3"; break;
	case MOD_TYPE_XM:	tmps = "Fast Tracker 2"; break;
	case MOD_TYPE_IT:	tmps = "Impulse Tracker"; break;
	case MOD_TYPE_MED:	tmps = "OctaMed"; break;
	case MOD_TYPE_MTM:	tmps = "MultiTracker Module"; break;
	case MOD_TYPE_669:	tmps = "669 Composer / UNIS 669"; break;
	case MOD_TYPE_ULT:	tmps = "Ultra Tracker"; break;
	case MOD_TYPE_STM:	tmps = "Scream Tracker"; break;
	case MOD_TYPE_FAR:	tmps = "Farandole"; break;
	case MOD_TYPE_AMF:	tmps = "ASYLUM Music Format"; break;
	case MOD_TYPE_AMS:	tmps = "AMS module"; break;
	case MOD_TYPE_DSM:	tmps = "DSIK Internal Format"; break;
	case MOD_TYPE_MDL:	tmps = "DigiTracker"; break;
	case MOD_TYPE_OKT:	tmps = "Oktalyzer"; break;
	case MOD_TYPE_DMF:	tmps = "Delusion Digital Music Fileformat (X-Tracker)"; break;
	case MOD_TYPE_PTM:	tmps = "PolyTracker"; break;
	case MOD_TYPE_DBM:	tmps = "DigiBooster Pro"; break;
	case MOD_TYPE_MT2:	tmps = "MadTracker 2"; break;
	case MOD_TYPE_AMF0:	tmps = "AMF0"; break;
	case MOD_TYPE_PSM:	tmps = "Protracker Studio Module"; break;
	default:		tmps = "ModPlug unknown"; break;
	}
	tuple_associate_string(ti, FIELD_CODEC, NULL, tmps);
	tuple_associate_string(ti, FIELD_QUALITY, NULL, "sequenced");
	tuple_associate_int(ti, FIELD_LENGTH, NULL, lSoundFile->GetSongTime() * 1000);

	gchar *tmps2 = MODPLUG_CONVERT(lSoundFile->GetTitle());
	// Chop any leading spaces off. They are annoying in the playlist.
	gchar *tmps3 = tmps2; // Make another pointer so tmps2 can still be free()d
	while ( *tmps3 == ' ' ) tmps3++ ;
	tuple_associate_string(ti, FIELD_TITLE, NULL, tmps3);
	g_free(tmps2);

	//unload the file
	lSoundFile->Destroy();
	delete lSoundFile;
	delete lArchive;
	return ti;
}

void ModplugXMMS::SetInputPlugin(InputPlugin& aInPlugin)
{
	mInPlug = &aInPlugin;
}

const ModplugXMMS::Settings& ModplugXMMS::GetModProps()
{
	return mModProps;
}

const char* ModplugXMMS::Bool2OnOff(bool aValue)
{
	if(aValue)
		return "on";
	else
		return "off";
}

void ModplugXMMS::SetModProps(const Settings& aModProps)
{
	mModProps = aModProps;

	// [Reverb level 0(quiet)-100(loud)], [delay in ms, usually 40-200ms]
	if(mModProps.mReverb)
	{
		CSoundFile::SetReverbParameters
		(
			mModProps.mReverbDepth,
			mModProps.mReverbDelay
		);
	}
	// [XBass level 0(quiet)-100(loud)], [cutoff in Hz 10-100]
	if(mModProps.mMegabass)
	{
		CSoundFile::SetXBassParameters
		(
			mModProps.mBassAmount,
			mModProps.mBassRange
		);
	}
	else //modplug seems to ignore the SetWaveConfigEx() setting for bass boost
	{
		CSoundFile::SetXBassParameters
		(
			0,
			0
		);
	}
	// [Surround level 0(quiet)-100(heavy)] [delay in ms, usually 5-40ms]
	if(mModProps.mSurround)
	{
		CSoundFile::SetSurroundParameters
		(
			mModProps.mSurroundDepth,
			mModProps.mSurroundDelay
		);
	}
	CSoundFile::SetWaveConfigEx
	(
		mModProps.mSurround,
		!mModProps.mOversamp,
		mModProps.mReverb,
		true,
		mModProps.mMegabass,
		mModProps.mNoiseReduction,
		false
	);
	CSoundFile::SetResamplingMode(mModProps.mResamplingMode);
	mPreampFactor = exp(mModProps.mPreampLevel);

	aud_set_bool (MODPLUG_CFGID, "Surround", mModProps.mSurround);
	aud_set_bool (MODPLUG_CFGID, "Oversampling", mModProps.mOversamp);
	aud_set_bool (MODPLUG_CFGID, "Megabass", mModProps.mMegabass);
	aud_set_bool (MODPLUG_CFGID, "NoiseReduction", mModProps.mNoiseReduction);
	aud_set_bool (MODPLUG_CFGID, "VolumeRamp", mModProps.mVolumeRamp);
	aud_set_bool (MODPLUG_CFGID, "Reverb", mModProps.mReverb);
	aud_set_bool (MODPLUG_CFGID, "FastInfo", mModProps.mFastinfo);
	aud_set_bool (MODPLUG_CFGID, "UseFileName", mModProps.mUseFilename);
	aud_set_bool (MODPLUG_CFGID, "GrabAmigaMOD", mModProps.mGrabAmigaMOD);
	aud_set_bool (MODPLUG_CFGID, "PreAmp", mModProps.mPreamp);
	aud_set_double (MODPLUG_CFGID, "PreAmpLevel", mModProps.mPreampLevel);
	aud_set_int (MODPLUG_CFGID, "Channels", mModProps.mChannels);
	aud_set_int (MODPLUG_CFGID, "Bits", mModProps.mBits);
	aud_set_int (MODPLUG_CFGID, "Frequency", mModProps.mFrequency);
	aud_set_int (MODPLUG_CFGID, "ResamplineMode", mModProps.mResamplingMode);
	aud_set_int (MODPLUG_CFGID, "ReverbDepth", mModProps.mReverbDepth);
	aud_set_int (MODPLUG_CFGID, "ReverbDelay", mModProps.mReverbDelay);
	aud_set_int (MODPLUG_CFGID, "BassAmount", mModProps.mBassAmount);
	aud_set_int (MODPLUG_CFGID, "BassRange", mModProps.mBassRange);
	aud_set_int (MODPLUG_CFGID, "SurroundDepth", mModProps.mSurroundDepth);
	aud_set_int (MODPLUG_CFGID, "SurroundDelay", mModProps.mSurroundDelay);
	aud_set_int (MODPLUG_CFGID, "LoopCount", mModProps.mLoopCount);
}

ModplugXMMS gModplugXMMS;
