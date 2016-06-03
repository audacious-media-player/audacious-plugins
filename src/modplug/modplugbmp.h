/* Modplug XMMS Plugin
 * Authors: Kenton Varda <temporal@gauge3d.org>
 *
 * This source code is public domain.
 */

#ifndef __MODPLUGXMMS_CMODPLUGXMMS_H_INCLUDED__
#define __MODPLUGXMMS_CMODPLUGXMMS_H_INCLUDED__

#include <libaudcore/i18n.h>
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
#define PTM_MAGIC   "PTMF"  /* Same position for Polytracker magic string */

/* These nicer formats have the magic bytes at the front of the file where they belong */
#define UMX_MAGIC   "\xC1\x83\x2A\x9E"
#define XM_MAGIC    "Exte"              /* Exte(nded Module) */
#define M669_MAGIC  "if  "              /* Last two bytes are bogus, and not checked */
#define IT_MAGIC    "IMPM"              /* IMPM */
#define MTM_MAGIC   "\x4D\x54\x4D\x10"
#define PSM_MAGIC   "PSM "
#define MMD0_MAGIC  "MMD0"              /* Med MoDule 0 */
#define MMD1_MAGIC  "MMD1"              /* OctaMED Professional */
#define OKTA_MAGIC  "OKTASONG"          /* Oktalyzer */

class CSoundFile;
class Archive;

struct PreferencesWidget;

class ModplugXMMS : public InputPlugin
{
public:
    static const char * const exts[];
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("ModPlug (Module Player)"),
        PACKAGE,
        nullptr,
        & prefs
    };

    constexpr ModplugXMMS () : InputPlugin (info, InputInfo (FlagSubtunes)
        .with_exts (exts)) {}

    bool init ();

    bool is_our_file (const char * filename, VFSFile & file);
    bool read_tag (const char * filename, VFSFile & file, Tuple & tuple, Index<char> * image);
    bool play (const char * filename, VFSFile & file);

private:
    unsigned char * mBuffer = nullptr;
    uint32_t mBufSize = 0;

    ModplugSettings mModProps {};

    uint32_t mBufTime = 0; // milliseconds

    CSoundFile * mSoundFile = nullptr;
    Archive * mArchive = nullptr;

    float mPreampFactor = 0;

    void load_settings ();
    void apply_settings ();

    void PlayLoop();
};

#endif //included
