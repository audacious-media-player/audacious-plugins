dnl Process this file with autoconf to produce a configure script.

AC_INIT([audacious-plugins], [1.3.0], [bugs+audacious-plugins@atheme.org])
AC_PREREQ([2.59])

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AC_CONFIG_HEADERS([config.h])

dnl libbeep
LIBBEEP_MAJOR_VERSION=2
LIBBEEP_MINOR_VERSION=0
LIBBEEP_MICRO_VERSION=0
LIBBEEP_VERSION=$LIBBEEP_MAJOR_VERSION.$LIBBEEP_MINOR_VERSION.$LIBBEEP_MICRO_VERSION
AC_SUBST(LIBBEEP_MAJOR_VERSION)
AC_SUBST(LIBBEEP_MINOR_VERSION)
AC_SUBST(LIBBEEP_MICRO_VERSION)

dnl XXX kludge for FreeBSD
if test -d "/usr/X11R6/include"; then
	CPPFLAGS="$CPPFLAGS -I/usr/X11R6/include"
fi

dnl OMK bootstrap
AC_SUBST([PACKAGE], [AC_PACKAGE_TARNAME])
AC_SUBST([VERSION], [AC_PACKAGE_VERSION])

AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE", [Name of package])
AC_DEFINE_UNQUOTED(VERSION, "$VERSION", [Version number of package])

dnl Check for C compiler

AC_PROG_CC
AC_PROG_CXX
AC_ISC_POSIX
AC_C_BIGENDIAN

dnl Check for assembler

AM_PROG_AS

dnl Checks for various programs

AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

AC_PATH_PROG(RM, rm)
AC_PATH_PROG(MV, mv)
AC_PATH_PROG(CP, cp)
AC_PATH_PROG(AR, ar)
AC_PATH_PROG(RANLIB, ranlib)

dnl Gettext -nenolod
AM_GNU_GETTEXT([external])

dnl Path settings

AC_ARG_WITH(dev-dsp,
    [  --with-dev-dsp=path            Path to OSS DSP data pipe, default is /dev/dsp.],
    [dev_dsp=$withval],
    [dev_dsp=/dev/dsp]
)
AC_DEFINE_UNQUOTED(DEV_DSP, "$dev_dsp",
    [Path to OSS DSP, really just a data pipe, default is /dev/dsp.]
)

AC_ARG_WITH(dev-mixer,
    [  --with-dev-mixer=path          Path to OSS sound mixer, default is /dev/mixer.],
    [dev_mixer=$withval],
    [dev_mixer=/dev/mixer]
)
AC_DEFINE_UNQUOTED(DEV_MIXER, "$dev_mixer",
    [Path to OSS mixer, default is /dev/mixer.]
)

AC_ARG_WITH(cdda-device,
    [  --with-cdda-device=path        Path to default cdaudio device.],
    [AC_DEFINE_UNQUOTED(CDDA_DEVICE, "$withval",
         [Path to default cdaudio device.]
     )
    ]
)

AC_ARG_WITH(cdda-dir,
    [  --with-cdda-dir=path           Path to default cdaudio directory.],
    [AC_DEFINE_UNQUOTED(CDDA_DIRECTORY, "$withval",
         [Path to default cdaudio directory.]
     )
    ]
)

dnl These plugins are always built.

INPUT_PLUGINS="tonegen console sexypsf wav cue alac metronom"
OUTPUT_PLUGINS="disk_writer null"
EFFECT_PLUGINS="audiocompress ladspa voice_removal stereo_plugin echo_plugin"
GENERAL_PLUGINS="song_change alarm"
VISUALIZATION_PLUGINS="blur_scope spectrum"
CONTAINER_PLUGINS="m3u pls stdio"

dnl Check for Audacious

PKG_CHECK_MODULES(AUDACIOUS, [audacious >= 1.2.99],
    [ADD_PC_REQUIRES([audacious >= 1.2.99])],
    [AC_MSG_ERROR([Cannot find Audacious, have you installed audacious yet?])]
)

dnl Check for GTK/GLib/GThread/Pango

PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.6.0 gthread-2.0],
    [ADD_PC_REQUIRES([glib-2.0 >= 2.6.0])],
    [AC_MSG_ERROR([Cannot find glib2 >= 2.6.0])]
)

PKG_CHECK_MODULES(GTK, [gtk+-2.0 >= 2.6.0],
    [ADD_PC_REQUIRES([gtk+-2.0 >= 2.6.0])],
    [AC_MSG_ERROR([Cannot find gtk2 >= 2.6.0])]
)

PKG_CHECK_MODULES(PANGO, [pango >= 1.8.0],
    [ADD_PC_REQUIRES([pango >= 1.8.0])],
    [AC_MSG_ERROR([Cannot find pango >= 1.8.0])]
)

dnl Check for libglade

PKG_CHECK_MODULES(LIBGLADE, [libglade-2.0 >= 2.3.1],
    [],
    [AC_MSG_ERROR([Cannot find libglade])]
)

AC_PATH_PROG(TAGLIB_CONFIG, taglib-config, [no], [$PATH:$prefix/bin])
if test "x$TAGLIB_CONFIG" = "xno" ; then
AC_MSG_ERROR([Cannot find taglib])
else
    TAGLIB_CFLAGS=[`$TAGLIB_CONFIG --cflags`]
    TAGLIB_LIBS=[`$TAGLIB_CONFIG --libs`]
fi
AC_SUBST(TAGLIB_CFLAGS)
AC_SUBST(TAGLIB_LIBS)

dnl IPv6 support
dnl ========================
AC_ARG_ENABLE(ipv6,
	[  --enable-ipv6		  enable IPv6 support (default=no)],
			enable_ipv6=$enableval, enable_ipv6=no)
if test "x$enable_ipv6" = xyes; then
    AC_DEFINE(USE_IPV6,, [Define if building with IPv6 support] )
fi
AM_CONDITIONAL(USE_IPV6,test "x$enable_ipv6" = xyes)
AC_SUBST(USE_IPV6)

dnl chardet support
dnl ========================
AC_ARG_ENABLE(chardet,
	[  --enable-chardet	  enable character set detection support (default=no)],
			enable_chardet=$enableval, enable_chardet=no)
if test "x$enable_chardet" = xyes; then
    AC_DEFINE(USE_CHARDET, 1, [Define if character set detection enabled] )
fi
AM_CONDITIONAL(USE_CHARDET,test "x$enable_chardet" = xyes)
AC_SUBST(USE_CHARDET)

dnl plugin stuff
dnl ========================

dnl Ok, first we find out what flags we need to use.
AC_MSG_CHECKING([for what extension and flags to use for plugin compilation])
case "$target" in
	*-apple-*)
		AC_MSG_RESULT([Mac OS X: -fPIC -bundle -fno-common -flat_namespace -undefined suppress, .dylib])
		PICFLAGS="-fPIC -DPIC"
		PICLDFLAGS="-fPIC -DPIC -bundle -fno-common -flat_namespace -undefined suppress"
		LIBLDFLAGS="-dynamiclib"
		AUDLDFLAGS=""
		SHARED_SUFFIX=".dylib"
		;;
	*)
		AC_MSG_RESULT([libdl-compatible: -fPIC -DPIC -shared, .so])
		PICFLAGS="-fPIC -DPIC"
		PICLDFLAGS="-fPIC -DPIC -shared"
		LIBLDFLAGS="-fPIC -DPIC -shared"
		AUDLDFLAGS="-Wl,-export-dynamic -Wl,-rpath=\${libdir}"
		SHARED_SUFFIX=".so"
		;;
esac
AC_SUBST(PICFLAGS)
AC_SUBST(PICLDFLAGS)
AC_SUBST(LIBLDFLAGS)
AC_SUBST(AUDLDFLAGS)
AC_SUBST(SHARED_SUFFIX)
AC_DEFINE_UNQUOTED(SHARED_SUFFIX, "${SHARED_SUFFIX}",
	[Define the shared module suffix extension on your platform.])

AC_MSG_CHECKING([if you are running Apple-GCC])
case "$target" in
	*-apple-*)
		AC_MSG_RESULT([yes, sorry you poor bastard])
		LDFLAGS="$LDFLAGS -Wl,-framework -Wl,CoreFoundation -Wl,-framework -Wl,CoreServices"
		;;
	*)
		AC_MSG_RESULT([no])
		;;
esac

dnl GConf support

AC_ARG_ENABLE( gconf,
    [  --enable-gconf          enable GConf support (default=disabled)],
    [enable_gconf=$enableval],
    [enable_gconf="no"]
)

if test "$enable_gconf" = "yes"; then
    PKG_CHECK_MODULES(GCONF, [gconf-2.0 >= 2.6.0],
        [
         AC_DEFINE(HAVE_GCONF, , [Define if building with GConf support])
         ADD_PC_REQUIRES([gconf-2.0 >= 2.6.0])
        ],
        [AC_MSG_ERROR([Cannot find GConf])]
    )
fi

if test "$enable_gconf" = "yes"; then
	CONFIGDB_BACKEND="gconf"
else
	CONFIGDB_BACKEND="rcfile"
fi

AC_SUBST(CONFIGDB_BACKEND)

dnl Check for esound

AC_ARG_ENABLE( esd,
    [  --disable-esd           disable esound output plugin [default=enabled]],
    [enable_esd=$enableval],
    [enable_esd="yes"]
)

if test "$enable_esd" = "yes"; then
    PKG_CHECK_MODULES(ESD, [esound >= 0.2],
        [have_esd=yes
         OUTPUT_PLUGINS="$OUTPUT_PLUGINS esd"],
        [have_esd=no]
    )
else
    AC_MSG_RESULT([*** esound plugin disabled per user request ***])
    have_esd=no
fi

dnl Check for PulseAudio

AC_ARG_ENABLE( pulse,
    [  --disable-pulse         disable PulseAudio output plugin [default=enabled]],
    [enable_pulse=$enableval],
    [enable_pulse="yes"]
)

if test "$enable_pulse" = "yes"; then
    PKG_CHECK_MODULES(PULSE, [libpulse >= 0.9.3],
         [have_pulse=yes
          OUTPUT_PLUGINS="$OUTPUT_PLUGINS pulse_audio"],
         [have_pulse=no]
    )
else
    AC_MSG_RESULT([*** pulseaudio output plugin disabled per user request ***])
    have_pulse=no
fi

dnl *** CoreAudio

AC_ARG_ENABLE(coreaudio,
    [  --disable-coreaudio     disable CoreAudio output plugin (default=enabled)],
    [enable_coreaudio=$enableval],
    [enable_coreaudio="yes"]
)

if test "$enable_coreaudio" = "yes"; then
	AC_CHECK_HEADERS([CoreServices/CoreServices.h],
		[have_coreaudio=yes
		 OUTPUT_PLUGINS="$OUTPUT_PLUGINS CoreAudio"],
		[have_coreaudio=no])
else
	AC_MSG_RESULT([*** CoreAudio output plugin disabled per user request ***])
	have_coreaudio=no
fi

dnl *** AltiVec

AC_CHECK_HEADERS(altivec.h,
    [AC_DEFINE(HAVE_ALTIVEC, 1, [Define to 1 if your system has AltiVec.])
     AC_DEFINE(HAVE_ALTIVEC_H, 1, [Define to 1 if your system has an altivec.h file.])
     AC_DEFINE(ARCH_POWERPC, 1, [Define to 1 if your system is a PowerPC.])
     DCT64=dct64_altivec.c],
    [DCT64=dct64.c]
)    
AC_SUBST(DCT64)

dnl *** MP3

AC_ARG_ENABLE(mp3,
    [  --disable-mp3           disable mp3 plugin. (default=enabled) ],
    [enable_mp3=$enableval],
    [enable_mp3=yes]
)

if test "$enable_mp3" = "yes"; then
	PKG_CHECK_MODULES(MAD, [mad],
	     [INPUT_PLUGINS="$INPUT_PLUGINS madplug"])
fi
# XXX rtsp support
#        PKG_CHECK_MODULES(LIBNMS,[libnms >= 0.6.0],
#            [LIBNMS_SRC=rtsp.c
#             AC_DEFINE(HAVE_NEMESI,,[Define if rtp/rtsp support is available])
#            ],[LIBNMS_SRC=""])
#        AC_SUBST(LIBNMS_CFLAGS)
#        AC_SUBST(LIBNMS_LIBS)
#        AC_SUBST(LIBNMS_SRC)
# fi

dnl *** Rocklight

AC_ARG_ENABLE(rocklight,
    AS_HELP_STRING([--disable-rocklight], [disable Rocklight vis plugin (default=enabled)]),
    [enable_rocklight="$enableval"],
    [enable_rocklight="yes"]
)

if test "$enable_rocklight" = "yes"; then
    VISUALIZATION_PLUGINS="$VISUALIZATION_PLUGINS rocklight"
fi

dnl *** LIRC client libraries

AC_ARG_ENABLE(lirc,
    [  --disable-lirc          disable LIRC plugin (default=enabled)],
    [enable_lirc=$enableval],
    [enable_lirc="yes"]
)

if test "$enable_lirc" = "yes"; then
    AC_CHECK_LIB([lirc_client],[lirc_init],[have_lirc=yes],[have_lirc=no])
else
    AC_MSG_RESULT([*** LIRC plugin disabled per user request ***])
    have_lirc="no"
fi

if test "$have_lirc" = "yes"; then
	GENERAL_PLUGINS="$GENERAL_PLUGINS lirc"
fi

dnl *** EvDev-Plug general plugin (only built on Linux)

AC_ARG_ENABLE(evdevplug,
    [  --disable-evdevplug     disable Linux evdev plugin (default=enabled)],
    [enable_evdevplug=$enableval],
    [enable_evdevplug="yes"]
)

if test "x$enable_evdevplug" = "xyes"; then
    case "$target" in
        *-*-linux*)
            have_evdevplug="yes"
        ;;
        *)
            AC_MSG_RESULT([*** Linux evdev plugin disabled (host does not run linux) ***])
            have_evdevplug="no"
        ;;
    esac
else
    AC_MSG_RESULT([*** Linux evdev plugin disabled per user request ***])
    have_evdevplug="no"
fi

if test "x$have_evdevplug" = "xyes"; then
    GENERAL_PLUGINS="$GENERAL_PLUGINS evdev-plug"
fi

dnl *** Status Icon plugin (for X11 only)

AC_ARG_ENABLE(statusicon,
    [  --disable-statusicon    disable X11 Status Icon plugin (default=enabled)],
    [enable_statusicon=$enableval],
    [enable_statusicon="yes"]
)

if test "x$enable_statusicon" = "xyes"; then
    have_statusicon="yes"
    PKG_CHECK_MODULES(GDKX11,[gdk-x11-2.0 >= 2.6.0],,[have_statusicon="no"])
else
    AC_MSG_RESULT([*** X11 Status Icon plugin disabled per user request ***])
    have_statusicon="yes"
fi

if test "x$have_statusicon" = "xyes"; then
    GENERAL_PLUGINS="$GENERAL_PLUGINS statusicon"
fi

dnl *** Audacious OSD plugin (pangocairo-based)

AC_ARG_ENABLE(aosd,
    [  --disable-aosd          disable X11 Audacious OSD plugin (default=enabled)],
    [enable_aosd=$enableval],
    [enable_aosd="yes"]
)

if test "x$enable_aosd" = "xyes"; then
    have_aosd="yes"
    PKG_CHECK_MODULES(PANGO,[pango >= 1.14.7],,
        [have_aosd="no"
        AC_MSG_RESULT([*** pango >= 1.14.7 is required for Audacious OSD plugin ***])]
    )
    PKG_CHECK_MODULES(CAIRO,[cairo >= 1.2.6],,
        [have_aosd="no"
        AC_MSG_RESULT([*** cairo >= 1.2.6 is required for Audacious OSD plugin ***])]
    )
    PKG_CHECK_MODULES(PANGOCAIRO,[pangocairo >= 1.14.7],,
        [have_aosd="no"
        AC_MSG_RESULT([*** pangocairo >= 1.14.7 is required for Audacious OSD plugin ***])]
    )
else
    AC_MSG_RESULT([*** Audacious OSD plugin disabled per user request ***])
    have_aosd="yes"
fi

if test "x$have_aosd" = "xyes"; then
    GENERAL_PLUGINS="$GENERAL_PLUGINS aosd"
fi

dnl *** AdPlug requirement (libbinio)

AC_ARG_ENABLE(adplug,
    [  --disable-adplug        disable AdPlug plugin (default=enabled)],
    [enable_adplug=$enableval],
    [enable_adplug="yes"]
)

if test "$enable_adplug" = "yes"; then
    have_adplug="yes"
    PKG_CHECK_MODULES(BINIO,[libbinio >= 1.4],,[have_adplug=no])
else
    AC_MSG_RESULT([*** AdPlug plugin disabled per user request ***])
    have_adplug="no"
fi

if test "$have_adplug" = "yes"; then
	INPUT_PLUGINS="$INPUT_PLUGINS adplug"
fi

dnl *** Ogg Vorbis

AC_ARG_ENABLE(vorbis,
    [  --disable-vorbis        disable Ogg Vorbis input plugin (default=enabled)],
    [enable_vorbis=$enableval],
    [enable_vorbis="yes"]
)

if test "$enable_vorbis" = "yes"; then
    PKG_CHECK_MODULES(OGG_VORBIS, [ogg >= 1.0 vorbis >= 1.0 vorbisfile >= 1.0],
        [have_oggvorbis=yes],
        [AC_MSG_WARN([*** Cannot find libogg/libvorbis, Ogg Vorbis support will not be built ***])
         have_oggvorbis=no]
    )
else
    AC_MSG_RESULT([*** Ogg Vorbis plugin disabled per user request ***])
    have_oggvorbis=no
fi

if test "$have_oggvorbis" = "yes"; then
	INPUT_PLUGINS="$INPUT_PLUGINS vorbis"
fi

dnl *** WavPack 4.31 support

AC_ARG_ENABLE(wavpack,
    [  --disable-wavpack       disable WavPack input plugin (default=enabled)],
    [enable_wavpack=$enableval],
    [enable_wavpack="yes"]
)

if test "$enable_wavpack" = "yes"; then
    PKG_CHECK_MODULES(WAVPACK, [wavpack >= 4.31],
        [have_wavpack=yes],
        [AC_MSG_WARN([*** Cannot find WavPack 4.31+, WavPack support will not be built ***])
         have_wavpack=no]
    )
else
    AC_MSG_RESULT([*** WavPack plugin disabled per user request ***])
    have_wavpack=no
fi

if test "$have_wavpack" = "yes"; then
	INPUT_PLUGINS="$INPUT_PLUGINS wavpack"
fi

dnl *** AAC

AC_ARG_ENABLE(aac,
    [  --disable-aac           disable aac plugin (default=enabled) ],
    [enable_aac=$enableval],
    [enable_aac=yes]
)

if test "$enable_aac" = "yes"; then
	INPUT_PLUGINS="$INPUT_PLUGINS aac"
fi

dnl *** sndfile

AC_ARG_ENABLE(sndfile,
    [  --disable-sndfile       disable sndfile extensions. [default=enabled] ],
    [enable_sndfile=$enableval],
    [enable_sndfile=yes]
)

if test "$enable_sndfile" = "yes"; then
    PKG_CHECK_MODULES(SNDFILE, [sndfile >= 0.19],
        [enable_sndfile=yes],
        [enable_sndfile=no]
    )
else
    AC_MSG_RESULT([*** libsndfile extensions disabled per user request ***])
    enable_sndfile=no
fi

if test "$enable_sndfile" = "yes"; then
	WAV_SNDFILE="-sndfile"
fi

AC_SUBST(WAV_SNDFILE)

dnl *** modplug

AC_ARG_ENABLE(modplug,
    [  --disable-modplug       disable ModPlug plugin (default=enabled)],
    [enable_modplug=$enableval],
    [enable_modplug="yes"]
)

if test "$enable_modplug" = "yes"; then
    INPUT_PLUGINS="$INPUT_PLUGINS modplug"
    have_modplug="yes"
else
    AC_MSG_RESULT([*** ModPlug plugin disabled per user request ***])
    have_modplug="no"
fi

dnl *** flac
AC_ARG_ENABLE( flac,
[  --disable-flac          disable flac input plugin (default=enabled)],,
                enable_flac="yes")

if test "x$enable_flac" = xyes; then
        AM_PATH_LIBFLAC(have_flac=yes, have_flac=no)
else
        AC_MSG_RESULT([*** flac plugin disabled per user request ***])
        have_flac=no
fi

if test "$have_flac" = "yes"; then
	INPUT_PLUGINS="$INPUT_PLUGINS flac$FLAC_VERSION"
fi

dnl *** Musepack

AC_ARG_ENABLE(musepack,
    [  --disable-musepack      disable musepack input plugin (default=enabled)],
    [enable_musepack=$enableval],
    [enable_musepack="yes"]
)

if test "$enable_musepack" = "yes"; then
    AC_CHECK_HEADERS(mpcdec/config_types.h,,
        enable_musepack=no
    )
else
    AC_MSG_RESULT([*** Musepack plugin disabled per user request ***])
    enable_musepack=no
fi

if test "$enable_musepack" = "yes"; then
        INPUT_PLUGINS="$INPUT_PLUGINS musepack"
fi

dnl *** WMA

AC_ARG_ENABLE(wma,
    [  --disable-wma           disable wma plugin. (default=enabled)],
    [enable_wma=$enableval],
    [enable_wma=yes]
)

if test "$enable_wma" = "yes"; then
	INPUT_PLUGINS="$INPUT_PLUGINS wma"
fi

dnl *** jack output plugin
AC_ARG_ENABLE( jack,
[  --disable-jack          disable jack output plugin (default=enabled)], [enable_jack=$enableval],
                [enable_jack="yes"])

if test "x$enable_jack" = xyes; then
        AM_PATH_JACK(have_jack=yes, have_jack=no)
else
        AC_MSG_RESULT([*** jack plugin disabled per user request ***])
        have_jack=no
fi

if test "$have_jack" = yes; then
	OUTPUT_PLUGINS="$OUTPUT_PLUGINS jack"
fi

dnl *** arts output plugin
AC_ARG_ENABLE( arts,
[  --disable-arts          disable arts output plugin (default=enabled)],,
                enable_arts="yes")

if test "x$enable_arts" = xyes; then
        AM_PATH_ARTSC(0.9.5, have_arts=yes, have_arts=no)
else
        AC_MSG_RESULT([*** arts plugin disabled per user request ***])
        have_arts=no
fi

if test "$have_arts" = yes; then
	OUTPUT_PLUGINS="$OUTPUT_PLUGINS arts"
fi

dnl *** sid
AC_ARG_ENABLE( sid,
[  --disable-sid           disable sid input plugin (default=enabled)],,
                enable_sid="yes")

if test "x$enable_sid" = xyes; then
        AM_PATH_SIDPLAY(have_sidplay=yes, have_sidplay=no)
else
        AC_MSG_RESULT([*** sid plugin disabled per user request ***])
        have_sidplay=no
fi

if test "$have_sidplay" = yes; then
	INPUT_PLUGINS="$INPUT_PLUGINS sid"
fi

dnl *** OSS output

AC_ARG_ENABLE(oss,
    [  --disable-oss           disable the OSS output plugin (default=enabled)],
    [enable_oss=$enableval],
    [enable_oss=yes]
)

have_oss=no
if test "$enable_oss" = "yes"; then
    AC_MSG_CHECKING(for OSS include dir)
    OSS_CFLAGS=""
    if test -f "/etc/oss.conf" ; then
        for i in `cat /etc/oss.conf`; do
            t=`echo $i | sed -e 's/OSSLIBDIR=//'`
            if test "$i" != "$t" ; then
                if test -f "$t/include/sys/soundcard.h" -o -f "$i/include/soundcard.h" ; then
                    OSS_CFLAGS="-I$t/include"
                fi
            fi
        done
    fi
    if test -n "$OSS_CFLAGS" ; then
        AC_MSG_RESULT([$OSS_CFLAGS])
    else
        AC_MSG_RESULT([not found])
    fi
    CFLAGS_save=$CFLAGS
    CFLAGS="$CFLAGS $OSS_CFLAGS"
    AC_CHECK_HEADERS(soundcard.h)
    AC_CHECK_HEADERS(sys/soundcard.h)
    AC_CHECK_HEADERS(machine/soundcard.h)
    CFLAGS=$CFLAGS_save

    if test "${ac_cv_header_soundcard_h}" = "yes" || test "${ac_cv_header_sys_soundcard_h}" = "yes" || test "${ac_cv_header_machine_soundcard_h}" = "yes"; then
        have_oss=yes
    fi

    if test "${have_oss}" = "yes"; then
	AC_MSG_CHECKING(whether we need -lossaudio)
        AC_TRY_LINK([
		#include <sys/ioctl.h>
		#ifdef HAVE_SYS_SOUNDCARD_H
		#include <sys/soundcard.h>
		#else
		#include <soundcard.h>
		#endif
        ], [
		int fd, value;
		ioctl(fd, SOUND_MIXER_READ_VOLUME, &value);
	], AC_MSG_RESULT(no), [
		OSS_LIBS="-lossaudio"
		AC_MSG_RESULT(yes)
        ])
   fi
fi

if test "$have_oss" = "yes"; then
    AC_DEFINE(HAVE_OSS, 1, [Define if the OSS output plugin should be built])
else
    have_oss=no
fi

if test "$have_oss" = "yes"; then
	OUTPUT_PLUGINS="$OUTPUT_PLUGINS OSS"
fi

AC_SUBST(OSS_LIBS)

dnl *** ALSA output plugin

AC_ARG_ENABLE(alsa,
    [  --disable-alsa          disable ALSA input plugin (default=enabled)],
    [enable_alsa=$enableval],
    [enable_alsa=yes]
)

if test "$enable_alsa" = "yes"; then
    PKG_CHECK_MODULES(ALSA, [alsa >= 1.0],
[
have_alsa=yes
alsalib_available=yes
],
[
have_alsa=no
alsalib_available=no
]
    )
else
    AC_MSG_RESULT([*** ALSA output plugin disabled per user request ***])
    have_alsa=no
    alsalib_available=check
fi

if test "$have_alsa" = "yes"; then
	OUTPUT_PLUGINS="$OUTPUT_PLUGINS alsa"
fi


dnl *** amidi-plug (note: to avoid checking twice ALSA, this should appear somewhere after the alsa ouput plugin check)

AC_ARG_ENABLE(amidiplug,
    [  --disable-amidiplug     disable amidi-plug input plugin (default=check) ],
    [enable_amidiplug=$enableval],
    [enable_amidiplug=check]
)

AC_ARG_ENABLE(amidiplug-alsa,
    [  --disable-amidiplug-alsa    disable amidi-plug ALSA backend (default=enabled) ],
    [enable_amidiplug_alsa=$enableval],
    [enable_amidiplug_alsa=auto]
)

AC_ARG_ENABLE(amidiplug-flsyn,
    [  --disable-amidiplug-flsyn   disable amidi-plug FluidSynth backend (default=enabled) ],
    [enable_amidiplug_fluidsynth=$enableval],
    [enable_amidiplug_fluidsynth=auto]
)

AC_ARG_ENABLE(amidiplug-dummy,
    [  --disable-amidiplug-dummy   disable amidi-plug dummy backend (default=enabled) ],
    [enable_amidiplug_dummy=$enableval],
    [enable_amidiplug_dummy=auto]
)


if test "x$enable_amidiplug" = "xno"; then
  AC_MSG_RESULT([*** amidi-plug disabled per user request ***])
  enable_amidiplug=no
else
  if test "x$enable_amidiplug_alsa" != "xauto"; then
    enable_amidiplug=yes
  elif test "x$enable_amidiplug_fluidsynth" != "xauto"; then
    enable_amidiplug=yes
  elif test "x$enable_amidiplug_dummy" != "xauto"; then
    enable_amidiplug=yes
  fi
fi

if test "x$enable_amidiplug" = "xcheck"; then
  if test "x$alsalib_available" = "xcheck"; then
    PKG_CHECK_MODULES(ALSA, [alsa >= 1.0],
      [alsalib_available=yes],[alsalib_available=no])
  fi
  if test "x$alsalib_available" = "xyes"; then
    AM_PATH_AMIDIPLUG_HWSYNTH(ap_have_hwsynth=yes, ap_have_hwsynth=no)
    if test "x$ap_have_hwsynth" = "xyes"; then
      enable_amidiplug=yes
      ap_disable_timidity=yes
    else
      PKG_CHECK_MODULES(FLUIDSYNTH, [fluidsynth >= 1.0.6],
        [fluidsynth_available=yes],[fluidsynth_available=no])
      if test "x$fluidsynth_available" = "xyes"; then
        enable_amidiplug=yes
        ap_disable_timidity=yes
      else
        enable_amidiplug=no
        AC_MSG_WARN([*** Neither an ALSA-supported hardware synth or FluidSynth software synth (>= 1.0.6) have been found (even if ALSA was detected), so amidi-plug won't be compiled unless you explicitly request it with --enable-amidiplug ***])
      fi
    fi
  else
    PKG_CHECK_MODULES(FLUIDSYNTH, [fluidsynth >= 1.0.6],
      [fluidsynth_available=yes],[fluidsynth_available=no])
    if test "x$fluidsynth_available" = "xyes"; then
      enable_amidiplug=yes
      ap_disable_timidity=yes
    else
      enable_amidiplug=no
      AC_MSG_WARN([*** Neither ALSA or FluidSynth software synth (>= 1.0.6) have been found, so amidi-plug won't be compiled unless you explicitly request it with --enable-amidiplug ***])
    fi
  fi
fi

if test "x$enable_amidiplug" = "xyes"; then
  INPUT_PLUGINS="$INPUT_PLUGINS amidi-plug"
  PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.6.0],,)
  PKG_CHECK_MODULES(GMODULE, [gmodule-2.0 >= 2.6.0],,)
  AMIDIPLUGBACKENDDIR=${libdir}/amidi-plug/audacious
  AC_SUBST(AMIDIPLUGBACKENDDIR)
  if test "x$enable_amidiplug_alsa" = "xauto" -o "x$enable_amidiplug_alsa" = "xyes"; then
    if test "x$alsalib_available" = "xcheck"; then
      PKG_CHECK_MODULES(ALSA, [alsa >= 1.0],
        [alsalib_available=yes],[alsalib_available=no])
    fi
    if test "x$alsalib_available" = "xyes"; then
      AMIDIPLUG_BACKENDS="$AMIDIPLUG_BACKENDS backend-alsa"
      enable_amidiplug_alsa=yes
    else
      if test "x$enable_amidiplug_alsa" = "xyes"; then
        AC_MSG_ERROR([Cannot find ALSA development files (ver >= 1.0), but compilation of AMIDI-Plug ALSA backend has been explicitly requested; please install ALSA dev files and run configure again])
        enable_amidiplug_alsa=no
      else
        enable_amidiplug_alsa=no
      fi
    fi
  fi
  if test "x$enable_amidiplug_fluidsynth" = "xauto" -o "x$enable_amidiplug_fluidsynth" = "xyes"; then
    if test "x$fluidsynth_available" != "xyes" -a "x$fluidsynth_available" != "xno"; then
      PKG_CHECK_MODULES(FLUIDSYNTH, [fluidsynth >= 1.0.6],
        [fluidsynth_available=yes],[fluidsynth_available=no])
    fi
    if test "x$fluidsynth_available" = "xyes"; then
      AMIDIPLUG_BACKENDS="$AMIDIPLUG_BACKENDS backend-fluidsynth"
      enable_amidiplug_fluidsynth=yes
    else
      if test "x$enable_amidiplug_fluidsynth" = "xyes"; then
        AC_MSG_ERROR([Cannot find FluidSynth development files (ver >= 1.0.6), but compilation of AMIDI-Plug FluidSynth backend has been explicitly requested; please install FluidSynth dev files and run configure again])
        enable_amidiplug_fluidsynth=no
      else
        enable_amidiplug_fluidsynth=no
      fi
    fi
  fi
  if test "x$enable_amidiplug_dummy" = "xauto" -o "x$enable_amidiplug_dummy" = "xyes"; then
    AMIDIPLUG_BACKENDS="$AMIDIPLUG_BACKENDS backend-dummy"
    enable_amidiplug_dummy=yes
  fi
  AC_SUBST(AMIDIPLUG_BACKENDS)
fi


dnl *** TiMidity input plugin (note: should appear somewhere after the amidi-plug check)

AC_ARG_ENABLE(timidity,
[  --disable-timidity      disable timidity plugin. (default=enabled)],
[enable_timidity=$enableval],
[
  if test "x$ap_disable_timidity" = "xyes"; then
    enable_timidity=no
  else
    enable_timidity=yes
  fi
]
)

if test "$enable_timidity" = "yes"; then
	INPUT_PLUGINS="$INPUT_PLUGINS timidity"
fi

dnl FreeBSD newpcm driver detection

AC_CACHE_CHECK(for FreeBSD newpcm driver, beep_cv_newpcm_driver,
    if test -r "/dev/sndstat"; then
        grep 'newpcm' /dev/sndstat 2>/dev/null 1>/dev/null
        if test "x$?" = "x0"; then
            beep_cv_newpcm_driver="yes"
        else
            beep_cv_newpcm_driver="no"
        fi
    else
        beep_cv_newpcm_driver="no"
    fi
)

if test "$beep_cv_newpcm_driver" = "yes"; then
    AC_DEFINE(HAVE_NEWPCM, 1, [Define if you have the FreeBSD newpcm driver])
fi


AC_CHECK_HEADERS(linux/cdrom.h)
AC_CHECK_HEADERS(sys/cdio.h)
AC_CHECK_HEADERS(sys/cdrio.h)
AC_CHECK_HEADERS(linux/joystick.h)
AC_CHECK_HEADERS(unistd.h)
AC_CHECK_HEADERS(fcntl.h)
AC_CHECK_HEADERS(sys/ioctl.h)
AC_CHECK_HEADERS(fnmatch.h)
AC_CHECK_HEADERS(limits.h)
AC_CHECK_HEADERS(wchar.h)
AC_CHECK_HEADERS(fts.h)

AC_CHECK_FUNCS([mkdtemp getmntinfo statvfs strtoul lrintf])


if test "${ac_cv_header_linux_cdrom_h}" = "yes" || test "${ac_cv_header_sys_cdio_h}" = "yes"
then
    INPUT_PLUGINS="cdaudio $INPUT_PLUGINS"
    case "$host" in
        *-*-sunos* | *-*-solaris* | *-*-linux*)
            AC_DEFINE(BEEP_CDROM_SOLARIS,, [Define if cdrom access is in Solaris style])
        ;;
        *-*-freebsd*)
            AC_DEFINE(BEEP_CDROM_BSD,, [Define if cdrom access is in BSD style])
        ;;
        *-*-netbsd* | *-*-openbsd* | *-*-mirbsd*)
            AC_DEFINE(BEEP_CDROM_BSD,, [Define if cdrom access is in BSD style])
            AC_DEFINE(BEEP_CDROM_BSD_NETBSD,, [Define if cdrom access uses NetBSD variant])
        ;;
        *-*darwin*)
            AC_DEFINE(BEEP_CDROM_BSD,, [Define if cdrom access is in BSD style])
            AC_DEFINE(BEEP_CDROM_BSD_DARWIN,, [Define if cdrom access uses Darwin variant])
        ;;
    esac
fi


AM_CONDITIONAL(HAVE_CDROM, test "${ac_cv_header_linux_cdrom_h}" = "yes" || test "${ac_cv_header_sys_cdio_h}" = "yes")
AM_CONDITIONAL(HAVE_LINUX_JOYSTICK,test "${ac_cv_header_linux_joystick_h}" = "yes")

CFLAGS="$CFLAGS $AUDACIOUS_CFLAGS"

AC_MSG_CHECKING(if __ELF__ is defined)
AC_EGREP_CPP(yes,
    [#ifdef __ELF__
        yes
     #endif
    ],
    [is_elf=yes],
    [is_elf=no]
)
AC_MSG_RESULT([${is_elf}])


have_solaris=no
have_sun=no
ARCH_DEFINES=""
case "$host" in
    *-*-openbsd* | *-*-netbsd* | *-*-mirbsd*)
        have_sun=yes
        if test "${is_elf}" = "no"
        then
            AC_DEFINE(SYMBOL_PREFIX, "_", [Define to symbol prefix, if any])
        fi
        OUTPUT_PLUGINS="$OUTPUT_PLUGINS sun"
    ;;
    *-*-darwin*)
        AC_DEFINE(SYMBOL_PREFIX, "_", [Define to symbol prefix, if any])
    ;;
    *-hpux-*)
        ARCH_DEFINES="-DHPUX"
    ;;
    *-*-solaris* | *-*-sunos*)
        have_solaris=yes
    ;;
esac

arch_type=other
x86_asm=no
case "$host" in
    i386-*-* | i486-*-* | i586-*-* | i686-*-* | i86pc-*-*)
        arch_type=ix86
        case "$host" in
            *-*-netbsd* | *-*openbsd* | *-*-mirbsd*)
            ;;
            *)
                ARCH_DEFINES="-DI386_ASSEM"
                x86_asm=yes
            ;;
        esac
    ;;
esac

dnl *** Scrobbler ***

scrobbler="yes"

dnl libcurl check

my_cv_curl_vers=NONE
dnl check is the plain-text version of the required version
check="7.9.7"
dnl check_hex must be UPPERCASE if any hex letters are present
check_hex="070907"

have_curl="no"
PKG_CHECK_MODULES(CURL, [libcurl >= 7.9.7],
[
	CONTAINER_PLUGINS="$CONTAINER_PLUGINS curl"
	have_curl="yes"
], [scrobbler="no"])

if test "x$scrobbler" = "xyes"; then
	GENERAL_PLUGINS="$GENERAL_PLUGINS scrobbler"
fi

AC_SUBST(CURL_CFLAGS)
AC_SUBST(CURL_LIBS)

dnl *** End of Scrobbler checks ***

dnl *** MMS

AC_ARG_ENABLE(mms,
[  --disable-mms           disable mms support. (default=enabled)],
[have_mms=$enableval],
[have_mms=yes])

if test "x$ap_disable_paranormal" = "xyes"; then
    have_mms=no
    AC_MSG_RESULT([*** mms plugin disabled by request ***])
else
    have_mms=yes
    PKG_CHECK_MODULES(MMS, [libmms >= 0.3], [CONTAINER_PLUGINS="$CONTAINER_PLUGINS mms" ], [have_mms="no"])
fi

dnl *** libparanormal checks

AC_ARG_ENABLE(paranormal,
[  --disable-paranormal    disable paranormal vis plugin. (default=enabled)],
[have_paranormal=$enableval],
[have_paranormal=yes])

if test "x$ap_disable_paranormal" = "xyes"; then
    have_paranormal=no
    AC_MSG_RESULT([*** paranormal plugin disabled by request ***])
else
    have_paranormal=yes
    AM_PATH_SDL(1.2.5, , [
	AC_MSG_WARN([*** SDL is not installed. Paranormal will not be built. ***])
	have_paranormal="no"
    ])
fi

AC_ARG_ENABLE(xspf,
[  --disable-xspf          disable XSPF support. (default=enabled)],
[have_xspf=$enableval], [have_xspf=yes])

AM_PATH_XML2(2.0.0, , [
	AC_MSG_WARN([*** libxml2 is not installed. Paranormal nor XSPF support will not be built. ***])
	have_paranormal="no"
	have_xspf="no"
])

if test "$have_paranormal" = "yes"; then
	VISUALIZATION_PLUGINS="$VISUALIZATION_PLUGINS paranormal rovascope"
fi

if test "$have_xspf" = "yes"; then
	CONTAINER_PLUGINS="$CONTAINER_PLUGINS xspf"
fi

dnl *** ProjectM (GL Milkdrop port)

AC_ARG_ENABLE(projectm,
[  --disable-projectm          disable projectM vis plugin. (default=enabled)],
[have_projectm=$enableval],
[have_projectm=yes])

if test "$have_projectm" = "yes"; then
	PKG_CHECK_MODULES(LIBPROJECTM, libprojectM >= 0.0.0, [have_projectm="yes"], [have_projectm="no"])
	if test x$libprjM = xno; then
	        AC_MSG_WARN([*** libprojectM not found.
	                        libprojectM is needed to build this package.
	                        You can download libprojectM at http://xmms-projectM.sf.net/])
	fi
	LIBPROJECTM_datadir=`pkg-config --variable=pkgdatadir libprojectM`;
	LIBPROJECTM_sysconfdir=`pkg-config --variable=sysconfdir libprojectM`;
	LIBPROJECTM_CFLAGS="$LIBPROJECTM_CFLAGS -DPROJECTM_DATADIR=\\\"$LIBPROJECTM_datadir\\\" -DPROJECTM_SYSCONFDIR=\\\"$LIBPROJECTM_sysconfdir\\\"";
fi
AC_SUBST(LIBPROJECTM_LIBS)
AC_SUBST(LIBPROJECTM_CFLAGS)

if test "$have_projectm" = "yes"; then
	VISUALIZATION_PLUGINS="$VISUALIZATION_PLUGINS projectm"
fi

dnl *** TTA

AC_ARG_ENABLE(tta,
    [  --disable-tta           disable TrueAudio plugin (default=enabled) ],
    [enable_tta=$enableval],
    [enable_tta=yes]
)

if test "$enable_tta" = "yes"; then
	INPUT_PLUGINS="$INPUT_PLUGINS tta"
fi

dnl *** End of all plugin checks ***

AC_SUBST(ARCH_DEFINES)

dnl XXX delete ASAP -nenolod
AM_CONDITIONAL(ARCH_X86, test "x$arch_type" = "xix86")
AM_CONDITIONAL(USE_X86ASM, test "x$x86_asm" = xyes)
AM_CONDITIONAL(HAVE_SOLARIS, test "x$have_solaris" = xyes)
AM_CONDITIONAL(HAVE_SUN, test "x$have_sun" = xyes)

plugindir=`pkg-config audacious --variable=plugin_dir`
AC_SUBST(plugindir)

dnl XXX

INPUT_PLUGIN_DIR=Input
OUTPUT_PLUGIN_DIR=Output
EFFECT_PLUGIN_DIR=Effect
GENERAL_PLUGIN_DIR=General
VISUALIZATION_PLUGIN_DIR=Visualization
CONTAINER_PLUGIN_DIR=Container

AC_SUBST(INPUT_PLUGIN_DIR)
AC_SUBST(OUTPUT_PLUGIN_DIR)
AC_SUBST(EFFECT_PLUGIN_DIR)
AC_SUBST(GENERAL_PLUGIN_DIR)
AC_SUBST(VISUALIZATION_PLUGIN_DIR)
AC_SUBST(CONTAINER_PLUGIN_DIR)
AC_SUBST(pluginsubs)

dnl XXX Work around some autoconf bugs.
if test "$prefix" = "NONE"; then
        prefix="${ac_default_prefix}"
fi

if test -z "$datadir"; then
	datadir="${prefix}/share"
fi

localedir="$datadir/locale"
AC_SUBST(localedir)

BEEP_DEFINES="$CFLAGS -DDATA_DIR=\\\"$datadir/audacious\\\" -DPLUGIN_DIR=\\\"$plugindir\\\" -DPLUGINSUBS=$pluginsubs -DLOCALEDIR=\\\"$localedir\\\""
CFLAGS="$CFLAGS $EXTRA_CFLAGS"

if test "$enable_user_plugin_dir" = "no"; then
    AC_DEFINE(DISABLE_USER_PLUGIN_DIR,, [Define to disable per user plugin directory])
fi

AC_DEFINE(BMP_RCPATH, ".audacious", [Config dir to use])

AC_SUBST(BEEP_DEFINES)

AC_SUBST(beepdir)
AC_SUBST(plugindir)

AC_SUBST(EFFECT_PLUGINS)
AC_SUBST(GENERAL_PLUGINS)
AC_SUBST(INPUT_PLUGINS)
AC_SUBST(OUTPUT_PLUGINS)
AC_SUBST(VISUALIZATION_PLUGINS)
AC_SUBST(CONTAINER_PLUGINS)

AC_CONFIG_FILES([
	mk/rules.mk
	po/Makefile.in
])

AC_OUTPUT


echo
echo "Configuration:"
echo
echo "  Install path:                           ${audacious_plugin_root}"
echo
echo "  Output Plugins"
echo "  --------------"
echo "  Open Sound System (oss):                $have_oss"
echo "  Advanced Linux Sound Arch. (alsa):      $have_alsa"
echo "  Enlightenment Sound Daemon (esd):       $have_esd"
echo "  Jack Audio Connection Kit (jack):       $have_jack"
echo "  Analog Realtime Synthesizer (arts):     $have_arts"
echo "  BSD/SUN audio output (sun):             $have_sun"
echo "  PulseAudio sound server (pulse_audio):  $have_pulse"
echo "  Mac OS X sound support (CoreAudio):     $have_coreaudio"
echo "  Null Audio output (null):               yes"
echo
echo "  Input Plugins"
echo "  -------------"
echo "  MPEG 1/2/3 (madplug):                   $enable_mp3"
echo "  MPEG 4 Audio (AAC):                     $enable_aac"
echo "  Windows Media Audio (wma):              $enable_wma"
echo "  Module decoder (modplug):               $have_modplug"
echo "  MIDI modular plugin (amidi-plug):       $enable_amidiplug"
echo "    -> ALSA backend:                      $enable_amidiplug_alsa"
echo "    -> FluidSynth backend:                $enable_amidiplug_fluidsynth"
echo "    -> dummy backend:                     $enable_amidiplug_dummy"
echo "  MIDI to WAVE converter (timidity):      $enable_timidity"
echo "  CD Digital Audio (cdda):                yes"
echo "  Microsoft WAV (wav):                    yes"
echo "    + sndfile extensions:                 $enable_sndfile"
echo "  Tone Generator:                         yes"
echo "  Ogg Vorbis (vorbis):                    $have_oggvorbis"
echo "  Free Lossless Audio Codec (flac):       $have_flac"
echo "  Commodore 64 audio (sid):               $have_sidplay"
echo "  Game music (spc, nsf & gbs):            yes"
echo "  PlayStation audio (sexypsf):            yes"
echo "  AdLib synthesizer (adplug):             $have_adplug"
echo "  Apple Lossless Audio Codec (alac):      yes"
echo "  WavPack 4.31+ (wavpack):                $have_wavpack"
echo "  Musepack support (musepack):            $enable_musepack"
echo "  TrueAudio (tta):                        $enable_tta"
echo "  Metronom:                               yes"
echo
echo "  General"
echo "  -------"
echo "  Alarm:                                  yes"
echo "  Song Change:                            yes"
echo "  Status Icon:                            $have_statusicon"
echo "  Audacious OSD:                          $have_aosd"
echo "  Control via event device (evdev-plug):  $have_evdevplug"
echo "  LIRC:                                   $have_lirc"
echo "  AudioScrobbler Client:                  $scrobbler"
echo
echo "  Effect"
echo "  ------"
echo "  AudioCompressor (AGC):                  yes"
echo "  LADSPA effects host (ladspa):           yes"
echo "  Voice Removal:                          yes"
echo "  Extra Stereo:                           yes"
echo "  Echo/Surround:                          yes"
echo
echo "  Visualization"
echo "  -------------"
echo "  Blur Scope:                             yes"
echo "  Spectrum Analyzer:                      yes"
echo "  Paranormal Visualization Library:       $have_paranormal"
echo "  ProjectM (GL milkdrop):                 $have_projectm"
echo
echo "  Transport"
echo "  ---------"
echo "  stdio transport:                        yes"
echo "  curl-based http/https:                  $have_curl"
echo "  libmms-based mms:                       $have_mms"
echo
echo "  Container"
echo "  ---------"
echo "  Winamp PLS playlist format (pls):       yes"
echo "  M3U playlist format (m3u):              yes"
echo "  XML Sharable Playlist Format (xspf):    $have_xspf"
echo

if test "x$ap_disable_timidity" = "xyes" && test "x$enable_timidity" = "xno"; then
    echo "Note: an ALSA-supported hardware midi synth or a FluidSynth software synth"
    echo "was detected in configuration; in order to use it, the amidi-plug input"
    echo "plugin will be compiled in place of timidity. If you wish to compile timidity"
    echo "anyway, simply use --enable-timidity in configuration."
    echo ""
fi
