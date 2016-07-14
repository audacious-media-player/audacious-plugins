dnl ***
dnl *** Process this file with autoconf to produce a configure script.
dnl ***

dnl Initialize
dnl ==========
AC_PREREQ([2.59])
AC_INIT([audacious-plugins], [3.8-devel])
AC_COPYRIGHT([Copyright (C) 2001-2015 Audacious developers and others])

AC_DEFINE_UNQUOTED([PACKAGE], "$PACKAGE_NAME", [Name of package])
AC_DEFINE_UNQUOTED([VERSION], "$PACKAGE_VERSION", [Version number of package])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AUD_COMMON_PROGS

BUILDSYS_SHARED_LIB

dnl gettext
dnl =======
AM_GNU_GETTEXT([external])
if test "$MSGFMT" = ":" ; then
    AC_MSG_ERROR([msgfmt was not found; have you installed gettext?])
fi

LIBS="$LIBS $LIBINTL"

dnl Check for Audacious
dnl ===================

PKG_CHECK_MODULES(AUDACIOUS, [audacious >= 3.8],
    [],
    [AC_MSG_ERROR([Cannot find Audacious 3.8; have you installed Audacious yet?])]
)

CPPFLAGS="$CPPFLAGS $AUDACIOUS_CFLAGS"
LIBS="$LIBS $AUDACIOUS_LIBS"

dnl Check for libxml2 (required to load XSPF playlists from previous versions)
dnl ==========================================================================

if test $HAVE_DARWIN = yes ; then
    AC_ARG_WITH([system-libxml2],
     [AS_HELP_STRING([--with-system-libxml2=yes,no], [Use system version of libxml2 (default=yes)])],
     [with_system_libxml2=$withval],
     [with_system_libxml2=yes])

    if test $with_system_libxml2 = yes ; then
        XML_LIBS="-lxml2"
        XML_CFLAGS="-I/usr/include/libxml2"
        AC_SUBST(XML_LIBS)
        AC_SUBST(XML_CFLAGS)
    else
        PKG_CHECK_MODULES([XML], [libxml-2.0])
    fi
else
    PKG_CHECK_MODULES([XML], [libxml-2.0])
fi

dnl Default Set of Plugins
dnl ======================

INPUT_PLUGINS="adplug metronom psf tonegen vtx xsf"
OUTPUT_PLUGINS=""
EFFECT_PLUGINS="compressor crossfade crystalizer mixer silence-removal stereo_plugin voice_removal echo_plugin"
GENERAL_PLUGINS=""
VISUALIZATION_PLUGINS=""
CONTAINER_PLUGINS="asx asx3 audpl m3u pls xspf"
TRANSPORT_PLUGINS="gio"

if test "x$USE_GTK" = "xyes" ; then
    EFFECT_PLUGINS="$EFFECT_PLUGINS ladspa"
    GENERAL_PLUGINS="$GENERAL_PLUGINS alarm albumart lyricwiki playlist-manager search-tool statusicon"
    GENERAL_PLUGINS="$GENERAL_PLUGINS gtkui skins"
    VISUALIZATION_PLUGINS="$VISUALIZATION_PLUGINS blur_scope cairo-spectrum"
fi

if test "x$USE_QT" = "xyes" ; then
    GENERAL_PLUGINS="$GENERAL_PLUGINS albumart-qt lyricwiki-qt playlist-manager-qt search-tool-qt song-info-qt statusicon-qt"
    GENERAL_PLUGINS="$GENERAL_PLUGINS qtui skins-qt"
fi

USE_GTK_OR_QT=no
if test "x$USE_GTK" = "xyes" -o "x$USE_QT" = "xyes" ; then
    USE_GTK_OR_QT=yes
    GENERAL_PLUGINS="$GENERAL_PLUGINS delete-files skins-data"
fi

if test $HAVE_MSWINDOWS = yes ; then
    OUTPUT_PLUGINS="$OUTPUT_PLUGINS waveout"
fi

dnl Enable/disable plugin with pkg-config dependency
dnl ================================================
dnl $1 = short name of plugin (e.g. pulse)
dnl $2 = long name of plugin (e.g. PulseAudio output plugin)
dnl $3 = enabled by default (auto/yes/no)
dnl $4 = type of plugin (e.g. OUTPUT)
dnl $5 = CFLAGS/LIBS prefix (e.g. PULSE)
dnl $6 = dependency (e.g. libpulse >= 0.9.5)

AC_DEFUN([ENABLE_PLUGIN_WITH_DEP], [
    AC_ARG_ENABLE($1,
        [AS_HELP_STRING([--enable-$1], [enable $2 (default=$3)])],
        [enable_$1=$enableval],
        [enable_$1=$3])

    have_$1=no
    if test $enable_$1 != no ; then
        PKG_CHECK_MODULES($5, $6, [
            have_$1=yes
            $4_PLUGINS="$$4_PLUGINS $1"
        ], [
            if test $enable_$1 = yes ; then
                AC_MSG_ERROR([Missing dependency for $2: $6])
            else
                AC_MSG_WARN([$2 disabled due to missing dependency: $6])
            fi
        ])
    fi
])

dnl Enable/disable plugin, first calling test_<name>
dnl ================================================
dnl $1 = short name of plugin (e.g. pulse)
dnl $2 = long name of plugin (e.g. PulseAudio output plugin)
dnl $3 = enabled by default (auto/yes/no)
dnl $4 = type of plugin (e.g. OUTPUT)

AC_DEFUN([ENABLE_PLUGIN_WITH_TEST], [
    AC_ARG_ENABLE($1,
        [AS_HELP_STRING([--enable-$1], [enable $2 (default=$3)])],
        [enable_$1=$enableval],
        [enable_$1=$3])

    have_$1=no
    if test $enable_$1 != no ; then
        test_$1
        if test $have_$1 = no ; then
            if test $enable_$1 = yes ; then
                AC_MSG_ERROR([Missing dependency for $2])
            else
                AC_MSG_WARN([$2 disabled due to missing dependency])
            fi
        fi
    fi
])

dnl Optional Plugins
dnl ================

ENABLE_PLUGIN_WITH_DEP(cdaudio,
    Audio CD support,
    auto,
    INPUT,
    CDIO,
    libcdio >= 0.70 libcdio_cdda >= 0.70 libcddb >= 1.2.1)

ENABLE_PLUGIN_WITH_DEP(flac,
    FLAC support,
    yes,
    INPUT,
    LIBFLAC,
    flac >= 1.2.1)

ENABLE_PLUGIN_WITH_DEP(vorbis,
    Ogg Vorbis support,
    yes,
    INPUT,
    VORBIS,
    ogg >= 1.0 vorbis >= 1.0 vorbisenc >= 1.0 vorbisfile >= 1.0)

ENABLE_PLUGIN_WITH_DEP(amidiplug,
    MIDI synthesizer,
    auto,
    INPUT,
    FLUIDSYNTH,
    fluidsynth >= 1.0.6)

ENABLE_PLUGIN_WITH_DEP(mpg123,
    MP3 support,
    yes,
    INPUT,
    MPG123,
    libmpg123 >= 1.12)

test_aac () {
    AC_CHECK_HEADER(neaacdec.h, have_aac=yes, have_aac=no)
    AC_CHECK_LIB(faad, NeAACDecInit2, true, have_aac=no)
}

ENABLE_PLUGIN_WITH_TEST(aac,
    AAC support,
    auto,
    INPUT)

ENABLE_PLUGIN_WITH_DEP(wavpack,
    WavPack support,
    yes,
    INPUT,
    WAVPACK,
    wavpack >= 4.31)

ENABLE_PLUGIN_WITH_DEP(sndfile,
    libsndfile decoder,
    auto,
    INPUT,
    SNDFILE,
    sndfile >= 0.19)

ENABLE_PLUGIN_WITH_DEP(modplug,
    ModPlug,
    auto,
    INPUT,
    MODPLUG,
    libmodplug)

ENABLE_PLUGIN_WITH_DEP(sid,
    Commodore 64 audio,
    auto,
    INPUT,
    SIDPLAYFP,
    libsidplayfp >= 1.0)

test_console () {
    AC_CHECK_HEADERS(zlib.h, have_console=yes, have_console=no)
}

ENABLE_PLUGIN_WITH_TEST(console,
    Game Music Emu,
    auto,
    INPUT)

ENABLE_PLUGIN_WITH_DEP(bs2b,
    BS2B effect,
    auto,
    EFFECT,
    BS2B,
    libbs2b >= 3.0.0)

ENABLE_PLUGIN_WITH_DEP(resample,
    Sample rate converter,
    auto,
    EFFECT,
    SAMPLERATE,
    samplerate)

ENABLE_PLUGIN_WITH_DEP(speedpitch,
    Speed/pitch effect,
    auto,
    EFFECT,
    SAMPLERATE,
    samplerate)

ENABLE_PLUGIN_WITH_DEP(soxr,
    SoX resampler,
    auto,
    EFFECT,
    SOXR,
    soxr)

ENABLE_PLUGIN_WITH_DEP(alsa,
    ALSA output,
    auto,
    OUTPUT,
    ALSA,
    alsa >= 1.0.16)

test_jack () {
    PKG_CHECK_MODULES(JACK, jack >= 1.9.7, have_jack=yes, [
        PKG_CHECK_MODULES(JACK, jack >= 0.120.1 jack < 1.0, have_jack=yes, have_jack=no)
    ])
}

ENABLE_PLUGIN_WITH_TEST(jack,
    JACK output,
    auto,
    OUTPUT)

test_oss4 () {
    OSS_CFLAGS=
    if test -f "/etc/oss.conf"; then
        . "/etc/oss.conf"
        OSS_CFLAGS="-I$OSSLIBDIR/include"
    fi
    AC_SUBST(OSS_CFLAGS)

    OLD_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $OSS_CFLAGS"
    have_oss4=no
    AC_CHECK_HEADERS(sys/soundcard.h soundcard.h, [
        have_oss4=yes
        break
    ])
    CPPFLAGS="$OLD_CPPFLAGS"
}

ENABLE_PLUGIN_WITH_TEST(oss4,
    OSS output,
    auto,
    OUTPUT)

ENABLE_PLUGIN_WITH_DEP(pulse,
    PulseAudio output,
    auto,
    OUTPUT,
    PULSE,
    libpulse >= 0.9.5)

test_sndio () {
    AC_CHECK_LIB(sndio, sio_open, have_sndio=yes, have_sndio=no)
}

ENABLE_PLUGIN_WITH_TEST(sndio,
    Sndio output,
    auto,
    OUTPUT)

test_cue () {
    PKG_CHECK_MODULES(CUE, libcue >= 2.0, [
        AC_DEFINE(HAVE_LIBCUE2, 1, [Define if libcue version is >= 2.0])
        have_cue=yes
    ], [
        PKG_CHECK_MODULES(CUE, libcue, have_cue=yes, have_cue=no)
    ])
}

ENABLE_PLUGIN_WITH_TEST(cue,
    Cue sheet support,
    auto,
    CONTAINER)

ENABLE_PLUGIN_WITH_DEP(neon,
    HTTP/HTTPS transport,
    yes,
    TRANSPORT,
    NEON,
    neon >= 0.27)

ENABLE_PLUGIN_WITH_DEP(mms,
    MMS transport,
    auto,
    TRANSPORT,
    MMS,
    libmms >= 0.3)

ENABLE_PLUGIN_WITH_DEP(gnomeshortcuts,
    GNOME shortcuts,
    auto,
    GENERAL,
    DBUS,
    dbus-1 >= 0.60 dbus-glib-1 >= 0.60)

ENABLE_PLUGIN_WITH_DEP(notify,
    libnotify OSD,
    auto,
    GENERAL,
    NOTIFY,
    libnotify >= 0.7 gdk-pixbuf-2.0 >= 2.26)

test_lirc () {
    AC_CHECK_HEADERS(lirc/lirc_client.h, have_lirc=yes, have_lirc=no)
}

ENABLE_PLUGIN_WITH_TEST(lirc,
    LIRC support,
    auto,
    GENERAL)

test_mpris2 () {
    unset have_mpris2
    AC_CHECK_PROG(have_mpris2, gdbus-codegen, yes, no)
}

ENABLE_PLUGIN_WITH_TEST(mpris2,
    MPRIS 2 server,
    auto,
    GENERAL)

test_songchange () {
    AC_CHECK_HEADERS(sys/wait.h, have_songchange=yes, have_songchange=no)
}

ENABLE_PLUGIN_WITH_TEST(songchange,
    Song change plugin,
    auto,
    GENERAL)

dnl QtAudio
dnl =======

AC_ARG_ENABLE(qtaudio,
    [AS_HELP_STRING([--disable-qtaudio], [disable QtMultimedia output plugin (default=enabled)])],
    [enable_qtaudio=$enableval],
    [enable_qtaudio=auto])

have_qtaudio=no
if test "x$enable_qtaudio" = "xyes" -o \( "x$USE_QT" = "xyes" -a "x$enable_qtaudio" = "xauto" \); then
    PKG_CHECK_MODULES(QTMULTIMEDIA, [Qt5Multimedia], [have_qtaudio=yes], [have_qtaudio=no])
    if test "x$have_qtaudio" = "xyes"; then
        QTMULTIMEDIA_CFLAGS="$QTMULTIMEDIA_CFLAGS -fPIC"
        OUTPUT_PLUGINS="$OUTPUT_PLUGINS qtaudio"
    elif test "x$enable_qtaudio" = "xyes"; then
        AC_MSG_ERROR([QtAudio output plugin requested but QtMultimedia not found!])
    fi
fi

dnl CoreAudio
dnl =========

AC_ARG_ENABLE(coreaudio,
    [AS_HELP_STRING([--disable-coreaudio], [disable CoreAudio output plugin (default=enabled)])],
    [enable_coreaudio=$enableval],
    [enable_coreaudio=auto]
)

have_coreaudio=no
if test "x$enable_coreaudio" != "xno"; then
    if test "x$HAVE_DARWIN" != "xno"; then
        have_coreaudio=yes
        OUTPUT_PLUGINS="$OUTPUT_PLUGINS coreaudio"
    fi
else
    AC_MSG_RESULT([*** CoreAudio output plugin disabled per user request ***])
fi

dnl *** Global Hotkey general plugin (only built on X11)

AC_ARG_ENABLE(hotkey,
    [AS_HELP_STRING([--disable-hotkey], [disable global hotkey plugin (default=enabled)])],
    [enable_hotkey=$enableval],
    [enable_hotkey=auto]
)

have_hotkey=no

if test $enable_hotkey = yes -a $USE_GTK != yes ; then
    AC_MSG_ERROR([--enable-hotkey cannot be used without --enable-gtk])
fi

if test $enable_hotkey != no -a $USE_GTK = yes ; then
    PKG_CHECK_MODULES(GDKX11, [gdk-x11-2.0],
        [have_hotkey="yes"
         GENERAL_PLUGINS="$GENERAL_PLUGINS hotkey"],
        [if test "x$enable_hotkey" = "xyes"; then
            AC_MSG_ERROR([Cannot find gdk-x11-2.0 development files, but compilation of X11 Global Hotkey plugin has been explicitly requested; please install gdk-x11-2.0 dev files and run configure again])
         fi]
    )
fi

dnl *** Audacious OSD plugin (pangocairo-based)

AC_ARG_ENABLE(aosd,
    [AS_HELP_STRING([--disable-aosd], [disable Audacious OSD plugin (default=enabled)])],
    [enable_aosd=$enableval],
    [enable_aosd=auto]
)

have_aosd=no

if test $enable_aosd = yes -a $USE_GTK != yes ; then
    AC_MSG_ERROR([--enable-aosd cannot be used without --enable-gtk])
fi

if test $enable_aosd != no -a $USE_GTK = yes ; then
    PKG_CHECK_MODULES(X11EXT, [xrender xcomposite],
        [have_aosd=yes
         GENERAL_PLUGINS="$GENERAL_PLUGINS aosd"],
        [AC_MSG_RESULT([*** X Render and Composite extensions are required for Audacious OSD plugin ***])]
    )

    if test "x$enable_aosd" = "xyes" -a "x$have_aosd" != "xyes"; then
        AC_MSG_ERROR([Compilation of OSD plugin has been explicitly requested; please install required dev files and run configure again])
    fi
fi

dnl *** FFaudio

AC_ARG_WITH([ffmpeg],
 AS_HELP_STRING([--with-ffmpeg=ffmpeg,libav,none], [choose between FFmpeg, libav, or neither (default=ffmpeg)]),
 [ffmpeg_variant=$withval], [ffmpeg_variant=ffmpeg])

if test $ffmpeg_variant = ffmpeg ; then
    PKG_CHECK_MODULES([FFMPEG], [libavcodec >= 53.40.0 libavformat >= 53.21.0 libavutil >= 51.27.0],
     [INPUT_PLUGINS="$INPUT_PLUGINS ffaudio"
      AC_DEFINE([HAVE_FFMPEG], [1], [Define if using FFmpeg])],
     [AC_MSG_ERROR([FFmpeg is not installed or too old (required: libavcodec 53.40.0, libavformat 53.21.0, libavutil 51.27.0).  Use --with-ffmpeg=none to disable the ffaudio plugin or --with-ffmpeg=libav to use libav instead.])])
elif test $ffmpeg_variant = libav ; then
    PKG_CHECK_MODULES([FFMPEG], [libavcodec >= 53.25.0 libavformat >= 53.15.0 libavutil >= 51.18.0],
     [INPUT_PLUGINS="$INPUT_PLUGINS ffaudio"
      AC_DEFINE([HAVE_LIBAV], [1], [Define if using libav])],
     [AC_MSG_ERROR([libav is not installed or too old (required: libavcodec 53.25.0, libavformat 53.15.0, libavutil 51.18.0).])])
fi

dnl SDL Output
dnl ==========

AC_ARG_WITH(libsdl,
 AS_HELP_STRING([--with-libsdl=1,2], [select which SDL version to use (default=auto)]),
 [with_libsdl=$withval], [with_libsdl=auto])

test_sdlout () {
    if test "x$with_libsdl" = "x1"; then
        PKG_CHECK_MODULES(SDL, sdl >= 1.2.11, have_sdlout=yes, have_sdlout=no)
    elif test "x$with_libsdl" = "x2"; then
        PKG_CHECK_MODULES(SDL, sdl2 >= 2.0, have_sdlout=yes, have_sdlout=no)
    else
        PKG_CHECK_MODULES(SDL, sdl2 >= 2.0, have_sdlout=yes, [
            PKG_CHECK_MODULES(SDL, sdl >= 1.2.11, have_sdlout=yes, have_sdlout=no)
        ])
    fi
}

ENABLE_PLUGIN_WITH_TEST(sdlout,
    SDL Output,
    auto,
    OUTPUT)

dnl Check for at least one output plugin (not including filewriter)
dnl ===============================================================

if test "x$OUTPUT_PLUGINS" = "x" ; then
    AC_MSG_ERROR([No output plugin was enabled.  Please install the necessary packages for your system and run configure again.])
fi

dnl *** scrobbler2 ***

AC_ARG_ENABLE(scrobbler2,
 [AS_HELP_STRING([--disable-scrobbler2], [disable Scrobbler 2 plugin (default=enabled)])],
 [enable_scrobbler2=$enableval], [enable_scrobbler2=auto])

have_scrobbler2=no

if test $enable_scrobbler2 = yes -a $USE_GTK != yes ; then
    AC_MSG_ERROR([--enable-scrobbler2 cannot be used without --enable-gtk])
fi

if test $enable_scrobbler2 != no -a $USE_GTK = yes ; then
    PKG_CHECK_MODULES(CURL, [libcurl >= 7.9.7],
        [have_scrobbler2=yes
         AC_SUBST(CURL_CFLAGS)
         AC_SUBST(CURL_LIBS)]
         GENERAL_PLUGINS="$GENERAL_PLUGINS scrobbler2",
        [if test "x$enable_scrobbler2" = "xyes"; then
            AC_MSG_ERROR([Cannot find libcurl development files (ver >= 7.9.7), but compilation of Scrobbler 2 plugin has been explicitly requested; please install libcurl dev files and run configure again])
         fi]
    )
fi

dnl FileWriter
dnl ==========

AC_ARG_ENABLE(filewriter,
    [AS_HELP_STRING([--disable-filewriter], [disable FileWriter output plugin (default=enabled)])],
    [enable_filewriter=$enableval],
    [enable_filewriter=yes]
)

if test "x$enable_filewriter" != "xno"; then
    OUTPUT_PLUGINS="$OUTPUT_PLUGINS filewriter"
fi

AC_ARG_ENABLE(filewriter_mp3,
    [AS_HELP_STRING([--disable-filewriter_mp3], [disable FileWriter MP3 output part (default=enabled)])],
    [enable_filewriter_mp3=$enableval], [enable_filewriter_mp3=auto]
)

have_lame=no
if test "x$enable_filewriter" = "xyes" -a "x$enable_filewriter_mp3" != "xno"; then
    AC_CHECK_LIB([mp3lame], [lame_get_id3v2_tag],
        [have_lame=yes
         AC_DEFINE(FILEWRITER_MP3, 1, [Define if MP3 output part should be built])
         FILEWRITER_LIBS="$FILEWRITER_LIBS -lmp3lame"],
        [if test "x$enable_filewriter_mp3" = "xyes"; then
            AC_MSG_ERROR([Cannot find lame development files, but compilation of FileWriter MP3 output part has been explicitly requested; please install lame dev files and run configure again])
         fi],
        [-lm]
    )
fi

dnl Vorbis and FLAC support support reuse input plugin checks.

if test "x$enable_filewriter" = "xyes" -a "x$have_vorbis" = "xyes"; then
    AC_DEFINE(FILEWRITER_VORBIS, 1, [Define if Vorbis output part should be built])
    FILEWRITER_CFLAGS="$FILEWRITER_CFLAGS $VORBIS_CFLAGS"
    FILEWRITER_LIBS="$FILEWRITER_LIBS $VORBIS_LIBS"
fi

if test "x$enable_filewriter" = "xyes" -a "x$have_flac" != "xyes"; then
    AC_DEFINE(FILEWRITER_FLAC, 1, [Define if FLAC output part should be built])
    FILEWRITER_CFLAGS="$FILEWRITER_CFLAGS $LIBFLAC_CFLAGS"
    FILEWRITER_LIBS="$FILEWRITER_LIBS $LIBFLAC_LIBS"
fi

AC_SUBST(FILEWRITER_CFLAGS)
AC_SUBST(FILEWRITER_LIBS)

dnl OpenGL Spectrum Analyzer
dnl ========================

AC_ARG_ENABLE(glspectrum,
 [AS_HELP_STRING([--enable-glspectrum], [enable OpenGL Spectrum Analyzer (default=auto)])],
 [enable_glspectrum=$enableval], [enable_glspectrum=auto])

have_glspectrum=no

if test $enable_glspectrum = yes -a $USE_GTK != yes ; then
    AC_MSG_ERROR([--enable-glspectrum cannot be used without --enable-gtk])
fi

if test $enable_glspectrum != no -a $USE_GTK = yes ; then
    if test $HAVE_MSWINDOWS = yes ; then
        have_glspectrum=yes
        GL_LIBS="-lopengl32"
    else
        AC_CHECK_LIB(GL, glXCreateContext, [have_glspectrum=yes], [have_glspectrum=no])
        if test $have_glspectrum = yes ; then
            GL_LIBS="-lGL -lX11"
        fi
    fi

    if test $enable_glspectrum = yes && test $have_glspectrum = no ; then
        AC_MSG_ERROR([Unable to enable OpenGL Spectrum Analyzer; check config.log])
    fi

    if test $have_glspectrum = yes ; then
        VISUALIZATION_PLUGINS="$VISUALIZATION_PLUGINS gl-spectrum"
    fi
fi

AC_SUBST(GL_LIBS)

dnl Qt OpenGL Spectrum Analyzer
dnl ===========================

AC_ARG_ENABLE(qtglspectrum,
    [AS_HELP_STRING([--disable-qtglspectrum], [disable Qt OpenGL Spectrum Analyzer (default=enabled)])],
    [enable_qtglspectrum=$enableval],
    [enable_qtglspectrum=auto])

have_qtglspectrum=no

if test "x$USE_QT" = "xno" -a "x$enable_qtglspectrum" = "xyes"; then
    AC_MSG_ERROR([--enable-qtglspectrum cannot be used without --enable-qt])
fi

if test "x$USE_QT" = "xyes" -a "x$enable_qtglspectrum" != "xno"; then
    if test $have_glspectrum = yes ; then
        have_qtglspectrum=yes
    elif test $HAVE_MSWINDOWS = yes ; then
        have_qtglspectrum=yes
        GL_LIBS="-lopengl32"
    else
        AC_CHECK_LIB(GL, glBegin, [have_qtglspectrum=yes], [have_qtglspectrum=no])
        if test $have_qtglspectrum = yes ; then
            GL_LIBS="-lGL"
        fi
    fi

    if test "x$have_qtglspectrum" = "xyes"; then
        PKG_CHECK_MODULES(QTOPENGL, [Qt5OpenGL], [have_qtglspectrum=yes], [have_qtglspectrum=no])
    fi

    if test "x$have_qtglspectrum" = "xyes"; then
        QTOPENGL_CFLAGS="$QTOPENGL_CFLAGS -fPIC"
        VISUALIZATION_PLUGINS="$VISUALIZATION_PLUGINS gl-spectrum-qt"
    elif test "x$enable_qtglspectrum" = "xyes"; then
        AC_MSG_ERROR([Qt OpenGL Spectrum Analyzer could not be enabled; check config.log])
    fi
fi

dnl Mac Media Keys
dnl ============

AC_ARG_ENABLE(mac_media_keys,
 [AS_HELP_STRING([--enable-mac-media-keys], [enable Mac Media Keys])],
 [enable_mac_media_keys=$enableval], [enable_mac_media_keys="no"])

if test "x$enable_mac_media_keys" != "xno"; then
    GENERAL_PLUGINS="$GENERAL_PLUGINS mac-media-keys"
fi

dnl *** End of all plugin checks ***

plugindir=`pkg-config audacious --variable=plugin_dir`
AC_SUBST(plugindir)

dnl XXX

INPUT_PLUGIN_DIR=Input
OUTPUT_PLUGIN_DIR=Output
EFFECT_PLUGIN_DIR=Effect
GENERAL_PLUGIN_DIR=General
VISUALIZATION_PLUGIN_DIR=Visualization
CONTAINER_PLUGIN_DIR=Container
TRANSPORT_PLUGIN_DIR=Transport

AC_SUBST(INPUT_PLUGIN_DIR)
AC_SUBST(OUTPUT_PLUGIN_DIR)
AC_SUBST(EFFECT_PLUGIN_DIR)
AC_SUBST(GENERAL_PLUGIN_DIR)
AC_SUBST(VISUALIZATION_PLUGIN_DIR)
AC_SUBST(CONTAINER_PLUGIN_DIR)
AC_SUBST(TRANSPORT_PLUGIN_DIR)

localedir="$datarootdir/locale"
AC_SUBST(localedir)

AC_SUBST(EFFECT_PLUGINS)
AC_SUBST(GENERAL_PLUGINS)
AC_SUBST(INPUT_PLUGINS)
AC_SUBST(OUTPUT_PLUGINS)
AC_SUBST(VISUALIZATION_PLUGINS)
AC_SUBST(CONTAINER_PLUGINS)
AC_SUBST(TRANSPORT_PLUGINS)


dnl Reliably #include "config.h" (for large file support)
dnl =====================================================
CPPFLAGS="$CPPFLAGS -include config.h"


dnl Generate config files
dnl =====================

AC_CONFIG_FILES([
    buildsys.mk
    extra.mk
])

AC_OUTPUT


dnl Print results
dnl =============

echo
echo "Configuration:"
echo
echo "  Install path:                           $plugindir"
echo
echo "  GTK+ support:                           $USE_GTK"
echo "  Qt support:                             $USE_QT"
echo
echo "  Audio Formats"
echo "  -------------"
echo "  Audio CD:                               $have_cdaudio"
echo "  Free Lossless Audio Codec:              $have_flac"
echo "  Ogg Vorbis:                             $have_vorbis"
echo "  MIDI (via FluidSynth):                  $have_amidiplug"
echo "  MPEG-1 Layer I/II/III (via mpg123):     $have_mpg123"
echo "  MPEG-2/4 AAC:                           $have_aac"
echo "  WavPack:                                $have_wavpack"
echo
echo "  External Decoders"
echo "  -----------------"
echo "  FFmpeg/Libav:                           $ffmpeg_variant"
echo "  libsndfile:                             $have_sndfile"
echo
echo "  Chiptunes"
echo "  ---------"
echo "  AdLib synthesizer (adplug):             yes"
echo "  Commodore 64 audio (sid):               $have_sid"
echo "  Game Music Emu (spc, nsf, gbs, etc.):   $have_console"
echo "  ModPlug:                                $have_modplug"
echo "  Nintendo DS audio (xsf):                yes"
echo "  PlayStation audio (psf/psf2):           yes"
echo "  Vortex Tracker (vtx):                   yes"
echo
echo "  Other Inputs"
echo "  ------------"
echo "  Metronome:                              yes"
echo "  Tone Generator:                         yes"
echo
echo "  Effects"
echo "  -------"
echo "  Bauer stereophonic-to-binaural (bs2b):  $have_bs2b"
echo "  Channel Mixer:                          yes"
echo "  Crystalizer:                            yes"
echo "  Dynamic Range Compressor:               yes"
echo "  Echo/Surround:                          yes"
echo "  Extra Stereo:                           yes"
echo "  LADSPA Host (requires GTK+):            $USE_GTK"
echo "  Sample Rate Converter:                  $have_resample"
echo "  Silence Removal:                        yes"
echo "  SoX Resampler:                          $have_soxr"
echo "  Speed and Pitch:                        $have_speedpitch"
echo "  Voice Removal:                          yes"
echo
echo "  Outputs"
echo "  -------"
echo "  Advanced Linux Sound Architecture:      $have_alsa"
echo "  Jack Audio Connection Kit:              $have_jack"
echo "  Open Sound System:                      $have_oss4"
echo "  PulseAudio:                             $have_pulse"
echo "  Simple DirectMedia Layer:               $have_sdlout"
echo "  Sndio:                                  $have_sndio"
echo "  Win32 waveOut:                          $HAVE_MSWINDOWS"
echo "  FileWriter:                             $enable_filewriter"
echo "    -> MP3 encoding:                      $have_lame"
echo "    -> Vorbis encoding:                   $have_vorbis"
echo "    -> FLAC encoding:                     $have_flac"
echo
echo "  Playlists"
echo "  ---------"
echo "  Cue sheets:                             $have_cue"
echo "  M3U playlists:                          yes"
echo "  Microsoft ASX (legacy):                 yes"
echo "  Microsoft ASX 3.0:                      yes"
echo "  PLS playlists:                          yes"
echo "  XML Sharable Playlist Format (XSPF):    yes"
echo
echo "  Transports"
echo "  ----------"
echo "  FTP, SFTP, SMB (via GIO):               yes"
echo "  HTTP/HTTPS (via neon):                  $have_neon"
echo "  MMS (via libmms):                       $have_mms"
echo
echo "  General"
echo "  -------"
echo "  Alarm (requires GTK+):                  $USE_GTK"
echo "  Delete Files:                           $USE_GTK_OR_QT"
echo "  GNOME Shortcuts:                        $have_gnomeshortcuts"
echo "  libnotify OSD:                          $have_notify"
echo "  Linux Infrared Remote Control (LIRC):   $have_lirc"
echo "  MPRIS 2 Server:                         $have_mpris2"
echo "  Scrobbler 2.0 (requires GTK+):          $have_scrobbler2"
echo "  Song Change:                            $have_songchange"
echo

if test "x$USE_GTK" = "xyes" ; then
    echo "  GTK+ Support"
    echo "  ------------"
    echo "  GTK Interface:                          yes"
    echo "  Winamp Classic Interface:               yes"
    echo "  Album Art:                              yes"
    echo "  Blur Scope:                             yes"
    echo "  OpenGL Spectrum Analyzer:               $have_glspectrum"
    echo "  LyricWiki viewer:                       yes"
    echo "  Playlist Manager:                       yes"
    echo "  Search Tool:                            yes"
    echo "  Spectrum Analyzer (2D):                 yes"
    echo "  Status Icon:                            yes"
    echo "  X11 Global Hotkeys:                     $have_hotkey"
    echo "  X11 On-Screen Display (aosd):           $have_aosd"
    echo
fi

if test "x$USE_QT" = "xyes" ; then
    echo "  Qt Support"
    echo "  ----------"
    echo "  Qt Multimedia output:                   $have_qtaudio"
    echo "  Qt Interface:                           yes"
    echo "  Winamp Classic Interface:               yes"
    echo "  Album Art:                              yes"
    echo "  LyricWiki viewer:                       yes"
    echo "  OpenGL Spectrum Analyzer:               $have_qtglspectrum"
    echo "  Playlist Manager:                       yes"
    echo "  Search Tool:                            yes"
    echo "  Song Info:                              yes"
    echo "  Status Icon:                            yes"
    echo
fi

if test "x$HAVE_DARWIN" = "xyes" ; then
    echo "  Mac OS X Support"
    echo "  ----------------"
    echo "  CoreAudio output:                       $have_coreaudio"
    echo "  Media Keys:                             $enable_mac_media_keys"
    echo
fi
