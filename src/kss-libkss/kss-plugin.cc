/*
 * KSS Input Plugin for Audacious
 *
 * Based on libkss by digital-sound-antiques
 * Supports PSG, SCC, OPLL/FMPAC, MSX-Audio/OPL
 */

#include <string.h>
#include <stdlib.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>

extern "C" {
#include "kss/kss.h"
#include "kssplay.h"
}

static constexpr int NUM_CHANNELS = 2;
static constexpr int BUF_SAMPLES = 1024;

#define CFG_ID "kss-libkss"

static struct {
    int sample_rate;
    int default_length;   /* seconds */
    int fade_length;      /* seconds */
    int loop_count;
    int silent_limit;     /* seconds */
    bool high_quality;
    bool stereo_pan;
    int psg_a_pan;        /* -128..128, positive = left */
    int psg_b_pan;
    int psg_c_pan;
    int scc_1_pan;
    int scc_2_pan;
    int scc_3_pan;
    int scc_4_pan;
    int scc_5_pan;
    bool opll_stereo;
} kcfg;

static const char * const cfg_defaults[] = {
    "sample_rate", "44100",
    "default_length", "180",
    "fade_length", "8",
    "loop_count", "2",
    "silent_limit", "5",
    "high_quality", "TRUE",
    "stereo_pan", "TRUE",
    "psg_a_pan", "64",
    "psg_b_pan", "0",
    "psg_c_pan", "-64",
    "scc_1_pan", "96",
    "scc_2_pan", "48",
    "scc_3_pan", "0",
    "scc_4_pan", "-48",
    "scc_5_pan", "-96",
    "opll_stereo", "TRUE",
    nullptr
};

class KSSPlugin : public InputPlugin
{
public:
    static const char about[];
    static const char * const exts[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("KSS Player (libkss)"),
        PACKAGE,
        about,
        & prefs
    };

    constexpr KSSPlugin() : InputPlugin(info, InputInfo(FlagSubtunes)
        .with_exts(exts)) {}

    bool init() override;
    void cleanup() override;

    bool is_our_file(const char * filename, VFSFile & file) override;
    bool read_tag(const char * filename, VFSFile & file, Tuple & tuple,
                  Index<char> * image) override;
    bool play(const char * filename, VFSFile & file) override;
};

EXPORT KSSPlugin aud_plugin_instance;

const char KSSPlugin::about[] =
    N_("KSS Player based on libkss\n"
       "Supports PSG, SCC, OPLL/FMPAC, MSX-Audio/OPL\n\n"
       "Plugin by Ramon Smits\n"
       "https://github.com/ramonsmits/audacious-plugins\n\n"
       "libkss 1.2.1 (ec34104) by digital-sound-antiques\n"
       "https://github.com/digital-sound-antiques/libkss");

const char * const KSSPlugin::exts[] = {"kss", nullptr};

static const ComboItem sample_rate_items[] = {
    ComboItem(N_("22050 Hz"), 22050),
    ComboItem(N_("44100 Hz"), 44100),
    ComboItem(N_("48000 Hz"), 48000),
    ComboItem(N_("96000 Hz"), 96000),
};

const PreferencesWidget KSSPlugin::widgets[] = {
    WidgetLabel(N_("<b>Playback</b>")),
    WidgetCombo(N_("Sample rate:"),
        WidgetInt(kcfg.sample_rate),
        {{sample_rate_items}}),
    WidgetSpin(N_("Default song length:"),
        WidgetInt(kcfg.default_length),
        {1, 7200, 1, N_("seconds")}),
    WidgetSpin(N_("Loops before fade:"),
        WidgetInt(kcfg.loop_count),
        {1, 99, 1}),
    WidgetSpin(N_("Default fade-out:"),
        WidgetInt(kcfg.fade_length),
        {0, 60, 1, N_("seconds")}),
    WidgetSpin(N_("Silent detection:"),
        WidgetInt(kcfg.silent_limit),
        {0, 60, 1, N_("seconds")}),
    WidgetLabel(N_("<b>Rendering</b>")),
    WidgetCheck(N_("High quality"),
        WidgetBool(kcfg.high_quality)),
    WidgetLabel(N_("<b>Stereo</b>")),
    WidgetCheck(N_("Enable stereo panning"),
        WidgetBool(kcfg.stereo_pan)),
    WidgetLabel(N_("PSG channels (L +128 .. 0 .. -128 R):"),
        WIDGET_CHILD),
    WidgetSpin(N_("Channel A:"),
        WidgetInt(kcfg.psg_a_pan),
        {-128, 128, 8},
        WIDGET_CHILD),
    WidgetSpin(N_("Channel B:"),
        WidgetInt(kcfg.psg_b_pan),
        {-128, 128, 8},
        WIDGET_CHILD),
    WidgetSpin(N_("Channel C:"),
        WidgetInt(kcfg.psg_c_pan),
        {-128, 128, 8},
        WIDGET_CHILD),
    WidgetLabel(N_("SCC channels (L +128 .. 0 .. -128 R):"),
        WIDGET_CHILD),
    WidgetSpin(N_("Channel 1:"),
        WidgetInt(kcfg.scc_1_pan),
        {-128, 128, 8},
        WIDGET_CHILD),
    WidgetSpin(N_("Channel 2:"),
        WidgetInt(kcfg.scc_2_pan),
        {-128, 128, 8},
        WIDGET_CHILD),
    WidgetSpin(N_("Channel 3:"),
        WidgetInt(kcfg.scc_3_pan),
        {-128, 128, 8},
        WIDGET_CHILD),
    WidgetSpin(N_("Channel 4:"),
        WidgetInt(kcfg.scc_4_pan),
        {-128, 128, 8},
        WIDGET_CHILD),
    WidgetSpin(N_("Channel 5:"),
        WidgetInt(kcfg.scc_5_pan),
        {-128, 128, 8},
        WIDGET_CHILD),
    WidgetCheck(N_("OPLL channel stereo"),
        WidgetBool(kcfg.opll_stereo),
        WIDGET_CHILD),
};

const PluginPreferences KSSPlugin::prefs = {{widgets}};

bool KSSPlugin::init()
{
    aud_config_set_defaults(CFG_ID, cfg_defaults);

    kcfg.sample_rate    = aud_get_int(CFG_ID, "sample_rate");
    kcfg.default_length = aud_get_int(CFG_ID, "default_length");
    kcfg.fade_length    = aud_get_int(CFG_ID, "fade_length");
    kcfg.loop_count     = aud_get_int(CFG_ID, "loop_count");
    kcfg.silent_limit   = aud_get_int(CFG_ID, "silent_limit");
    kcfg.high_quality   = aud_get_bool(CFG_ID, "high_quality");
    kcfg.stereo_pan     = aud_get_bool(CFG_ID, "stereo_pan");
    kcfg.psg_a_pan      = aud_get_int(CFG_ID, "psg_a_pan");
    kcfg.psg_b_pan      = aud_get_int(CFG_ID, "psg_b_pan");
    kcfg.psg_c_pan      = aud_get_int(CFG_ID, "psg_c_pan");
    kcfg.scc_1_pan      = aud_get_int(CFG_ID, "scc_1_pan");
    kcfg.scc_2_pan      = aud_get_int(CFG_ID, "scc_2_pan");
    kcfg.scc_3_pan      = aud_get_int(CFG_ID, "scc_3_pan");
    kcfg.scc_4_pan      = aud_get_int(CFG_ID, "scc_4_pan");
    kcfg.scc_5_pan      = aud_get_int(CFG_ID, "scc_5_pan");
    kcfg.opll_stereo    = aud_get_bool(CFG_ID, "opll_stereo");

    return true;
}

void KSSPlugin::cleanup()
{
    aud_set_int(CFG_ID, "sample_rate", kcfg.sample_rate);
    aud_set_int(CFG_ID, "default_length", kcfg.default_length);
    aud_set_int(CFG_ID, "fade_length", kcfg.fade_length);
    aud_set_int(CFG_ID, "loop_count", kcfg.loop_count);
    aud_set_int(CFG_ID, "silent_limit", kcfg.silent_limit);
    aud_set_bool(CFG_ID, "high_quality", kcfg.high_quality);
    aud_set_bool(CFG_ID, "stereo_pan", kcfg.stereo_pan);
    aud_set_int(CFG_ID, "psg_a_pan", kcfg.psg_a_pan);
    aud_set_int(CFG_ID, "psg_b_pan", kcfg.psg_b_pan);
    aud_set_int(CFG_ID, "psg_c_pan", kcfg.psg_c_pan);
    aud_set_int(CFG_ID, "scc_1_pan", kcfg.scc_1_pan);
    aud_set_int(CFG_ID, "scc_2_pan", kcfg.scc_2_pan);
    aud_set_int(CFG_ID, "scc_3_pan", kcfg.scc_3_pan);
    aud_set_int(CFG_ID, "scc_4_pan", kcfg.scc_4_pan);
    aud_set_int(CFG_ID, "scc_5_pan", kcfg.scc_5_pan);
    aud_set_bool(CFG_ID, "opll_stereo", kcfg.opll_stereo);
}

struct M3uEntry {
    int song_num;
    String title;
    int duration_ms;
    int fade_ms;
};

/* Parse duration field: "mm:ss" or bare seconds */
static int parse_duration(const char * s)
{
    char * end;
    long val = strtol(s, &end, 10);
    if (val <= 0)
        return 0;

    if (*end == ':')
    {
        long sec = strtol(end + 1, nullptr, 10);
        return (int)(val * 60 + sec) * 1000;
    }

    return (int)val * 1000;
}

static Index<M3uEntry> parse_m3u(const char * m3u_uri)
{
    Index<M3uEntry> entries;

    VFSFile file(m3u_uri, "r");
    if (!file)
        return entries;

    Index<char> buf = file.read_all();
    if (buf.len() == 0)
        return entries;

    const char * p = buf.begin();
    const char * end = p + buf.len();

    while (p < end)
    {
        const char * eol = p;
        while (eol < end && *eol != '\n' && *eol != '\r')
            eol++;

        if (p < eol && *p != '#')
        {
            StringBuf line = str_copy(p, eol - p);

            /* KSS m3u: file::KSS,$XX,title,dur,,fade  (hex track)
             *      or: file::KSS,NN,title,dur,fade-   (decimal track) */
            const char * marker = strstr(line, "::KSS,");
            if (marker)
            {
                const char * num_start = marker + 6;
                int song_num;
                char * after_num;

                if (*num_start == '$')
                    song_num = (int)strtol(num_start + 1, &after_num, 16);
                else
                    song_num = (int)strtol(num_start, &after_num, 10);

                if (*after_num == ',')
                {
                    const char * title_start = after_num + 1;
                    const char * comma2 = strchr(title_start, ',');

                    String title;
                    int duration_ms = kcfg.default_length * 1000;
                    int fade_ms = kcfg.fade_length * 1000;

                    if (comma2)
                    {
                        title = String(str_copy(title_start, comma2 - title_start));

                        const char * dur_str = comma2 + 1;
                        int dur = parse_duration(dur_str);
                        if (dur > 0)
                            duration_ms = dur;

                        /* look for fade: either ,,fade or ,fade[-] */
                        const char * comma3 = strchr(dur_str, ',');
                        if (comma3)
                        {
                            const char * field = comma3 + 1;
                            if (*field == ',')
                            {
                                /* double comma: skip empty field, fade after */
                                long fade = strtol(field + 1, nullptr, 10);
                                fade_ms = (int)fade * 1000;
                            }
                            else
                            {
                                /* single comma: this field is fade (ignore trailing -) */
                                long fade = strtol(field, nullptr, 10);
                                fade_ms = (int)fade * 1000;
                            }
                        }
                    }
                    else
                    {
                        title = String(str_copy(title_start));
                    }

                    M3uEntry entry;
                    entry.song_num = song_num;
                    entry.title = std::move(title);
                    entry.duration_ms = duration_ms;
                    entry.fade_ms = fade_ms;
                    entries.append(std::move(entry));
                }
            }
        }

        p = eol;
        while (p < end && (*p == '\n' || *p == '\r'))
            p++;
    }

    return entries;
}

static Index<M3uEntry> find_m3u(const char * kss_uri)
{
    const char * sub;
    uri_parse(kss_uri, nullptr, nullptr, &sub, nullptr);
    StringBuf base = str_copy(kss_uri, sub - kss_uri);

    const char * dot = strrchr(base, '.');
    if (dot)
        base.resize(dot - base);

    /* try .m3u8 (UTF-8) first, then .m3u */
    StringBuf path = str_concat({base, ".m3u8"});
    Index<M3uEntry> entries = parse_m3u(path);

    if (entries.len() == 0)
    {
        path = str_concat({base, ".m3u"});
        entries = parse_m3u(path);
    }

    return entries;
}

static KSS * load_kss(VFSFile & file)
{
    Index<char> buf = file.read_all();
    if (buf.len() == 0)
        return nullptr;

    return KSS_bin2kss((uint8_t *)buf.begin(), buf.len(), nullptr);
}

bool KSSPlugin::is_our_file(const char * filename, VFSFile & file)
{
    char magic[4];
    if (file.fread(magic, 1, 4) != 4)
        return false;

    return !memcmp(magic, "KSCC", 4) || !memcmp(magic, "KSSX", 4);
}

bool KSSPlugin::read_tag(const char * filename, VFSFile & file,
                          Tuple & tuple, Index<char> * image)
{
    int track = 0;
    uri_parse(filename, nullptr, nullptr, nullptr, &track);
    track -= 1;

    KSS * kss = load_kss(file);
    if (!kss)
        return false;

    Index<M3uEntry> m3u = find_m3u(filename);

    tuple.set_str(Tuple::Codec, "KSS (libkss)");
    tuple.set_int(Tuple::Channels, NUM_CHANNELS);

    if (kss->title[0])
        tuple.set_str(Tuple::Album, (const char *)kss->title);

    if (track >= 0)
    {
        tuple.set_int(Tuple::Track, track + 1);
        tuple.set_int(Tuple::Subtune, track + 1);

        int duration = kcfg.default_length * 1000;
        int fade = kcfg.fade_length * 1000;
        for (auto & e : m3u)
        {
            if (e.song_num == track)
            {
                if (e.title)
                    tuple.set_str(Tuple::Title, e.title);
                duration = e.duration_ms;
                fade = e.fade_ms;
                break;
            }
        }

        tuple.set_int(Tuple::Length, duration + fade);
    }
    else
    {
        if (m3u.len() > 0)
        {
            Index<short> subtunes;
            for (auto & e : m3u)
                subtunes.append((short)(e.song_num + 1));
            tuple.set_subtunes(subtunes.len(), subtunes.begin());
        }
        else
        {
            int count = kss->trk_max - kss->trk_min + 1;
            Index<short> subtunes;
            for (int i = 0; i < count; i++)
                subtunes.append((short)(kss->trk_min + i + 1));
            tuple.set_subtunes(subtunes.len(), subtunes.begin());
        }

        if (kss->title[0])
            tuple.set_str(Tuple::Title, (const char *)kss->title);
    }

    KSS_delete(kss);
    return true;
}

bool KSSPlugin::play(const char * filename, VFSFile & file)
{
    int track = 0;
    uri_parse(filename, nullptr, nullptr, nullptr, &track);
    track -= 1;
    if (track < 0)
        track = 0;

    KSS * kss = load_kss(file);
    if (!kss)
        return false;

    int duration_ms = kcfg.default_length * 1000;
    int fade_ms = kcfg.fade_length * 1000;
    Index<M3uEntry> m3u = find_m3u(filename);
    for (auto & e : m3u)
    {
        if (e.song_num == track)
        {
            duration_ms = e.duration_ms;
            fade_ms = e.fade_ms;
            break;
        }
    }

    KSSPLAY * kssplay = KSSPLAY_new(kcfg.sample_rate, NUM_CHANNELS, 16);
    if (!kssplay)
    {
        KSS_delete(kss);
        return false;
    }

    KSSPLAY_set_data(kssplay, kss);
    KSSPLAY_reset(kssplay, track, 0);

    int quality = kcfg.high_quality ? 1 : 0;
    KSSPLAY_set_device_quality(kssplay, KSS_DEVICE_PSG, quality);
    KSSPLAY_set_device_quality(kssplay, KSS_DEVICE_SCC, quality);
    KSSPLAY_set_device_quality(kssplay, KSS_DEVICE_OPLL, quality);
    KSSPLAY_set_device_quality(kssplay, KSS_DEVICE_OPL, quality);

    if (kcfg.stereo_pan)
    {
        kssplay->psg_per_ch_pan = 1;
        kssplay->psg_ch_pan[0] = kcfg.psg_a_pan;
        kssplay->psg_ch_pan[1] = kcfg.psg_b_pan;
        kssplay->psg_ch_pan[2] = kcfg.psg_c_pan;

        kssplay->scc_per_ch_pan = 1;
        kssplay->scc_ch_pan[0] = kcfg.scc_1_pan;
        kssplay->scc_ch_pan[1] = kcfg.scc_2_pan;
        kssplay->scc_ch_pan[2] = kcfg.scc_3_pan;
        kssplay->scc_ch_pan[3] = kcfg.scc_4_pan;
        kssplay->scc_ch_pan[4] = kcfg.scc_5_pan;
    }

    if (kcfg.opll_stereo)
    {
        kssplay->opll_stereo = 1;
        KSSPLAY_set_channel_pan(kssplay, KSS_DEVICE_OPLL, 0, 1);
        KSSPLAY_set_channel_pan(kssplay, KSS_DEVICE_OPLL, 1, 2);
        KSSPLAY_set_channel_pan(kssplay, KSS_DEVICE_OPLL, 2, 1);
        KSSPLAY_set_channel_pan(kssplay, KSS_DEVICE_OPLL, 3, 2);
        KSSPLAY_set_channel_pan(kssplay, KSS_DEVICE_OPLL, 4, 1);
        KSSPLAY_set_channel_pan(kssplay, KSS_DEVICE_OPLL, 5, 2);
    }

    if (kcfg.silent_limit > 0)
        KSSPLAY_set_silent_limit(kssplay, kcfg.silent_limit * 1000);

    open_audio(FMT_S16_NE, kcfg.sample_rate, NUM_CHANNELS);

    int16_t buf[BUF_SAMPLES * NUM_CHANNELS];
    int samples_played = 0;
    bool fade_started = false;

    while (!check_stop())
    {
        int seek_value = check_seek();
        if (seek_value >= 0)
        {
            KSSPLAY_reset(kssplay, track, 0);
            int skip = (int64_t)seek_value * kcfg.sample_rate / 1000;
            KSSPLAY_calc_silent(kssplay, skip);
            samples_played = skip;
            fade_started = false;
        }

        KSSPLAY_calc(kssplay, buf, BUF_SAMPLES);
        write_audio(buf, sizeof(buf));
        samples_played += BUF_SAMPLES;

        if (KSSPLAY_get_stop_flag(kssplay))
            break;

        if (KSSPLAY_get_fade_flag(kssplay) == KSSPLAY_FADE_END)
            break;

        if (!fade_started)
        {
            int played_ms = (int64_t)samples_played * 1000 / kcfg.sample_rate;
            bool time_up = played_ms >= duration_ms;
            bool looped = KSSPLAY_get_loop_count(kssplay) >= kcfg.loop_count;

            if (time_up || looped)
            {
                if (fade_ms > 0)
                    KSSPLAY_fade_start(kssplay, fade_ms);
                else
                    break;
                fade_started = true;
            }
        }
    }

    KSSPLAY_delete(kssplay);
    KSS_delete(kss);
    return true;
}
