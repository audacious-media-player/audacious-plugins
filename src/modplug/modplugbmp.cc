/* Modplug XMMS Plugin
 * Authors: Kenton Varda <temporal@gauge3d.org>
 *
 * This source code is public domain.
 */

#include "modplugbmp.h"

#include <fstream>
#include <stdint.h>
#include <sys/types.h>
#include <math.h>

#include <libmodplug/stdafx.h>
#include <libmodplug/sndfile.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>

#include "archive/open.h"

using namespace std;

// ModplugXMMS member functions ===============================

bool ModplugXMMS::init ()
{
    load_settings ();
    apply_settings ();
    return true;
}

bool ModplugXMMS::is_our_file (const char * filename, VFSFile & file)
{
    string lExt;
    uint32_t lPos;
    const int magicSize = 32;
    char magic[magicSize];

    if (file.fread (magic, 1, magicSize) < magicSize)
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
    if (!memcmp(magic, MMD0_MAGIC, 4))
        return true;
    if (!memcmp(magic, MMD1_MAGIC, 4))
        return true;
    if (!memcmp(magic, OKTA_MAGIC, 8))
        return true;

    if (file.fseek (44, VFS_SEEK_SET))
        return false;
    if (file.fread (magic, 1, 4) < 4)
        return false;
    if (!memcmp(magic, S3M_MAGIC, 4))
        return true;
    if (!memcmp(magic, PTM_MAGIC, 4))
        return true;

    if (file.fseek (1080, VFS_SEEK_SET))
        return false;
    if (file.fread (magic, 1, 4) < 4)
        return false;

    // Check for Fast Tracker multichannel modules (xCHN, xxCH)
    if (magic[1] == 'C' && magic[2] == 'H' && magic[3] == 'N') {
        if (magic[0] == '6' || magic[0] == '8')
            return true;
    }
    if (magic[2] == 'C' && magic[3] == 'H' && magic[0] >= '0' && magic[0] <= '9'
     && magic[1] >= '0' && magic[1] <= '9') {
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
    string aFilename = filename;
    lPos = aFilename.find_last_of('.');
    if((int)lPos == -1)
        return false;
    lExt = aFilename.substr(lPos);
    for(uint32_t i = 0; i < lExt.length(); i++)
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

void ModplugXMMS::PlayLoop()
{
    uint32_t lLength;

    while (! check_stop ())
    {
        int seek_time = check_seek ();
        if (seek_time != -1)
            mSoundFile->SetCurrentPos (seek_time * (int64_t)
             mSoundFile->GetMaxPosition () / (mSoundFile->GetSongTime () * 1000));

        lLength = mSoundFile->Read (mBuffer, mBufSize);

        if (! lLength)
            break;

        if(mModProps.mPreamp)
        {
            //apply preamp
            if(mModProps.mBits == 16)
            {
                unsigned n = mBufSize >> 1;
                for(unsigned i = 0; i < n; i++) {
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
                for(unsigned i = 0; i < mBufSize; i++) {
                    unsigned char old = ((unsigned char*)mBuffer)[i];
                    ((unsigned char*)mBuffer)[i] *= (short int)mPreampFactor;
                    // detect overflow and clip!
                    if ((old & 0x80) !=
                     (((unsigned char*)mBuffer)[i] & 0x80))
                      ((unsigned char*)mBuffer)[i] = old | 0x7F;
                }
            }
        }

        write_audio (mBuffer, mBufSize);
    }
}

bool ModplugXMMS::play (const char * filename, VFSFile & file)
{
    //open and mmap the file
    mArchive = OpenArchive(filename);
    if(mArchive->Size() == 0)
    {
        delete mArchive;
        return false;
    }

    mSoundFile = new CSoundFile;

    //find buftime to get approx. 512 samples/block
    mBufTime = 512000 / mModProps.mFrequency + 1;

    mBufSize = mBufTime;
    mBufSize *= mModProps.mFrequency;
    mBufSize /= 1000;    //milliseconds
    mBufSize *= mModProps.mChannels;
    mBufSize *= mModProps.mBits / 8;

    mBuffer = new unsigned char[mBufSize];

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
        (unsigned char*)mArchive->Map(),
        mArchive->Size()
    );

    set_stream_bitrate(mSoundFile->GetNumChannels() * 1000);

    int fmt = (mModProps.mBits == 16) ? FMT_S16_NE : FMT_U8;
    open_audio(fmt, mModProps.mFrequency, mModProps.mChannels);

    PlayLoop();

    delete[] mBuffer;
    mBuffer = nullptr;
    delete mSoundFile;
    mSoundFile = nullptr;
    delete mArchive;
    mArchive = nullptr;

    return true;
}

bool ModplugXMMS::read_tag (const char * filename, VFSFile & file, Tuple & tuple,
 Index<char> * image)
{
    CSoundFile* lSoundFile;
    Archive* lArchive;
    const char *tmps;

    //open and mmap the file
    lArchive = OpenArchive(filename);
    if(lArchive->Size() == 0)
    {
        delete lArchive;
        return false;
    }

    lSoundFile = new CSoundFile;
    lSoundFile->Create((unsigned char*)lArchive->Map(), lArchive->Size());

    switch(lSoundFile->GetType())
        {
    case MOD_TYPE_MOD:  tmps = "ProTracker"; break;
    case MOD_TYPE_S3M:  tmps = "Scream Tracker 3"; break;
    case MOD_TYPE_XM:   tmps = "Fast Tracker 2"; break;
    case MOD_TYPE_IT:   tmps = "Impulse Tracker"; break;
    case MOD_TYPE_MED:  tmps = "OctaMed"; break;
    case MOD_TYPE_MTM:  tmps = "MultiTracker Module"; break;
    case MOD_TYPE_669:  tmps = "669 Composer / UNIS 669"; break;
    case MOD_TYPE_ULT:  tmps = "Ultra Tracker"; break;
    case MOD_TYPE_STM:  tmps = "Scream Tracker"; break;
    case MOD_TYPE_FAR:  tmps = "Farandole"; break;
    case MOD_TYPE_AMF:  tmps = "ASYLUM Music Format"; break;
    case MOD_TYPE_AMS:  tmps = "AMS module"; break;
    case MOD_TYPE_DSM:  tmps = "DSIK Internal Format"; break;
    case MOD_TYPE_MDL:  tmps = "DigiTracker"; break;
    case MOD_TYPE_OKT:  tmps = "Oktalyzer"; break;
    case MOD_TYPE_DMF:  tmps = "Delusion Digital Music Fileformat (X-Tracker)"; break;
    case MOD_TYPE_PTM:  tmps = "PolyTracker"; break;
    case MOD_TYPE_DBM:  tmps = "DigiBooster Pro"; break;
    case MOD_TYPE_MT2:  tmps = "MadTracker 2"; break;
    case MOD_TYPE_AMF0: tmps = "AMF0"; break;
    case MOD_TYPE_PSM:  tmps = "Protracker Studio Module"; break;
    default:        tmps = "ModPlug unknown"; break;
    }
    tuple.set_str (Tuple::Codec, tmps);
    tuple.set_str (Tuple::Quality, _("sequenced"));
    tuple.set_int (Tuple::Length, lSoundFile->GetSongTime() * 1000);

    const char *tmps2 = lSoundFile->GetTitle();
    // Chop any leading spaces off. They are annoying in the playlist.
    while (tmps2[0] == ' ')
        tmps2++;
    if (tmps2[0])
        tuple.set_str(Tuple::Title, tmps2);

    //unload the file
    lSoundFile->Destroy();
    delete lSoundFile;
    delete lArchive;
    return true;
}

void ModplugXMMS::apply_settings ()
{
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
}
