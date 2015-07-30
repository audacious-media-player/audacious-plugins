dnl ***
dnl *** Process this file with autoconf to produce a configure script.
dnl ***

dnl Initialize
dnl ==========
AC_PREREQ([2.59])
AC_INIT([audacious-plugins], [3.7-alpha1])
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

PKG_CHECK_MODULES(AUDACIOUS, [audacious >= 3.6],
    [],
    [AC_MSG_ERROR([Cannot find Audacious 3.6; have you installed Audacious yet?])]
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
    GENERAL_PLUGINS="$GENERAL_PLUGINS alarm albumart delete-files lyricwiki playlist-manager search-tool"
    GENERAL_PLUGINS="$GENERAL_PLUGINS gtkui skins"
    VISUALIZATION_PLUGINS="$VISUALIZATION_PLUGINS blur_scope cairo-spectrum"
fi

if test "x$USE_QT" = "xyes" ; then
    GENERAL_PLUGINS="$GENERAL_PLUGINS albumart-qt lyricwiki-qt playlist-manager-qt song-info-qt"
    GENERAL_PLUGINS="$GENERAL_PLUGINS qtui skins-qt"
fi

if test "x$USE_GTK" = "xyes" -o "x$USE_QT" = "xyes" ; then
    GENERAL_PLUGINS="$GENERAL_PLUGINS skins-data"
fi

dnl Console
dnl =======

AC_ARG_ENABLE(console,
 [AS_HELP_STRING([--disable-console], [disable game music decoder (console)])],
 [enable_console=$enableval], [enable_console=auto])

have_console=no
if test "x$enable_console" != "xno"; then
    AC_CHECK_HEADERS([zlib.h],
        [have_console=yes
         INPUT_PLUGINS="$INPUT_PLUGINS console"],
        [if test "x$enable_console" = "xyes"; then
            AC_MSG_ERROR([Cannot find zlib development files, but compilation of game music decoder (console) has been explicitly requested; please install zlib dev files and run configure again])
         fi]
    )
fi

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

dnl PulseAudio
dnl ==========

AC_ARG_ENABLE( pulse,
    [AS_HELP_STRING([--disable-pulse], [disable PulseAudio output plugin (default=enabled)])],
    [enable_pulse=$enableval],
    [enable_pulse=auto]
)

have_pulse=no
if test "x$enable_pulse" != "xno"; then
    PKG_CHECK_MODULES(PULSE, [libpulse >= 0.9.5],
         [have_pulse=yes
          OUTPUT_PLUGINS="$OUTPUT_PLUGINS pulse_audio"],
         [if test "x$enable_pulse" = "xyes"; then
             AC_MSG_ERROR([Cannot find PulseAudio development files (ver >= 0.95), but compilation of PulseAudio output plugin has been explicitly requested; please install PulseAudio dev files and run configure again])
          fi]
    )
else
    AC_MSG_RESULT([*** pulseaudio output plugin disabled per user request ***])
fi

dnl MP3 (enabled by default)
dnl ========================

AC_ARG_ENABLE(mp3,
 [AS_HELP_STRING([--disable-mp3], [disable MP3 support (default=enabled)])],
 [enable_mp3=$enableval], [enable_mp3=yes])

if test $enable_mp3 = yes ; then
    PKG_CHECK_MODULES(MPG123, [libmpg123 >= 1.12])
    INPUT_PLUGINS="$INPUT_PLUGINS mpg123"
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
    PKG_CHECK_MODULES(GDKX11, [gdk-x11-3.0],
        [have_hotkey="yes"
         GENERAL_PLUGINS="$GENERAL_PLUGINS hotkey"],
        [if test "x$enable_hotkey" = "xyes"; then
            AC_MSG_ERROR([Cannot find gdk-x11-3.0 development files, but compilation of X11 Global Hotkey plugin has been explicitly requested; please install gdk-x11-3.0 dev files and run configure again])
         fi]
    )
fi

dnl *** Gnome Shortcuts Plugin

AC_ARG_ENABLE(gnomeshortcuts,
    [AS_HELP_STRING([--disable-gnomeshortcuts], [disable gnome shortcuts (default=enabled)])],
    [enable_gnomeshortcuts=$enableval],
    [enable_gnomeshortcuts=auto]
)

have_gnomeshortcuts=no
if test "x$enable_gnomeshortcuts" != "xno"; then
    PKG_CHECK_MODULES(DBUS, [dbus-1 >= 0.60 dbus-glib-1 >= 0.60],
     [have_gnomeshortcuts=yes
      GENERAL_PLUGINS="$GENERAL_PLUGINS gnomeshortcuts"],
     [if test "x$enable_gnomeshortcuts" = "xyes"; then
        AC_MSG_ERROR([Cannot find dbus or dbus-glib development files (ver >= 0.60), but compilation of Gnome Shortcuts Plugin has been explicitly requested; please install dbus and dbus-glib dev files and run configure again])
      fi]
    )
else
    AC_MSG_RESULT([*** Gnome Shortcuts Plugin disabled per user request ***])
fi

dnl Linux Infrared Remote Control (LIRC)
dnl ====================================

AC_ARG_ENABLE(lirc,
 [AS_HELP_STRING([--disable-lirc], [disable LIRC support (default=enabled)])],
 [enable_lirc=$enableval], [enable_lirc=auto])

have_lirc=no
if test "x$enable_lirc" != "xno"; then
    AC_CHECK_HEADERS([lirc/lirc_client.h],
        [have_lirc=yes
         GENERAL_PLUGINS="$GENERAL_PLUGINS lirc"],
        [if test "x$enable_lirc" = "xyes"; then
            AC_MSG_ERROR([Cannot find lirc development files, but compilation of LIRC support has been explicitly requested; please install lirc dev files and run configure again])
         fi])
fi

dnl Song Change
dnl ===========

AC_ARG_ENABLE(songchange,
 [AS_HELP_STRING([--disable-songchange], [disable song change plugin])],
 [enable_songchange=$enableval], [enable_songchange=auto])

have_songchange=no

if test $enable_songchange != no ; then
    AC_CHECK_HEADERS([sys/wait.h],
        [have_songchange=yes
         GENERAL_PLUGINS="$GENERAL_PLUGINS song_change"],
        [if test "x$enable_songchange" = "xyes"; then
            AC_MSG_ERROR([Cannot find sys/wait.h header, but compilation of song change plugin has been explicitly requested])
         fi]
    )
fi

dnl Status Icon
dnl ===========

AC_ARG_ENABLE(statusicon,
 [AS_HELP_STRING([--enable-statusicon], [enable X11 Status Icon plugin (default=disabled)])],
 [have_statusicon=$enableval], [have_statusicon="no"])

if test $have_statusicon = yes -a $USE_GTK != yes ; then
    AC_MSG_ERROR([--enable-statusicon cannot be used without --enable-gtk])
fi

if test $have_statusicon != no ; then
    GENERAL_PLUGINS="$GENERAL_PLUGINS statusicon"
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
    PKG_CHECK_MODULES(XRENDER, [xrender],
        [have_aosd=yes
         GENERAL_PLUGINS="$GENERAL_PLUGINS aosd"],
        [AC_MSG_RESULT([*** X Render extension is required for Audacious OSD plugin ***])]
    )

    if test "x$enable_aosd" = "xyes" -a "x$have_aosd" != "xyes"; then
        AC_MSG_ERROR([Compilation of OSD plugin has been explicitly requested; please install required dev files and run configure again])
    fi
fi

AC_ARG_ENABLE(aosd_xcomp,
    [AS_HELP_STRING([--disable-aosd-xcomp], [disable Audacious OSD X Composite Support (default=enabled)])],
    [enable_aosd_xcomp=$enableval],
    [enable_aosd_xcomp=auto]
)

if test "x$have_aosd" != "xyes"; then
    if test "x$enable_aosd_xcomp" = "xyes"; then
        AC_MSG_ERROR([Cannot enable X Composite support for Audacious OSD plugin which is disabled])
    else
        enable_aosd_xcomp=no
    fi
fi

have_aosd_xcomp=no
if test "x$enable_aosd_xcomp" != "xno"; then
    PKG_CHECK_MODULES(XCOMPOSITE, [xcomposite],
        [have_aosd_xcomp=yes
         AC_DEFINE([HAVE_XCOMPOSITE],[],[X Composite extension available])],
        [if test "x$enable_aosd_xcomp" = "xyes"; then
            AC_MSG_ERROR([Cannot find X Composite development files, but compilation of X Composite support for Audacious OSD plugin has been explicitly requested; please install X Composite dev files and run configure again])
         fi]
    )
else
    AC_MSG_RESULT([*** X Composite support for Audacious OSD plugin disabled per user request ***])
fi

dnl notify: libnotify-based OSD
dnl ===========================

AC_ARG_ENABLE(notify,
 [AS_HELP_STRING([--disable-notify], [disable notify plugin (default=enabled)])],
 [enable_notify=$enableval], [enable_notify=auto])

have_notify=no

if test $enable_notify = yes -a $USE_GTK != yes ; then
    AC_MSG_ERROR([--enable-notify cannot be used without --enable-gtk])
fi

if test $enable_notify != no -a $USE_GTK = yes ; then
    PKG_CHECK_MODULES(NOTIFY, [libnotify >= 0.7],
        [have_notify=yes
         GENERAL_PLUGINS="$GENERAL_PLUGINS notify"],
        [if test "x$enable_notify" = "xyes"; then
            AC_MSG_ERROR([Cannot find libnotify development files (ver >= 0.7), but compilation of notify plugin has been explicitly requested; please install libnotify dev files and run configure again])
         fi]
    )
fi

dnl MPRIS 2 (requires GDBus)
dnl ========================

AC_ARG_ENABLE(mpris2,
 AS_HELP_STRING(--disable-mpris2, [disable MPRIS 2 support (default=auto)]),
 enable_mpris2=$enableval, enable_mpris2=auto)

if test $enable_mpris2 = no ; then
	have_mpris2=no
else
	AC_CHECK_PROG(have_mpris2, gdbus-codegen, yes, no)
	if test $have_mpris2 = yes ; then
		GENERAL_PLUGINS="$GENERAL_PLUGINS mpris2"
	elif test $enable_mpris2 = yes ; then
		AC_MSG_ERROR([MPRIS 2 plugin could not be enabled; check config.log])
	fi
fi

dnl Ogg Vorbis (enabled by default)
dnl ===============================

dnl This test is reused later to enable/disable Vorbis support in filewriter.

AC_ARG_ENABLE(vorbis,
 [AS_HELP_STRING([--disable-vorbis], [disable Ogg Vorbis support (default=enabled)])],
 [enable_vorbis=$enableval], [enable_vorbis=yes])

if test $enable_vorbis = yes ; then
    PKG_CHECK_MODULES(VORBIS, [ogg >= 1.0 vorbis >= 1.0 vorbisenc >= 1.0 vorbisfile >= 1.0])
    INPUT_PLUGINS="$INPUT_PLUGINS vorbis"
fi

dnl FLAC (enabled by default)
dnl =========================

AC_ARG_ENABLE(flacng,
 [AS_HELP_STRING([--disable-flacng], [disable FLAC support (default=enabled)])],
 [enable_flacng=$enableval], [enable_flacng=yes])

if test $enable_flacng = yes ; then
    PKG_CHECK_MODULES(LIBFLAC, [flac >= 1.2.1])
    INPUT_PLUGINS="$INPUT_PLUGINS flacng"
fi

dnl *** WavPack 4.31 support

AC_ARG_ENABLE(wavpack,
    [AS_HELP_STRING([--disable-wavpack], [disable WavPack input plugin (default=enabled)])],
    [enable_wavpack=$enableval],
    [enable_wavpack=auto]
)

have_wavpack=no
if test "x$enable_wavpack" != "xno"; then
    PKG_CHECK_MODULES(WAVPACK, [wavpack >= 4.31],
        [have_wavpack=yes
         INPUT_PLUGINS="$INPUT_PLUGINS wavpack"],
        [if test "x$enable_wavpack" = "xyes"; then
            AC_MSG_ERROR([Cannot find WavPack development files (ver >= 4.31), but compilation of WavPack plugin has been explicitly requested; please install WavPack dev files and run configure again])
         fi]
    )
else
    AC_MSG_RESULT([*** WavPack plugin disabled per user request ***])
fi

dnl AAC (enabled by default)
dnl ========================

AC_ARG_ENABLE(aac,
 [AS_HELP_STRING([--disable-aac], [disable AAC support (default=enabled)])],
 [enable_aac=$enableval], [enable_aac=yes])

if test $enable_aac = yes ; then
    AC_CHECK_HEADER([neaacdec.h], [have_aac=yes], [have_aac=no])
    AC_CHECK_LIB([faad], [NeAACDecInit2], [have_aac=$have_aac], [have_aac=no])

    if test $have_aac != yes ; then
        AC_MSG_ERROR([Please install libfaad, or use --disable-aac to disable AAC support.])
    fi

    FAAD_CFLAGS=
    FAAD_LIBS="-lfaad"
    AC_SUBST(FAAD_CFLAGS)
    AC_SUBST(FAAD_LIBS)

    INPUT_PLUGINS="$INPUT_PLUGINS aac-raw"
fi

dnl *** sndfile

AC_ARG_ENABLE(sndfile,
    [AS_HELP_STRING([--disable-sndfile], [disable sndfile extensions. [default=enabled]])],
    [enable_sndfile=$enableval],
    [enable_sndfile=auto]
)

have_sndfile=no
if test "x$enable_sndfile" != "xno"; then
    PKG_CHECK_MODULES(SNDFILE, [sndfile >= 0.19],
        [have_sndfile=yes
         INPUT_PLUGINS="$INPUT_PLUGINS sndfile"],
        [if test "x$enable_sndfile" = "xyes"; then
            AC_MSG_ERROR([Cannot find libsndfile development files (ver >= 0.19), but compilation of libsndfile extensions has been explicitly requested; please install libsndfile dev files and run configure again])
         fi]
    )
else
    AC_MSG_RESULT([*** libsndfile extensions disabled per user request ***])
fi

dnl *** modplug

AC_ARG_ENABLE(modplug,
    [AS_HELP_STRING([--disable-modplug], [disable ModPlug plugin (default=enabled)])],
    [enable_modplug=$enableval],
    [enable_modplug=auto]
)

have_modplug=no
if test "x$enable_modplug" != "xno"; then
    PKG_CHECK_MODULES(MODPLUG, [libmodplug],
     [INPUT_PLUGINS="$INPUT_PLUGINS modplug"
      have_modplug=yes],
     [if test "x$enable_modplug" = "xyes"; then
         AC_MSG_ERROR([Cannot find libmodplug development files, but compilation of ModPlug plugin has been explicitly requested; please install libmodplug dev files and run configure again])
      fi]
    )
fi

dnl *** FFaudio

AC_ARG_WITH([ffmpeg],
 AS_HELP_STRING([--with-ffmpeg=ffmpeg,libav,none], [choose between FFmpeg, libav, or neither (default=ffmpeg)]),
 [ffmpeg_variant=$withval],
 [ffmpeg_variant=ffmpeg])

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

dnl *** jack output plugin

AC_ARG_ENABLE(jack,
 [AS_HELP_STRING([--disable-jack], [disable JACK output plugin (default=enabled)])],
 [enable_jack=$enableval], [enable_jack=auto])

have_jack=no
if test "x$enable_jack" != "xno"; then
    PKG_CHECK_MODULES(JACK, [jack >= 1.9.7],
        [have_jack=yes],
        [PKG_CHECK_MODULES(JACK, [jack >= 0.120.1 jack < 1.0],
            [have_jack=yes],
            [have_jack=no])]
    )

    if test "x$have_jack" = "xyes"; then
        OUTPUT_PLUGINS="$OUTPUT_PLUGINS jack-ng"
    elif test "x$enable_jack" = "xyes"; then
        AC_MSG_ERROR([Cannot find JACK (ver >= 0.120.1 but < 1.0 or >= 1.9.7) development files, but compilation of JACK output plugin has been explicitly requested; please install JACK dev files and run configure again])
    fi
fi

dnl *** sid
AC_ARG_ENABLE(sid,
[AS_HELP_STRING([--disable-sid], [disable SID input plugin (default=enabled)])],
                [enable_sid=$enableval], [enable_sid=auto])

have_sidplay=no
if test "x$enable_sid" != "xno"; then
    PKG_CHECK_MODULES([SIDPLAYFP], [libsidplayfp >= 1.0],
        [have_sidplay=yes
         INPUT_PLUGINS="$INPUT_PLUGINS sid"],
        [if test "x$enable_sid" = "xyes"; then
            AC_MSG_ERROR([Cannot find sidplayfp, but compilation of SID input plugin has been explicitly requested; please install sidplayfp and run configure again])
         fi]
    )
else
    AC_MSG_RESULT([*** SID plugin disabled per user request ***])
fi

dnl OSS output
dnl ==========

AC_ARG_ENABLE(oss4,
 [AS_HELP_STRING([--disable-oss4], [disable OSS4 output plugin])],
 [enable_oss4=$enableval], [enable_oss4=auto])

OSS_CFLAGS=
have_oss4=no
if test "x$enable_oss4" != "xno" ; then
    if test -f "/etc/oss.conf"; then
        . "/etc/oss.conf"
        OSS_CFLAGS="-I$OSSLIBDIR/include"
    fi

    OLD_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $OSS_CFLAGS"
    AC_CHECK_HEADERS([sys/soundcard.h soundcard.h], [have_oss4=yes])
    CPPFLAGS="$OLD_CPPFLAGS"

    if test "x$have_oss4" = "xyes" ; then
        OUTPUT_PLUGINS="$OUTPUT_PLUGINS oss4"
        AC_SUBST(OSS_CFLAGS)
    elif test "x$enable_oss4" = "xyes" ; then
        AC_MSG_ERROR([OSS4 output was enabled but soundcard.h was not found])
    fi

    AC_MSG_RESULT([$have_oss4])
fi

dnl *** ALSA output plugin

AC_ARG_ENABLE(alsa,
    [AS_HELP_STRING([--disable-alsa], [disable ALSA output plugin])],
    [enable_alsa=$enableval],
    [enable_alsa=auto]
)

have_alsa=no
if test "x$enable_alsa" != "xno"; then
    PKG_CHECK_MODULES([ALSA], [alsa >= 1.0.16],
        [have_alsa=yes
         OUTPUT_PLUGINS="$OUTPUT_PLUGINS alsa"],
        [if test "x$enable_alsa" = "xyes"; then
            AC_MSG_ERROR([Cannot find ALSA development files (ver >= 1.0.16), but compilation of ALSA output plugin has been explicitly requested; please install ALSA dev files and run configure again])
         fi])
else
    AC_MSG_RESULT([*** ALSA output plugin disabled per user request ***])
fi

dnl SDL Output
dnl ==========

AC_ARG_ENABLE(sdlout,
 [AS_HELP_STRING([--disable-sdlout], [disable SDL output plugin])],
 [enable_sdlout=$enableval], [enable_sdlout=auto])

if test "x$enable_sdlout" != "xno"; then
    AC_ARG_WITH(libsdl,
                [AS_HELP_STRING([--with-libsdl=VER], [select which SDL version to use. Set VER to 1 for libsdl1, to 2 for libsdl2. @<:@default=check@:>@])],
                [case "x$withval" in
                     x1) ;;
                     x2) ;;
                     x*) withval=check;;
                 esac
                 with_libsdl=$withval], [with_libsdl=check])
fi

libsdl1_min="1.2.11";
libsdl2_min="2.0";

have_sdlout=no
if test "x$enable_sdlout" != "xno"; then
    if test "x$with_libsdl" = "xcheck"; then
        PKG_CHECK_MODULES([SDL], [sdl2 >= $libsdl2_min],
           [have_sdlout=yes
            OUTPUT_PLUGINS="$OUTPUT_PLUGINS sdlout"],
           [PKG_CHECK_MODULES([SDL], [sdl >= $libsdl1_min],
               [have_sdlout=yes
                OUTPUT_PLUGINS="$OUTPUT_PLUGINS sdlout"],
               [if test "x$enable_sdlout" = "xyes"; then
                   AC_MSG_ERROR([Cannot find SDL development files (ver >= $libsdl1_min), but compilation of SDL output plugin has been explicitly requested; please install SDL dev files and run configure again])
                fi])])
    elif test "x$with_libsdl" = "x1"; then
        PKG_CHECK_MODULES([SDL], [sdl >= $libsdl1_min],
            [have_sdlout=yes
             OUTPUT_PLUGINS="$OUTPUT_PLUGINS sdlout"],
            [if test "x$enable_sdlout" = "xyes"; then
                 AC_MSG_ERROR([Cannot find SDL1 development files (ver >= $libsdl1_min), but compilation of SDL output plugin has been explicitly requested; please install SDL1 dev files and run configure again])
             fi])
    elif test "x$with_libsdl" = "x2"; then
        PKG_CHECK_MODULES([SDL], [sdl2 >= $libsdl2_min],
            [have_sdlout=yes
             OUTPUT_PLUGINS="$OUTPUT_PLUGINS sdlout"],
            [if test "x$enable_sdlout" = "xyes"; then
                 AC_MSG_ERROR([Cannot find SDL2 development files (ver >= $libsdl2_min), but compilation of SDL output plugin has been explicitly requested; please install SDL2 dev files and run configure again])
             fi])
    fi
fi

dnl *** sndio output

AC_ARG_ENABLE(sndio,
    [AS_HELP_STRING([--disable-sndio], [disable sndio output plugin (default=enabled)])],
    [enable_sndio=$enableval],
    [enable_sndio=auto]
)

have_sndio=no
if test "x$enable_sndio" != "xno"; then
    AC_CHECK_HEADER([sndio.h],
        [AC_CHECK_LIB([sndio], [sio_open],
            [have_sndio=yes
             OUTPUT_PLUGINS="$OUTPUT_PLUGINS sndio-ng"
             SNDIO_LIBS="-lsndio"
             AC_SUBST(SNDIO_LIBS)]
         )]
    )

    if test "x$enable_sndio" = "xyes" -a "x$have_sndio" != "xyes"; then
        AC_MSG_ERROR([Cannot find sndio development files, but compilation of sndio output plugin has been explicitly requested; please install sndio dev files and run configure again])
    fi
fi

dnl Check for at least one output plugin (not including filewriter)
dnl ===============================================================

if test "x$OUTPUT_PLUGINS" = "x" ; then
    AC_MSG_ERROR([No output plugin was enabled.  Please install the necessary packages for your system and run configure again.])
fi

dnl amidi-plug
dnl ==========

AC_ARG_ENABLE(amidiplug,
 [AS_HELP_STRING([--disable-amidiplug], [disable amidi-plug input plugin])],
 [enable_amidiplug=$enableval], [enable_amidiplug=auto])

have_amidiplug=no
if test "x$enable_amidiplug" != "xno"; then
    PKG_CHECK_MODULES(FLUIDSYNTH, [fluidsynth >= 1.0.6],
        [have_amidiplug=yes
         INPUT_PLUGINS="$INPUT_PLUGINS amidi-plug"],
        [if test "x$enable_amidiplug" = "xyes"; then
            AC_MSG_ERROR([Cannot find FluidSynth development files (ver >= 1.0.6), but compilation of amidi-plug input plugin has been explicitly requested; please install FluidSynth dev files and run configure again])
         fi])
fi

dnl Audio CD
dnl ========

AC_ARG_ENABLE(cdaudio,
    [AS_HELP_STRING([--disable-cdaudio], [disable cdaudio-ng input plugin (default=enabled)])],
    [enable_cdaudio_ng=$enableval],
    [enable_cdaudio_ng=auto]
)

have_cdaudio_ng=no
if test "x$enable_cdaudio_ng" != "xno"; then
    PKG_CHECK_MODULES(CDIO, [libcdio >= 0.70 libcdio_cdda >= 0.70],
        [PKG_CHECK_MODULES(CDDB, [libcddb >= 1.2.1],
            [have_cdaudio_ng=yes
             INPUT_PLUGINS="$INPUT_PLUGINS cdaudio-ng"
             GENERAL_PLUGINS="$GENERAL_PLUGINS cd-menu-items"
             AC_SUBST(CDIO_LIBS)
             AC_SUBST(CDIO_CFLAGS)
             AC_SUBST(CDDB_LIBS)
             AC_SUBST(CDDB_CFLAGS)],
            [AC_MSG_WARN([*** Cannot find libcddb 1.2.1 or newer, cdaudio-ng will not be built ***])]
         )],
        [AC_MSG_WARN([*** Cannot find libcdio 0.70 or newer, cdaudio-ng will not be built ***])]
    )

    if test "x$enable_cdaudio_ng" = "xyes" -a "x$have_cdaudio_ng" != "xyes"; then
        AC_MSG_ERROR([Compilation of cdaudio-ng input plugin has been explicitly requested; please install required dev files and run configure again])
    fi
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

dnl neon HTTP support (enabled by default)
dnl ======================================

AC_ARG_ENABLE(neon,
 [AS_HELP_STRING([--disable-neon], [disable neon HTTP support (default=enabled)])],
 [enable_neon=$enableval], [enable_neon=yes])

if test $enable_neon = yes ; then
    PKG_CHECK_MODULES(NEON, [neon >= 0.27])
    TRANSPORT_PLUGINS="$TRANSPORT_PLUGINS neon"
fi

dnl MMS
dnl ===

AC_ARG_ENABLE(mms,
[AS_HELP_STRING([--disable-mms], [disable mms support. (default=enabled)])],
[enable_mms=$enableval],
[enable_mms=auto])

have_mms=no
if test "x$enable_mms" != "xno"; then
    PKG_CHECK_MODULES(MMS, [libmms >= 0.3],
        [have_mms=yes
         TRANSPORT_PLUGINS="$TRANSPORT_PLUGINS mms"],
        [if test "x$enable_mms" = "xyes"; then
            AC_MSG_ERROR([Cannot find libmms development files (ver >= 0.3), but compilation of mms support has been explicitly requested; please install libmms dev files and run configure again])
         fi]
    )
else
    AC_MSG_RESULT([*** mms plugin disabled by request ***])
fi

dnl CUE
dnl ---

AC_ARG_ENABLE(cue,
[AS_HELP_STRING([--disable-cue], [disable cue support. (default=enabled)])],
[enable_cue=$enableval],
[enable_cue=auto])

have_cue=no
if test "x$enable_cue" != "xno"; then
    PKG_CHECK_MODULES(CUE, [libcue],
        [have_cue=yes
         CONTAINER_PLUGINS="$CONTAINER_PLUGINS cue"],
        [if test "x$enable_cue" = "xyes"; then
            AC_MSG_ERROR([Cannot find libcue development files, but compilation of cue support has been explicitly requested; please install libcue dev files and run configure again])
         fi]
    )
else
    AC_MSG_RESULT([*** cue plugin disabled by request ***])
fi

dnl FileWriter
dnl ==========

AC_ARG_ENABLE(filewriter,
    [AS_HELP_STRING([--disable-filewriter], [disable FileWriter output plugin (default=enabled)])],
    [enable_filewriter=$enableval],
    [enable_filewriter=$USE_GTK]
)

have_filewriter=no
if test "x$enable_filewriter" != "xno"; then
    if test "x$USE_GTK" != "xyes"; then
        AC_MSG_ERROR([--enable-filewriter cannot be used without --enable-gtk])
    fi
    have_filewriter=yes
    OUTPUT_PLUGINS="$OUTPUT_PLUGINS filewriter"
fi


AC_ARG_ENABLE(filewriter_mp3,
    [AS_HELP_STRING([--disable-filewriter_mp3], [disable FileWriter MP3 output part (default=enabled)])],
    [enable_filewriter_mp3=$enableval], [enable_filewriter_mp3=auto]
)

have_lame=no
if test "x$have_filewriter" = "xyes" -a "x$enable_filewriter_mp3" != "xno"; then
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

dnl Vorbis support reuses test done for Vorbis input plugin.

have_vorbisenc=no
if test "x$have_filewriter" = "xyes" -a "x$enable_vorbis" = "xyes"; then
    have_vorbisenc=yes
    AC_DEFINE(FILEWRITER_VORBIS, 1, [Define if Vorbis output part should be built])
    FILEWRITER_CFLAGS="$FILEWRITER_CFLAGS $VORBIS_CFLAGS"
    FILEWRITER_LIBS="$FILEWRITER_LIBS $VORBIS_LIBS"
fi

AC_ARG_ENABLE(filewriter_flac,
    [AS_HELP_STRING([--disable-filewriter_flac], [disable FileWriter FLAC output part (default=enabled)])],
    [enable_filewriter_flac=$enableval], [enable_filewriter_flac=auto]
)

have_writer_flac=no
if test "x$have_filewriter" = "xyes" -a "x$enable_filewriter_flac" != "xno"; then
    PKG_CHECK_MODULES(FLAC, [flac >= 1.1.3],
        [have_writer_flac=yes
         AC_DEFINE(FILEWRITER_FLAC, 1, [Define if FLAC output part should be built])
         FILEWRITER_CFLAGS="$FILEWRITER_CFLAGS $FLAC_CFLAGS"
         FILEWRITER_LIBS="$FILEWRITER_LIBS $FLAC_LIBS"],
        [if test "x$enable_filewriter_flac" = "xyes"; then
            AC_MSG_ERROR([Cannot find libflac development files (ver >= 1.1.3), but compilation of FileWriter FLAC output part has been explicitly requested; please install libflac dev files and run configure again])
         fi]
   )
fi

AC_SUBST(FILEWRITER_CFLAGS)
AC_SUBST(FILEWRITER_LIBS)

dnl *** BS2B effect plugin

AC_ARG_ENABLE(bs2b,
    [AS_HELP_STRING([--disable-bs2b], [disable BS2B effect plugin (default=enabled)])],
    [enable_bs2b=$enableval],
    [enable_bs2b=auto]
)

have_bs2b=no
if test "x$enable_bs2b" != "xno"; then
    PKG_CHECK_MODULES([BS2B], [libbs2b >= 3.0.0],
        [have_bs2b=yes
         EFFECT_PLUGINS="$EFFECT_PLUGINS bs2b"],
        [if test "x$enable_bs2b" = "xyes"; then
            AC_MSG_ERROR([Cannot find libbs2b development files (ver >= 3.0.0), but compilation of BS2B effect plugin has been explicitly requested; please install libbs2b dev files and run configure again])
         fi]
    )
else
    AC_MSG_RESULT([*** BS2B effect plugin disabled per user request ***])
fi

dnl *** Resample effect plugin

AC_ARG_ENABLE(resample,
 [AS_HELP_STRING([--disable-resample], [disable resample effect plugin (default=enabled)])],
 [enable_resample=$enableval], [enable_resample=auto]
)

have_resample=no
if test "x$enable_resample" != "xno"; then
    PKG_CHECK_MODULES([SAMPLERATE], [samplerate],
        [have_resample=yes
         EFFECT_PLUGINS="$EFFECT_PLUGINS resample"],
        [if test "x$enable_resample" = "xyes"; then
            AC_MSG_ERROR([Cannot find libsamplerate development files, but compilation of resample effect plugin has been explicitly requested; please install libsamplerate dev files and run configure again])
         fi]
    )
fi

dnl Speed and Pitch effect plugin
dnl =============================

AC_ARG_ENABLE(speedpitch,
 [AS_HELP_STRING([--disable-speedpitch],[disable Speed and Pitch effect plugin])],
 [enable_speedpitch="$enableval"], [enable_speedpitch=auto])

have_speedpitch=no
if test "x$enable_speedpitch" != "xno"; then
    PKG_CHECK_MODULES([SAMPLERATE], [samplerate],
        [have_speedpitch=yes
         EFFECT_PLUGINS="$EFFECT_PLUGINS speed-pitch"],
        [if test "x$enable_speedpitch" = "xyes"; then
            AC_MSG_ERROR([Cannot find libsamplerate development files, but compilation of Speed and Pitch effect plugin has been explicitly requested; please install libsamplerate dev files and run configure again])
         fi]
    )
fi

dnl *** SoX Resampler effect plugin

AC_ARG_ENABLE(soxr,
 [AS_HELP_STRING([--disable-soxr], [disable SoX Resampler effect plugin (default=enabled)])],
 [enable_soxr=$enableval], [enable_soxr=auto]
)

have_soxr=no
if test "x$enable_soxr" != "xno"; then
    PKG_CHECK_MODULES([SOXR], [soxr],
        [have_soxr=yes
         EFFECT_PLUGINS="$EFFECT_PLUGINS sox-resampler"],
        [if test "x$enable_soxr" = "xyes"; then
            AC_MSG_ERROR([Cannot find soxr development files, but compilation of SoX Resampler effect plugin has been explicitly requested; please install soxr dev files and run configure again])
         fi]
    )
fi

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
echo "  Output Plugins"
echo "  --------------"
echo "  Open Sound System (oss4):               $have_oss4"
echo "  Advanced Linux Sound Arch. (alsa):      $have_alsa"
echo "  Sndio (sndio):                          $have_sndio"
echo "  CoreAudio (coreaudio):                  $have_coreaudio"
echo "  PulseAudio (pulse):                     $have_pulse"
echo "  Jack Audio Connection Kit (jack):       $have_jack"
echo "  Simple DirectMedia Layer (sdlout):      $have_sdlout"
echo "  Qt Multimedia (qtaudio):                $have_qtaudio"
echo "  FileWriter:                             $have_filewriter"
echo "    -> FileWriter MP3 output part:        $have_lame"
echo "    -> FileWriter Vorbis output part:     $have_vorbisenc"
echo "    -> FileWriter FLAC output part:       $have_writer_flac"
echo
echo "  Input Plugins"
echo "  -------------"
echo "  MPEG-1 Layer I/II/III (mpg123):         $enable_mp3"
echo "  MPEG-2/4 AAC (aac):                     $enable_aac"
echo "  FFaudio (ffaudio):                      $ffmpeg_variant"
echo "  Module decoder (modplug):               $have_modplug"
echo "  MIDI synthesizer (amidi-plug):          $have_amidiplug"
echo "  CD Digital Audio (cdaudio_ng):          $have_cdaudio_ng"
echo "  sndfile extensions:                     $have_sndfile"
echo "  Tone Generator:                         yes"
echo "  Ogg Vorbis (vorbis):                    $enable_vorbis"
echo "  Free Lossless Audio Codec (flacng):     $enable_flacng"
echo "  Commodore 64 audio (SID):               $have_sidplay"
echo "  Game music (spc, nsf & gbs):            $have_console"
echo "  PlayStation (psf/psf2) audio:           yes"
echo "  Nintendo DS audio (xsf):                yes"
echo "  AdLib synthesizer (adplug):             yes"
echo "  WavPack 4.31+ (wavpack):                $have_wavpack"
echo "  Metronom:                               yes"
echo "  VTX Decoder:                            yes"
echo
echo "  General"
echo "  -------"
echo "  Alarm:                                  $USE_GTK"
echo "  Album Art:                              $USE_GTK"
echo "  Album Art (Qt):                         $USE_QT"
echo "  Delete from Filesystem:                 $USE_GTK"
echo "  Linux Infrared Remote Control (LIRC)    $have_lirc"
echo "  MPRIS 2 Server:                         $have_mpris2"
echo "  Playlist Manager:                       $USE_GTK"
echo "  Playlist Manager (Qt):                  $USE_QT"
echo "  Search Tool:                            $USE_GTK"
echo "  Song Change:                            $have_songchange"
echo "  Song Info (Qt):                         $USE_QT"
echo "  Status Icon:                            $have_statusicon"
echo "  Audacious OSD:                          $have_aosd"
echo "    -> X Composite support:               $have_aosd_xcomp"
echo "  libnotify OSD:                          $have_notify"
echo "  Global Hotkey Plugin:                   $have_hotkey"
echo "  Gnome Shortcuts Plugin:                 $have_gnomeshortcuts"
echo "  Scrobbler 2.0:                          $have_scrobbler2"
echo "  LyricWiki viewer:                       $USE_GTK"
echo "  LyricWiki viewer (Qt):                  $USE_QT"
echo "  Mac Media Keys:                         $enable_mac_media_keys"
echo
echo "  Effect"
echo "  ------"
echo "  Channel Mixer:                          yes"
echo "  Crystalizer:                            yes"
echo "  Dynamic Range Compressor:               yes"
echo "  Echo/Surround:                          yes"
echo "  Extra Stereo:                           yes"
echo "  LADSPA Host:                            $USE_GTK"
echo "  Voice Removal:                          yes"
echo "  Bauer stereophonic-to-binaural (bs2b):  $have_bs2b"
echo "  Sample Rate Converter (resample):       $have_resample"
echo "  Silence Removal:                        yes"
echo "  Speed and Pitch:                        $have_speedpitch"
echo "  SoX Resampler:                          $have_soxr"
echo
echo "  Visualization"
echo "  -------------"
echo "  Blur Scope:                             $USE_GTK"
echo "  Cairo Spectrum Analyzer:                $USE_GTK"
echo "  GTK+ OpenGL Spectrum Analyzer:          $have_glspectrum"
echo "  Qt OpenGL Spectrum Analyzer:            $have_qtglspectrum"
echo
echo "  Transport"
echo "  ---------"
echo "  neon-based http/https:                  $enable_neon"
echo "  libmms-based mms:                       $have_mms"
echo "  GIO:                                    yes"
echo
echo "  Container"
echo "  ---------"
echo "  Audacious playlist format (audpl):      yes"
echo "  Winamp PLS playlist format (pls):       yes"
echo "  M3U playlist format (m3u):              yes"
echo "  Microsoft ASX (legacy):                 yes"
echo "  Microsoft ASX 3.0:                      yes"
echo "  XML Sharable Playlist Format (xspf):    yes"
echo "  CUE playlist format (cue):              $have_cue"
echo
echo "  Interfaces"
echo "  ----------"
echo "  GTK (gtkui):                            $USE_GTK"
echo "  Qt (qtui):                              $USE_QT"
echo "  Winamp Classic (skins):                 $USE_GTK"
echo "  Winamp Classic (skins, Qt):             $USE_QT"
echo
