/*
 * xSF - 2SF Player
 * By Naram Qashat (CyberBotX) [cyberbotx@cyberbotx.com]
 * and Adam Higerd (Coda Highland) [chighland@gmail.com]
 * Last modification on 2014-09-24
 *
 * Based on a modified vio2sf v0.22c
 *
 * Partially based on the vio*sf framework
 *
 * Utilizes a modified DeSmuME v0.9.9 SVN for playback
 * http://desmume.org/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory>
#include <sstream>
#include <iostream>

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/runtime.h>

#include "desmume/NDSSystem.h"
#include "sndif2sf.h"
#include "XSFFile.h"

#if _WIN32
#include <windows.h>
#define sleep Sleep
#else
#include <unistd.h>
#endif

class XSFPlugin : public InputPlugin
{
public:
	static const char *const exts[];
	static const char *const defaults[];
	static const PreferencesWidget widgets[];
	static const PluginPreferences prefs;

	static constexpr PluginInfo info = {
		N_("2SF Decoder"),
		PACKAGE,
		nullptr,
		& prefs
	};

	constexpr XSFPlugin() : InputPlugin(info, InputInfo()
		.with_exts(exts)) {}

	bool init();

	bool is_our_file(const char *filename, VFSFile &file);
	bool read_tag(const char *filename, VFSFile &file, Tuple &tuple, Index<char> *image);
	bool play(const char *filename, VFSFile &file);
};

EXPORT XSFPlugin aud_plugin_instance;

class vfsfile_istream : public std::istream {
  class vfsfile_streambuf : public std::basic_streambuf<char> {
  public:
    vfsfile_streambuf(const char* filename) : source(new VFSFile(filename, "rb")), owned(source) {}
    vfsfile_streambuf(VFSFile* source) : source(source), owned(nullptr) {}

  protected:
    std::streamsize xsgetn(char* s, std::streamsize count) {
      return source->fread(s, 1, count);
    }

    traits_type::int_type underflow() {
      if (source && *source) {
        uint8_t result[1];
        int ok = source->fread(reinterpret_cast<char*>(result), 1, 1);
        if (ok) {
          if (source->fseek(-1, VFS_SEEK_CUR)) {
            return traits_type::eof();
          }
          return result[0];
        }
      } else {
      }
      return traits_type::eof();
    }

    traits_type::int_type uflow() {
      if (source && *source) {
        uint8_t result[1];
        int ok = source->fread(reinterpret_cast<char*>(result), 1, 1);
        if (ok) {
          return result[0];
        }
      }
      return traits_type::eof();
    }

    traits_type::pos_type seekoff(traits_type::off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which = std::ios_base::in) {
      if (!source || !*source) {
        return traits_type::off_type(-1);
      }
      int err = 1;
      if (dir == std::ios_base::beg) {
        err = source->fseek(off, VFS_SEEK_SET);
      } else if (dir == std::ios_base::end) {
        err = source->fseek(off, VFS_SEEK_END);
      } else {
        err = source->fseek(off, VFS_SEEK_CUR);
      }
      if (err) {
        return traits_type::off_type(-1);
      }
      return source->ftell();
    }

    traits_type::pos_type seekpos(traits_type::pos_type pos, std::ios_base::openmode which = std::ios_base::in) {
      return seekoff(pos, std::ios_base::beg, which);
    }

  private:
    VFSFile* source;
    std::unique_ptr<VFSFile> owned;
  };

public:
  vfsfile_istream(const char* source) : std::istream(new vfsfile_streambuf(source)) {}
  vfsfile_istream(VFSFile* source) : std::istream(new vfsfile_streambuf(source)) {}
  ~vfsfile_istream() { delete rdbuf(nullptr); }
};

/* xsf_get_lib: called to load secondary files */
static String dirpath;

#define CFG_ID "xsf"

const char* const XSFPlugin::defaults[] =
{
	"ignore_length", "FALSE",
  "fade", "5000",
  "sample_rate", "32728",
  "interpolation_mode", "none",
	nullptr
};

bool XSFPlugin::init()
{
	aud_config_set_defaults(CFG_ID, defaults);
	return true;
}

Index<char> xsf_get_lib(char *filename)
{
	VFSFile file(filename_build({dirpath, filename}), "r");
	return file ? file.read_all() : Index<char>();
}

bool XSFPlugin::read_tag(const char *filename, VFSFile &file, Tuple &tuple, Index<char> *image)
{
  try {
    vfsfile_istream vs(&file);
    if (!vs) {
      return false;
    }
    XSFFile xsf(vs, 0, 0, true);

    tuple.set_int(Tuple::Length, xsf.GetLengthMS(115000) + xsf.GetFadeMS(5000));
    tuple.set_str(Tuple::Artist, xsf.GetTagValue("artist").c_str());
    tuple.set_str(Tuple::Album, xsf.GetTagValue("game").c_str());
    tuple.set_str(Tuple::Title, xsf.GetTagValue("title").c_str());
    tuple.set_str(Tuple::Copyright, xsf.GetTagValue("copyright").c_str());
    tuple.set_str(Tuple::Quality, _("sequenced"));
    tuple.set_str(Tuple::Codec, "Nintendo DS Audio");
    if (xsf.GetTagExists("replaygain_album_gain")) {
      tuple.set_int(Tuple::AlbumGain, xsf.GetTagValue<double>("replaygain_album_gain", 1.0) * 1000);
      tuple.set_int(Tuple::AlbumPeak, xsf.GetTagValue<double>("replaygain_album_peak", 1.0) * 1000);
      tuple.set_int(Tuple::TrackGain, xsf.GetTagValue<double>("replaygain_track_gain", 1.0) * 1000);
      tuple.set_int(Tuple::TrackPeak, xsf.GetTagValue<double>("replaygain_track_peak", 1.0) * 1000);
      tuple.set_int(Tuple::GainDivisor, 1000);
      tuple.set_int(Tuple::PeakDivisor, 1000);
    }

    return true;
  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    return false;
  }
}

static void xsf_reset(int frameSkip)
{
  execute = false;
  NDS_Reset();
  execute = true;

  if (frameSkip > 0) {
    for (int i = 0; i < frameSkip; ++i) {
      NDS_exec<false>();
    }
  }
  buffer_rope.clear();
}

bool map2SF(std::vector<uint8_t>& rom, XSFFile* xsf)
{
  if (!xsf->IsValidType(0x24))
    return false;
  const auto& programSection = xsf->GetProgramSection();
  if (programSection.size()) {
    uint32_t offset = Get32BitsLE(&programSection[0]);
    uint32_t size = Get32BitsLE(&programSection[4]);
    uint32_t finalSize = size + offset;
    if (rom.size() < finalSize) {
      rom.resize(finalSize + 10);
    }
    memcpy(&rom[offset], &programSection[8], size);
  }
  return true;
}

bool recursiveLoad2SF(std::vector<uint8_t>& rom, XSFFile* xsf, int level)
{
  if (level <= 10 && xsf->GetTagExists("_lib"))
  {
    vfsfile_istream vs(filename_build({ dirpath, xsf->GetTagValue("_lib").c_str() }));
    if (!vs)
      return false;
    XSFFile libxsf(vs, 4, 8);
    if (!recursiveLoad2SF(rom, &libxsf, level + 1))
      return false;
  }

  if (!map2SF(rom, xsf))
    return false;

  bool found = true;
  for (int n = 2; found; n++) {
    std::ostringstream ss;
    ss << "_lib" << (n++);
    found = xsf->GetTagExists(ss.str());
    if (found) {
      vfsfile_istream vs(filename_build({ dirpath, xsf->GetTagValue(ss.str()).c_str() }));
      if (!vs)
        return false;
      XSFFile libxsf(vs, 4, 8);
      if (!recursiveLoad2SF(rom, &libxsf, level + 1))
        return false;
    }
  }
  return true;
}

bool XSFPlugin::play(const char *filename, VFSFile &file)
{
	int length = -1;
	bool error = false;
  int fade = aud_get_int(CFG_ID, "fade");
  int frameSkip = -1;
	float pos = 0.0;
  std::string interp = (const char*)aud_get_str(CFG_ID, "interpolation_mode");
  int interpMode = 0;
  if (interp == "linear") {
    interpMode = 1;
  } else if (interp == "cosine") {
    interpMode = 2;
  } else if (interp == "sharp") {
    interpMode = 3;
  }
  CommonSettings.spuInterpolationMode = (SPUInterpolationMode)interpMode;

	const char * slash = strrchr(filename, '/');
	if (!slash)
		return false;

  while (execute && !check_stop()) {
    std::cerr << "waiting for thread to finish..." << std::endl;
    sleep(100);
  }

	dirpath = String(str_copy(filename, slash + 1 - filename));

	Index<char> buf = file.read_all();
  try {
    vfsfile_istream vs(&file);
    if (!vs) {
      return false;
    }

    XSFFile xsf(vs, 4, 8);
    fade = xsf.GetFadeMS(5000);
    length = xsf.GetLengthMS(115000) + fade;

    std::vector<uint8_t> rom;
    if (!recursiveLoad2SF(rom, &xsf, 0) || !rom.size())
      return false;

    if (NDS_Init())
      return false;

    int sampleRate = aud_get_int(CFG_ID, "sample_rate");
    if (sampleRate < 11025 || sampleRate > 96000)
      sampleRate = 32728;
    SetDesmumeSampleRate(sampleRate); // TODO: config
    int BUFFERSIZE = DESMUME_SAMPLE_RATE / 59.837; //truncates to 737, the traditional value, for 44100
    SPU_ChangeSoundCore(SNDIFID_2SF, BUFFERSIZE);

    execute = false;

    MMU_unsetRom();
    NDS_SetROM(rom.data(), rom.size());
    gameInfo.loadData((char*)rom.data(), rom.size());

    frameSkip = xsf.GetTagValue<int>("_frames", -1);
    CommonSettings.rigorous_timing = true;
    CommonSettings.spu_advanced = true;
    CommonSettings.advanced_timing = true;

    xsf_reset(frameSkip);

    set_stream_bitrate(DESMUME_SAMPLE_RATE*2*2*8);
    open_audio(FMT_S16_NE, DESMUME_SAMPLE_RATE, 2);

    bool ignore_length = aud_get_bool(CFG_ID, "ignore_length");
    while (!check_stop() && (pos < length || ignore_length))
    {
      int seek_value = check_seek();

      if (seek_value >= 0)
      {
        if (seek_value < pos) {
          xsf_reset(frameSkip);
          pos = 0;
        }
        while (pos < seek_value)
        {
          while (buffer_rope.size()) {
            pos += buffer_rope.front().size() * 1000 / DESMUME_SAMPLE_RATE / 4;
            buffer_rope.pop_front();
          }
          NDS_exec<false>();
          SPU_Emulate_user();
        }
        buffer_rope.clear();
      }

      while (!buffer_rope.size() && !check_stop()) {
        NDS_exec<false>();
        SPU_Emulate_user();
      }
      while (buffer_rope.size() && !check_stop()) {
        auto& front = buffer_rope.front();
        if (pos > length - fade) {
          float fadeFactor = (length - pos) / (1.0 * fade);
          int sampleCount = front.size() / 2;
          int16_t* sampleBuffer = reinterpret_cast<int16_t*>(front.data());
          for (int i = 0; i < sampleCount; i++) {
            sampleBuffer[i] *= fadeFactor;
          }
        }
        write_audio(front.data(), front.size());
        pos += front.size() * 1000 / DESMUME_SAMPLE_RATE / 4;
        buffer_rope.pop_front();
      }
    }
  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    error = true;
  }

  MMU_unsetRom();
  NDS_DeInit();
	dirpath = String();
  execute = false;
	return !error;
}

bool XSFPlugin::is_our_file(const char *filename, VFSFile &file)
{
	char magic[4];
	if (file.fread (magic, 1, 4) < 4)
		return false;

	if (!memcmp(magic, "PSF$", 4))
		return 1;

	return 0;
}

const char *const XSFPlugin::exts[] = { "2sf", "mini2sf", nullptr };

static const ComboItem sampleRateItems[] = {
  ComboItem("32728 (default)", 32728),
  ComboItem("44100", 44100),
  ComboItem("48000", 48000),
  ComboItem("65456", 65456)
};

static const ComboItem interpItems[] = {
  ComboItem("None", "none"),
  ComboItem("Linear", "linear"),
  ComboItem("Cosine", "cosine"),
  ComboItem("Sharp", "sharp")
};

const PreferencesWidget XSFPlugin::widgets[] = {
	WidgetLabel(N_("<b>XSF Configuration</b>")),
	WidgetCheck(N_("Ignore length from file"), WidgetBool(CFG_ID, "ignore_length")),
  WidgetSpin(N_("Default fade time"), WidgetInt(CFG_ID, "fade"), { 0, 15000, 100, "ms" }),
  WidgetCombo(N_("Sample rate"), WidgetInt(CFG_ID, "sample_rate"), (WidgetVCombo){ sampleRateItems }),
  WidgetCombo(N_("Interpolation mode"), WidgetString(CFG_ID, "interpolation_mode"), (WidgetVCombo){ interpItems })
};

const PluginPreferences XSFPlugin::prefs = {{widgets}};
