/* Modplug XMMS Plugin
 * Authors: Kenton Varda <temporal@gauge3d.org>
 *
 * This source code is public domain.
 */

#ifndef __MODPLUGXMMS_CMODPLUGXMMS_H_INCLUDED__
#define __MODPLUGXMMS_CMODPLUGXMMS_H_INCLUDED__

#include <string>

#include <libaudcore/plugin.h>

#include "settings.h"

/* Module files have their magic deep inside the file, at offset 1080; source: http://www.onicos.com/staff/iz/formats/mod.html and information by Michael Doering from UADE */
#define MOD_MAGIC_PROTRACKER4   "M.K."  // Protracker 4 channel
#define MOD_MAGIC_PROTRACKER4X  "M!K!"  // Protracker 4 channel
#define MOD_MAGIC_NOISETRACKER  "M&K!"  // Noisetracker 1.3 by Kaktus & Mahoney
#define MOD_MAGIC_STARTRACKER4  "FLT4"  // Startracker 4 channel (Startrekker/AudioSculpture)
#define MOD_MAGIC_STARTRACKER8  "FLT8"  // Startracker 8 channel (Startrekker/AudioSculpture)
#define MOD_MAGIC_STARTRACKER4X "EX04"  // Startracker 4 channel (Startrekker/AudioSculpture)
#define MOD_MAGIC_STARTRACKER8X "EX08"  // Startracker 8 channel (Startrekker/AudioSculpture)
#define MOD_MAGIC_FASTTRACKER4  "4CHN"  // Fasttracker 4 channel
#define MOD_MAGIC_OKTALYZER8    "CD81"  // Atari oktalyzer 8 channel
#define MOD_MAGIC_OKTALYZER8X   "OKTA"  // Atari oktalyzer 8 channel
#define MOD_MAGIC_TAKETRACKER16 "16CN"  // Taketracker 16 channel
#define MOD_MAGIC_TAKETRACKER32 "32CN"  // Taketracker 32 channel

#define S3M_MAGIC   "SCRM"  /* This is the SCRM string at offset 44 to 47 in the S3M header */

/* These nicer formats have the magic bytes at the front of the file where they belong */
#define UMX_MAGIC   "\xC1\x83\x2A\x9E"
#define XM_MAGIC    "Exte"              /* Exte(nded Module) */
#define M669_MAGIC  "if  "              /* Last two bytes are bogus, and not checked */
#define IT_MAGIC    "IMPM"              /* IMPM */
#define MTM_MAGIC   "\x4D\x54\x4D\x10"
#define PSM_MAGIC   "PSM "

class CSoundFile;
class Archive;

class ModplugXMMS
{
public:
    ModplugXMMS();
    ~ModplugXMMS();

    bool CanPlayFileFromVFS(const std::string& aFilename, VFSFile &file);

    bool PlayFile(const std::string& aFilename);

    Tuple GetSongTuple(const std::string& aFilename);

    void SetModProps(const ModplugSettings& aModProps);

private:
    unsigned char*  mBuffer;
    uint32_t  mBufSize;

    ModplugSettings mModProps;

    uint32_t  mBufTime;     //milliseconds

    CSoundFile* mSoundFile;
    Archive*    mArchive;

    float mPreampFactor;

    void PlayLoop();
};

#endif //included
