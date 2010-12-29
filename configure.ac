dnl Process this file with autoconf to produce a configure script.

AC_INIT([audacious-plugins], [2.5-devel], [bugs+audacious-plugins@atheme.org])
AC_PREREQ([2.59])

AC_CANONICAL_HOST
AC_CANONICAL_TARGET
AC_CONFIG_HEADERS([config.h])

dnl XXX kludge for FreeBSD
if test -d "/usr/X11R6/include"; then
	CPPFLAGS="$CPPFLAGS -I/usr/X11R6/include"
fi

dnl append audacious header path to CPPFLAGS when --prefix is specified.
if test "x$prefix" != "xNONE" ; then
   CPPFLAGS="$CPPFLAGS -I${includedir}"
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

if test "x$GCC" = "xyes"; then
	CFLAGS="$CFLAGS -pipe -Wall"
	CXXFLAGS="$CXXFLAGS -pipe -Wall"
fi

dnl Large file support
dnl ==================
CPPFLAGS="$CPPFLAGS -D_FILE_OFFSET_BITS=64"

dnl Locate a program and check that its version is acceptable.
dnl AC_PROG_CHECK_VER(var, namelist, version-switch,
dnl 		      [version-extract-regexp], version-glob [, do-if-fail])
AC_DEFUN([AC_CHECK_PROG_VER],
[AC_CHECK_PROGS([$1], [$2])
if test -z "[$]$1"; then
  ac_verc_fail=yes
else
  # Found it, now check the version.
  AC_MSG_CHECKING([version of [$]$1])
changequote(<<,>>)dnl
  ac_prog_version=`<<$>>$1 $3 2>&1 ifelse(<<$4>>,,,
                   <<| sed -n 's/^.*patsubst(<<$4>>,/,\/).*$/\1/p'>>)`
  case $ac_prog_version in
    '') ac_prog_version="v. ?.??, bad"; ac_verc_fail=yes;;
    <<$5>>)
changequote([,])dnl
       ac_prog_version="$ac_prog_version, ok"; ac_verc_fail=no;;
    *) ac_prog_version="$ac_prog_version, bad"; ac_verc_fail=yes;;

  esac
  AC_MSG_RESULT([$ac_prog_version])
fi
ifelse([$6],,,
[if test $ac_verc_fail = yes; then
  $6
fi])
])

GCC42_CFLAGS="-fgnu89-inline"
AC_CHECK_PROG_VER(CC, ${ac_tool_prefix}gcc ${ac_tool_prefix}cc, -v,
  [version \([egcygnustpi-]*[0-9.]*\)], [4.[2-9]* ],
  GCC42_CFLAGS="")

dnl Check for assembler

AM_PROG_AS

dnl Checks for various programs

AC_PROG_LN_S
AC_PROG_MAKE_SET

AC_PATH_PROG(RM, rm)
AC_PATH_PROG(MV, mv)
AC_PATH_PROG(CP, cp)
AC_PATH_PROG(AR, ar)
AC_PATH_PROG(TR, tr)
AC_PATH_PROG(RANLIB, ranlib)

dnl Gettext -nenolod
AM_GNU_GETTEXT([external])

dnl Check for common functions
dnl ==========================

AC_CHECK_FUNCS([fcntl fsync mkdtemp strnlen strtoul])

dnl OSS stuff (Why is this here?)
dnl =============================

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

dnl These plugins are always built.

INPUT_PLUGINS="tonegen metronom vtx"
OUTPUT_PLUGINS="null"
EFFECT_PLUGINS="compressor crossfade crystalizer ladspa sndstretch stereo_plugin voice_removal echo_plugin"
GENERAL_PLUGINS="alarm lyricwiki"
VISUALIZATION_PLUGINS="blur_scope spectrum"
CONTAINER_PLUGINS="m3u pls asx"
TRANSPORT_PLUGINS="unix-io"

dnl Check for Audacious

PKG_CHECK_MODULES(AUDACIOUS, [audacious >= 2.4],
    [ADD_PC_REQUIRES([audacious >= 2.4])],
    [AC_MSG_ERROR([Cannot find Audacious 2.4 or later, have you installed audacious yet?])]
)

CFLAGS="$CFLAGS $AUDACIOUS_CFLAGS"
CPPFLAGS="$CPPFLAGS $AUDACIOUS_CPPFLAGS"
LIBS="$LIBS $AUDACIOUS_LIBS"

dnl Check for GTK/GLib/GThread/Pango

PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.12.0 gthread-2.0],
    [ADD_PC_REQUIRES([glib-2.0 >= 2.12.0])],
    [AC_MSG_ERROR([Cannot find Glib2 >= 2.12.0])]
)

PKG_CHECK_MODULES(GTK, [gtk+-2.0 >= 2.8.0],
    [ADD_PC_REQUIRES([gtk+-2.0 >= 2.8.0])],
    [AC_MSG_ERROR([Cannot find Gtk+2 >= 2.8.0])]
)

AC_ARG_ENABLE(gio, [  --disable-gio           disable GIO support (default=enabled if GIO present)],
 enable_gio=$enableval, enable_gio=yes)

if test "x$enable_gio" = "xyes"; then
    PKG_CHECK_MODULES(GIO, [gio-2.0 >= 2.0], [ADD_PC_REQUIRES([gio-2.0 >= 2.0])], [enable_gio=no])
fi

if test "x$enable_gio" = "xyes"; then
    TRANSPORT_PLUGINS="$TRANSPORT_PLUGINS gio"
fi

PKG_CHECK_MODULES(PANGO, [pango >= 1.8.0],
    [ADD_PC_REQUIRES([pango >= 1.8.0])],
    [AC_MSG_ERROR([Cannot find Pango >= 1.8.0])]
)

dnl Check for libmowgli
dnl ===================

PKG_CHECK_MODULES(MOWGLI, [libmowgli >= 0.4.0],
    [ADD_PC_REQUIRES([libmowgli >= 0.4.0])],
    [AC_MSG_ERROR([Cannot find libmowgli >= 0.4.0, try http://www.atheme.org/projects/mowgli.shtml])]
)

### ---------------------------------------------------------------------------
dnl Pthread support
dnl ===============
PTHREAD_LIBS=
PTHREAD_CFLAGS=

AC_CHECK_LIB(pthread, pthread_create, PTHREAD_LIBS="-lpthread",
  [AC_CHECK_LIB(pthreads, pthread_create, PTHREAD_LIBS="-lpthreads",
	[AC_CHECK_LIB(pthreadGC2, pthread_create, PTHREAD_LIBS="-lpthreadGC2")]
  )]
)

AC_SUBST([PTHREAD_LIBS])
AC_SUBST([PTHREAD_CFLAGS])

dnl IPv6 support
dnl ========================
AC_ARG_ENABLE(ipv6,
	[  --enable-ipv6		  enable IPv6 support (default=no)],
			enable_ipv6=$enableval, enable_ipv6=no)
if test "x$enable_ipv6" = "xyes"; then
    AC_DEFINE(USE_IPV6,, [Define if building with IPv6 support] )
fi
AM_CONDITIONAL(USE_IPV6,test "x$enable_ipv6" = "xyes")
AC_SUBST(USE_IPV6)

dnl chardet support
dnl ========================
AC_ARG_ENABLE(chardet,
	[  --enable-chardet	  enable character set detection support (default=no)],
			enable_chardet=$enableval, enable_chardet=no)
if test "x$enable_chardet" = "xyes"; then
    AC_DEFINE(USE_CHARDET, 1, [Define if character set detection enabled] )
fi
AM_CONDITIONAL(USE_CHARDET,test "x$enable_chardet" = "xyes")
AC_SUBST(USE_CHARDET)

dnl plugin stuff
dnl ========================

BUILDSYS_SHARED_LIB

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

dnl Console
dnl =======

AC_ARG_ENABLE(console,
 [  --disable-console       disable game music decoder (console)],
 [enable_console=$enableval], [enable_console="yes"])

if test $enable_console = yes ; then
    AC_CHECK_HEADERS([zlib.h], [enable_console=yes],
     [AC_MSG_WARN([Cannot find zlib.h; disabling game music decoder (console)])
      enable_console=no])
fi

if test $enable_console = yes ; then
    INPUT_PLUGINS="$INPUT_PLUGINS console"
fi


dnl XSF
dnl =======

AC_ARG_ENABLE(xsf,
 [  --disable-xsf           disable Nintendo DS audio decoder (xsf)],
 [enable_xsf=$enableval], [enable_xsf="yes"])

if test $enable_xsf = yes ; then
    INPUT_PLUGINS="$INPUT_PLUGINS xsf"
fi


dnl PulseAudio
dnl ==========

AC_ARG_ENABLE( pulse,
    [  --disable-pulse         disable PulseAudio output plugin (default=enabled)],
    [enable_pulse=$enableval],
    [enable_pulse="yes"]
)

if test "x$enable_pulse" = "xyes"; then
    PKG_CHECK_MODULES(PULSE, [libpulse >= 0.9.9],
         [have_pulse=yes
          OUTPUT_PLUGINS="$OUTPUT_PLUGINS pulse_audio"],
         [have_pulse=no]
    )
else
    AC_MSG_RESULT([*** pulseaudio output plugin disabled per user request ***])
    have_pulse=no
fi

dnl *** RoarAudio

AC_ARG_ENABLE(roar,
    [  --disable-roar          disable RoarAudio output plugin (default=enabled)],
    [enable_roar=$enableval],
    [enable_roar="yes"]
)

if test "x$enable_roar" = "xyes"; then
    PKG_CHECK_MODULES(LIBROAR, [libroar],
         [have_roar=yes
          OUTPUT_PLUGINS="$OUTPUT_PLUGINS roaraudio"],
         [have_roar=no]
    )
else
    AC_MSG_RESULT([*** roaraudio output plugin disabled per user request ***])
    have_roar=no
fi

dnl *** CoreAudio

AC_ARG_ENABLE(coreaudio,
    [  --disable-coreaudio     disable CoreAudio output plugin (default=enabled)],
    [enable_coreaudio=$enableval],
    [enable_coreaudio="yes"]
)

if test "x$enable_coreaudio" = "xyes"; then
	AC_CHECK_HEADERS([CoreServices/CoreServices.h],
		[have_coreaudio=yes
		 OUTPUT_PLUGINS="$OUTPUT_PLUGINS CoreAudio"],
		[have_coreaudio=no])
else
	AC_MSG_RESULT([*** CoreAudio output plugin disabled per user request ***])
	have_coreaudio=no
fi

dnl *** Dock Album Art (MacOS)

AC_ARG_ENABLE(dockalbumart,
    [  --disable-dockalbumart  disable MacOS dock albumart plugin (default=guess)],
    [enable_dockalbumart=$enableval],
    [enable_dockalbumart="yes"]
)

if test "x$enable_dockalbumart" = "xyes"; then
	AC_CHECK_HEADERS([Carbon/Carbon.h],
		[have_dockalbumart=yes
		 GENERAL_PLUGINS="$GENERAL_PLUGINS dockalbumart"],
		[have_dockalbumart=no])
else
	AC_MSG_RESULT([*** DockAlbumArt plugin disabled per user request ***])
	have_dockalbumart=no
fi

dnl *** AltiVec

AC_ARG_ENABLE(altivec,
    [  --disable-altivec       disable AltiVec support. (default=enabled) ],
    [enable_altivec=$enableval],
    [enable_altivec=yes]
)
if test "x$enable_altivec" = "xyes"; then
  AC_CHECK_HEADERS(altivec.h,
      [AC_DEFINE(HAVE_ALTIVEC, 1, [Define to 1 if your system has AltiVec.])
       AC_DEFINE(HAVE_ALTIVEC_H, 1, [Define to 1 if your system has an altivec.h file.])
       AC_DEFINE(ARCH_POWERPC, 1, [Define to 1 if your system is a PowerPC.])
       DCT64=dct64_altivec.c
       SIMD_CFLAGS=-maltivec
       AC_SUBST(SIMD_CFLAGS)],
      [DCT64=dct64.c]
  )
fi
AC_SUBST(DCT64)

dnl *** SSE2

AC_ARG_ENABLE(sse2,
    [  --disable-sse2          disable SSE2 support. (default=enabled) ],
    [enable_sse2=$enableval],
    [enable_sse2=yes]
)
if test "x$enable_sse2" = "xyes"; then
  AC_MSG_CHECKING(SSE2)
  ac_save_CFLAGS="$CFLAGS"
  CFLAGS="-msse2"
  AC_TRY_RUN([
  #include <emmintrin.h>
  int main()
  {
    _mm_setzero_pd();
    asm volatile("xorpd %xmm0,%xmm0\n\t");
    return 0;
  }
  ],
  [AC_MSG_RESULT(SSE2 yes)
  AC_DEFINE(HAVE_SSE2, 1, [Define to 1 if your system has SSE2])
  SIMD_CFLAGS=-msse2],
  [AC_MSG_RESULT(SSE2 no)],
  )
fi
AC_SUBST(SIMD_CFLAGS)
CFLAGS="$ac_save_CFLAGS"


dnl PSF
dnl ===

AC_ARG_ENABLE(psf,
 [  --disable-psf           disable PlayStation (psf/psf2) audio decoder],
 [enable_psf=$enableval],
 [enable_psf=yes])

if test "$enable_psf" = "yes"; then
    INPUT_PLUGINS="$INPUT_PLUGINS psf"
fi


dnl *** USF

AC_ARG_ENABLE(usf,
    [  --enable-usf            enable the USF input plugin (default=disabled)],
    [enable_usf=$enableval],
    [enable_usf=no]
)

if test "x$enable_usf" = "xyes"; then
    AC_MSG_CHECKING([if your platform is supported for the USF plugin])
    case "$target" in
	x86_64-*-linux-gnu)
		AC_MSG_RESULT([yes, AMD64 Linux])
		enable_usf=yes
		;;
	i*86-*-linux-gnu)
		AC_MSG_RESULT([yes, X86 Linux])
		enable_usf=yes
		;;
	*)
		AC_MSG_RESULT([no])
		enable_usf=no
		;;
    esac

    if test "x$enable_usf" = "xyes"; then
        INPUT_PLUGINS="$INPUT_PLUGINS usf"
    fi
fi

dnl *** MP3

AC_ARG_ENABLE(mp3,
    [  --disable-mp3           disable mp3 plugin. (default=enabled) ],
    [enable_mp3=$enableval],
    [enable_mp3=yes]
)

if test "x$enable_mp3" = "xyes"; then
	INPUT_PLUGINS="$INPUT_PLUGINS mpg123"
fi

dnl *** Rocklight

AC_ARG_ENABLE(rocklight,
    AS_HELP_STRING([--disable-rocklight], [disable Rocklight vis plugin (default=enabled)]),
    [enable_rocklight="$enableval"],
    [enable_rocklight="yes"]
)

if test "x$enable_rocklight" = "xyes"; then
    VISUALIZATION_PLUGINS="$VISUALIZATION_PLUGINS rocklight"
fi

dnl *** LIRC client libraries

AC_ARG_ENABLE(lirc,
    [  --disable-lirc          disable LIRC plugin (default=enabled)],
    [enable_lirc=$enableval],
    [enable_lirc="yes"]
)

if test "x$enable_lirc" = "xyes"; then
    AC_CHECK_LIB([lirc_client],[lirc_init],[have_lirc=yes],[have_lirc=no])
else
    AC_MSG_RESULT([*** LIRC plugin disabled per user request ***])
    have_lirc="no"
fi

if test "x$have_lirc" = "xyes"; then
	AC_CHECK_HEADER(lirc/lirc_client.h,[],[have_lirc=no])
fi

if test "x$have_lirc" = "xyes"; then
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

dnl *** Global Hotkey general plugin (only built on X11)

AC_ARG_ENABLE(hotkey,
    [  --disable-hotkey        disable global hotkey plugin (default=enabled)],
    [enable_hotkey=$enableval],
    [enable_hotkey="yes"]
)

if test "x$enable_hotkey" = "xyes"; then
    have_hotkey="yes"
    PKG_CHECK_MODULES(GDKX11,[gdk-x11-2.0 >= 2.6.0],,[have_hotkey="no"])
else
    AC_MSG_RESULT([*** X11 Global Hotkey plugin disabled per user request ***])
    have_hotkey="no"
fi

if test "x$have_hotkey" = "xyes"; then
    GENERAL_PLUGINS="$GENERAL_PLUGINS hotkey"
fi

dnl *** Gnome Shortcuts Plugin

AC_ARG_ENABLE(gnomeshortcuts,
    [  --disable-gnomeshortcuts        disable gnome shortcuts (default=enabled)],
    [enable_gnomeshortcuts=$enableval],
    [enable_gnomeshortcuts="yes"]
)

if test "x$enable_gnomeshortcuts" = "xyes"; then
    have_gnomeshortcuts="yes"
    PKG_CHECK_MODULES(DBUS, [dbus-1 >= 0.60 dbus-glib-1 >= 0.60],
     [have_gnomeshortcuts=yes],
     [AC_MSG_WARN([Cannot find dbus-glib >= 0.60; disabling Gnome shortcuts plugin.])
      have_gnomeshortcuts=no])
else
    AC_MSG_RESULT([*** Gnome Shortcuts Plugin disabled per user request ***])
    have_gnomeshortcuts="no"
fi

if test "x$have_gnomeshortcuts" = "xyes"; then
    GENERAL_PLUGINS="$GENERAL_PLUGINS gnomeshortcuts"
fi

dnl Song Change
dnl ===========

AC_ARG_ENABLE(songchange,
 [  --disable-songchange    disable song change plugin],
 [enable_songchange=$enableval], [enable_songchange="yes"])

if test $enable_songchange = yes ; then
    AC_CHECK_HEADERS([sys/wait.h], [enable_songchange=yes],
     [AC_MSG_WARN([Cannot find sys/wait.h; disabling song change plugin])
      enable_songchange=no])
fi

if test $enable_songchange = yes ; then
    GENERAL_PLUGINS="$GENERAL_PLUGINS song_change"
fi

dnl Status Icon
dnl ===========

AC_ARG_ENABLE(statusicon,
    [  --disable-statusicon    disable X11 Status Icon plugin (default=enabled)],
    [enable_statusicon=$enableval],
    [enable_statusicon="yes"]
)

if test "x$enable_statusicon" = "xyes"; then
    PKG_CHECK_MODULES(GTK, [gtk+-2.0 >= 2.16.0], [have_statusicon=yes],
     [have_statusicon=no])
else
    AC_MSG_RESULT([*** X11 Status Icon plugin disabled per user request ***])
    have_statusicon="no"
fi

if test "x$have_statusicon" = "xyes"; then
    GENERAL_PLUGINS="$GENERAL_PLUGINS statusicon"
fi

dnl *** Audacious OSD plugin (pangocairo-based)

AC_ARG_ENABLE(aosd,
    [  --disable-aosd          disable Audacious OSD plugin (default=enabled)],
    [enable_aosd=$enableval],
    [enable_aosd="guess"]
)

AC_ARG_ENABLE(aosd_xcomp,
    [  --disable-aosd-xcomp    disable Audacious OSD X Composite Support (default=enabled)],
    [enable_aosd_xcomp=$enableval],
    [enable_aosd_xcomp="yes"]
)

if test "x$enable_aosd" = "xguess"; then
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
    PKG_CHECK_MODULES(XRENDER, xrender,,
        [have_aosd="no"
        AC_MSG_RESULT([*** X Render extension is required for Audacious OSD plugin ***])]
    )
    if test "x$enable_aosd_xcomp" = "xyes"; then
        have_aosd_xcomp="yes"
        PKG_CHECK_MODULES(XCOMPOSITE, xcomposite,
            [AC_DEFINE([HAVE_XCOMPOSITE],[],[X Composite extension available])],
            [have_aosd_xcomp="no"
            AC_MSG_RESULT([*** X Composite extension not found, composite support for Audacious OSD will not be compiled ***])]
        )
    else
        AC_MSG_RESULT([*** X Composite support for Audacious OSD plugin disabled per user request ***])
        have_aosd_xcomp="no"
    fi
else
    if test "x$enable_aosd" = "xyes"; then
        AC_MSG_RESULT([*** Audacious OSD plugin has been forcefully enabled ***])
        have_aosd="yes"
        if test "x$enable_aosd_xcomp" = "xyes"; then
            have_aosd_xcomp="yes"
            PKG_CHECK_MODULES(XCOMPOSITE, xcomposite,
                [AC_DEFINE([HAVE_XCOMPOSITE],[],[X Composite extension available])],
                [have_aosd_xcomp="no"
                AC_MSG_RESULT([*** X Composite extension not found, composite support for Audacious OSD will not be compiled ***])]
            )
        else
            AC_MSG_RESULT([*** X Composite support for Audacious OSD plugin disabled per user request ***])
            have_aosd_xcomp="no"
        fi
    else
        AC_MSG_RESULT([*** Audacious OSD plugin disabled per user request ***])
        have_aosd="no"
    fi
fi

if test "x$have_aosd" = "xyes"; then
    GENERAL_PLUGINS="$GENERAL_PLUGINS aosd"
fi


dnl notify: libnotify-based OSD
dnl ===========================

AC_ARG_ENABLE(notify,
 [  --disable-notify        disable notify plugin (default=enabled)],
 [enable_notify=$enableval], [enable_notify="yes"])

if test "x$enable_notify" = "xyes"; then
    PKG_CHECK_MODULES(NOTIFY, [libnotify], [have_notify="yes"], [have_notify="no"])
else
    have_notify="no"
fi

if test "x$have_notify" = "xyes"; then
    GENERAL_PLUGINS="$GENERAL_PLUGINS notify"
fi


dnl *** AdPlug requirement (libbinio)

AC_ARG_ENABLE(adplug,
    [  --disable-adplug        disable AdPlug plugin (default=enabled)],
    [enable_adplug=$enableval],
    [enable_adplug="yes"]
)

if test "x$enable_adplug" = "xyes"; then
    have_adplug="yes"
    PKG_CHECK_MODULES(BINIO,[libbinio >= 1.4],,[have_adplug=no])
else
    AC_MSG_RESULT([*** AdPlug plugin disabled per user request ***])
    have_adplug="no"
fi

if test "x$have_adplug" = "xyes"; then
	INPUT_PLUGINS="$INPUT_PLUGINS adplug"
	AC_SUBST(BINIO_CFLAGS)
fi

dnl *** Ogg Vorbis

AC_ARG_ENABLE(vorbis,
    [  --disable-vorbis        disable Ogg Vorbis input plugin (default=enabled)],
    [enable_vorbis=$enableval],
    [enable_vorbis="yes"]
)

if test "x$enable_vorbis" = "xyes"; then
    PKG_CHECK_MODULES(OGG_VORBIS, [ogg >= 1.0 vorbis >= 1.1 vorbisfile >= 1.1],
        [have_oggvorbis=yes],
        [AC_MSG_WARN([*** Cannot find libogg/libvorbis, Ogg Vorbis support will not be built ***])
         have_oggvorbis=no]
    )
else
    AC_MSG_RESULT([*** Ogg Vorbis plugin disabled per user request ***])
    have_oggvorbis=no
fi

if test "x$have_oggvorbis" = "xyes"; then
	INPUT_PLUGINS="$INPUT_PLUGINS vorbis"
fi

dnl *** FLACng

AC_ARG_ENABLE(flacng,
    [  --disable-flacng        disable flac input plugin (default=enabled)],
    [enable_flacng=$enableval],
    [enable_flacng="yes"]
)

if test "x$enable_flacng" = "xyes"; then
    PKG_CHECK_MODULES(LIBFLAC, [flac >= 1.2.1],
        [have_flacng=yes],
        [AC_MSG_WARN([*** Cannot find libFLAC 1.2.1+, FLAC support will not be built ***])
         have_flacng=no]
    )
else
    AC_MSG_RESULT([*** FLACng plugin disabled per user request ***])
    have_flacng=no
fi

if test "x$have_flacng" = "xyes"; then
	INPUT_PLUGINS="$INPUT_PLUGINS flacng"
fi

dnl *** WavPack 4.31 support

AC_ARG_ENABLE(wavpack,
    [  --disable-wavpack       disable WavPack input plugin (default=enabled)],
    [enable_wavpack=$enableval],
    [enable_wavpack="yes"]
)

if test "x$enable_wavpack" = "xyes"; then
    PKG_CHECK_MODULES(WAVPACK, [wavpack >= 4.31],
        [have_wavpack=yes],
        [AC_MSG_WARN([*** Cannot find WavPack 4.31+, WavPack support will not be built ***])
         have_wavpack=no]
    )
else
    AC_MSG_RESULT([*** WavPack plugin disabled per user request ***])
    have_wavpack=no
fi

if test "x$have_wavpack" = "xyes"; then
	INPUT_PLUGINS="$INPUT_PLUGINS wavpack"
fi

dnl *** AAC

AC_ARG_ENABLE(aac,
    [  --disable-aac           disable aac plugin (default=enabled) ],
    [enable_aac=$enableval],
    [enable_aac=yes]
)

if test x$enable_aac = xyes; then
	FAAD_LIBS="-lfaad"
	FAAD_CFLAGS=

	AC_CHECK_HEADER(faad.h,,enable_aac=no)
	if test x$enable_aac = xyes; then
		AC_CHECK_DECL(FAAD2_VERSION,,enable_aac=no,[#include <neaacdec.h>])
	fi
	if test x$enable_aac = xyes; then
		AC_CHECK_DECL(NeAACDecInit2,,enable_aac=no,[#include <neaacdec.h>])
	fi
	if test x$enable_aac = xyes; then
		AC_CHECK_LIB(faad,NeAACDecInit2,enable_aac=yes,enable_aac=no)
	fi
fi

if test x$enable_aac = xyes; then
    AC_SUBST(FAAD_CFLAGS)
    AC_SUBST(FAAD_LIBS)
    INPUT_PLUGINS="$INPUT_PLUGINS aac"
fi

dnl *** sndfile

AC_ARG_ENABLE(sndfile,
    [  --disable-sndfile       disable sndfile extensions. [default=enabled] ],
    [enable_sndfile=$enableval],
    [enable_sndfile=yes]
)

if test "x$enable_sndfile" = "xyes"; then
    PKG_CHECK_MODULES(SNDFILE, [sndfile >= 0.19],
        [enable_sndfile=yes],
        [enable_sndfile=no]
    )
else
    AC_MSG_RESULT([*** libsndfile extensions disabled per user request ***])
    enable_sndfile=no
fi

if test "x$enable_sndfile" = "xyes"; then
	INPUT_PLUGINS="$INPUT_PLUGINS sndfile"
fi

dnl *** modplug

AC_ARG_ENABLE(modplug,
    [  --disable-modplug       disable ModPlug plugin (default=enabled)],
    [enable_modplug=$enableval],
    [enable_modplug="yes"]
)

if test $enable_modplug = yes ; then
    AC_CHECK_HEADERS([sys/mman.h], [enable_modplug=yes],
     [AC_MSG_WARN([Cannot find sys/mman.h; disabling ModPlug plugin])
      enable_modplug=no])
fi

if test "x$enable_modplug" = "xyes"; then
    INPUT_PLUGINS="$INPUT_PLUGINS modplug"
    have_modplug="yes"
else
    AC_MSG_RESULT([*** ModPlug plugin disabled per user request ***])
    have_modplug="no"
fi

dnl *** FFaudio

have_ffaudio="no"
AC_ARG_ENABLE(ffaudio,
    [  --disable-ffaudio       disable ffaudio plugin (default=enabled)],
    [enable_ffaudio=$enableval],
    [enable_ffaudio="yes"]
)

if test "x$enable_ffaudio" = "xyes"; then
    PKG_CHECK_MODULES(FFMPEG, [libavcodec >= 52.20.0  libavutil >= 49.15.0  libavformat >= 52.31.0],
                      [have_ffaudio="yes"], [have_ffaudio="no"])
fi

if test "x$have_ffaudio" = "xyes"; then
    INPUT_PLUGINS="$INPUT_PLUGINS ffaudio"
fi

dnl *** jack output plugin
AC_ARG_ENABLE( jack,
[  --disable-jack          disable jack output plugin (default=enabled)], [enable_jack=$enableval],
                [enable_jack="yes"])

if test "x$enable_jack" = "xyes"; then
        AM_PATH_JACK(have_jack=yes, have_jack=no)
else
        AC_MSG_RESULT([*** jack plugin disabled per user request ***])
        have_jack=no
fi

if test "x$have_jack" = "xyes"; then
	OUTPUT_PLUGINS="$OUTPUT_PLUGINS jack"
fi

dnl *** sid
AC_ARG_ENABLE( sid,
[  --disable-sid           disable SID input plugin (default=enabled)],,
                enable_sid="yes")

if test "x$enable_sid" = "xyes"; then
        AM_PATH_SIDPLAY(have_sidplay=yes, have_sidplay=no)
else
        AC_MSG_RESULT([*** SID plugin disabled per user request ***])
        have_sidplay=no
fi

if test "x$have_sidplay" = "xyes"; then
	INPUT_PLUGINS="$INPUT_PLUGINS sid"
fi

dnl *** OSS output

AC_ARG_ENABLE(oss,
    [  --disable-oss           disable the OSS output plugin (default=enabled)],
    [enable_oss=$enableval],
    [enable_oss=yes]
)

have_oss=no
if test "x$enable_oss" = "xyes"; then
    AC_MSG_CHECKING(for OSS include dir)
    OSS_CFLAGS=""
    if test -f "/etc/oss.conf" ; then
        for i in `cat /etc/oss.conf`; do
            t=`echo $i | sed -e 's/OSSLIBDIR=//'`
            if test "x$i" != "x$t" ; then
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

    if test "x${ac_cv_header_soundcard_h}" = "xyes" || test "x${ac_cv_header_sys_soundcard_h}" = "xyes" || test "x${ac_cv_header_machine_soundcard_h}" = "xyes"; then
        have_oss=yes
    fi

    if test "x${have_oss}" = "xyes"; then
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

if test "x$have_oss" = "xyes"; then
    AC_DEFINE(HAVE_OSS, 1, [Define if the OSS output plugin should be built])
else
    have_oss=no
fi

if test "x$have_oss" = "xyes"; then
	OUTPUT_PLUGINS="$OUTPUT_PLUGINS OSS"
fi
AC_SUBST(OSS_CFLAGS)
AC_SUBST(OSS_LIBS)


dnl *** OSS4 output

AC_ARG_ENABLE(oss4,
    [  --enable-oss4           enable the OSS4 output plugin (default=disabled)],
    [enable_oss4=$enableval],
    [enable_oss4=no]
)

have_oss4=no
if test "x$enable_oss4" = "xyes"; then
    AC_MSG_CHECKING(for OSS4 include dir)
    OSS4_CFLAGS=""
    if test -f "/etc/oss.conf" ; then
        for i in `cat /etc/oss.conf`; do
            t=`echo $i | sed -e 's/OSSLIBDIR=//'`
            if test "x$i" != "x$t" ; then
                if test -f "$t/include/sys/soundcard.h" -o -f "$i/include/soundcard.h" ; then
                    OSS4_CFLAGS="-I$t/include"
                fi
            fi
        done
    fi
    if test -n "$OSS4_CFLAGS" ; then
        AC_MSG_RESULT([$OSS4_CFLAGS])
    else
        AC_MSG_RESULT([not found])
    fi
    CFLAGS_save=$CFLAGS
    CFLAGS="$CFLAGS $OSS4_CFLAGS"
    AC_CHECK_HEADERS(soundcard.h)
    AC_CHECK_HEADERS(sys/soundcard.h)
    AC_CHECK_HEADERS(machine/soundcard.h)
    CFLAGS=$CFLAGS_save

    if test "x${ac_cv_header_soundcard_h}" = "xyes" || test "x${ac_cv_header_sys_soundcard_h}" = "xyes" || test "x${ac_cv_header_machine_soundcard_h}" = "xyes"; then
        have_oss4=yes
    fi

    if test "x${have_oss4}" = "xyes"; then
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

if test "x$have_oss4" = "xyes"; then
    AC_DEFINE(HAVE_OSS4, 1, [Define if the OSS4 output plugin should be built])
else
    have_oss4=no
fi

if test "x$have_oss4" = "xyes"; then
	OUTPUT_PLUGINS="$OUTPUT_PLUGINS oss4"
fi

AC_SUBST(OSS_LIBS)

dnl *** ALSA output plugin

AC_ARG_ENABLE(alsa,
    [  --disable-alsa          disable ALSA output plugin],
    [enable_alsa=$enableval],
    [enable_alsa=yes]
)

if test "x$enable_alsa" = "xyes"; then
    PKG_CHECK_MODULES([ALSA], [alsa >= 1.0], [have_alsa=yes], [have_alsa=no])
else
    AC_MSG_RESULT([*** ALSA output plugin disabled per user request ***])
    have_alsa=no
fi

if test "x$have_alsa" = "xyes"; then
	OUTPUT_PLUGINS="$OUTPUT_PLUGINS alsa"
fi

dnl SDL Output
dnl ==========

AC_ARG_ENABLE(sdlout,
 [  --disable-sdlout        disable SDL output plugin],
 [enable_sdlout=$enableval], [enable_sdlout=yes])

if test $enable_sdlout = yes ; then
    AM_PATH_SDL(1.2.5, [enable_sdlout=yes], [enable_sdlout=no])
fi

if test $enable_sdlout = yes ; then
	OUTPUT_PLUGINS="$OUTPUT_PLUGINS sdlout"
fi


dnl *** amidi-plug (note: to avoid checking twice ALSA, this should appear somewhere after the alsa output plugin check)

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

if test "x$enable_amidiplug" = "xno"; then
  AC_MSG_RESULT([*** amidi-plug disabled per user request ***])
  enable_amidiplug=no
else
  if test "x$enable_amidiplug_alsa" != "xauto"; then
    enable_amidiplug=yes
  elif test "x$enable_amidiplug_fluidsynth" != "xauto"; then
    enable_amidiplug=yes
  fi
fi

if test "x$have_alsa" = "xyes"; then
  AUD_TRY_COMPILE([ALSA MIDI support], [alsalib_cv_hasmidi],
[#include <alsa/asoundlib.h>],
[snd_seq_t *seq = NULL;],
  [alsalib_midi=yes], [alsalib_midi=no])
fi


if test "x$enable_amidiplug" = "xcheck"; then
  if test "x$alsalib_midi" = "xyes"; then
    enable_amidiplug=yes
  else
    PKG_CHECK_MODULES(FLUIDSYNTH, [fluidsynth >= 1.0.6],
      [fluidsynth_available=yes],[fluidsynth_available=no])
    if test "x$fluidsynth_available" = "xyes"; then
      enable_amidiplug=yes
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
  AMIDIPLUG_BACKEND_DIR=amidi-plug
  AC_SUBST(AMIDIPLUG_BACKEND_DIR)
  if test "x$enable_amidiplug_alsa" = "xauto" -o "x$enable_amidiplug_alsa" = "xyes"; then
    if test "x$alsalib_midi" = "xyes"; then
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
  AC_SUBST(AMIDIPLUG_BACKENDS)
fi

AC_CHECK_HEADERS(unistd.h)
AC_CHECK_HEADERS(fts.h)

dnl CD-Audio New Generation

AC_ARG_ENABLE(cdaudio,
    [  --disable-cdaudio       disable cdaudio-ng input plugin (default=enabled) ],
    [enable_cdaudio_ng=$enableval],
    [enable_cdaudio_ng=yes]
)

if test "$enable_cdaudio_ng" = "yes"; then
   PKG_CHECK_MODULES(CDIO, [libcdio >= 0.70],
   ,
   [AC_MSG_WARN([*** Cannot find libcdio 0.70 or newer, cdaudio-ng will not be built ***])
   enable_cdaudio_ng=no]
)
fi

if test "$enable_cdaudio_ng" = "yes"; then
   PKG_CHECK_MODULES(CDIO, [libcdio_cdda >= 0.70],
   [CDIO_LIBS=`pkg-config --libs libcdio_cdda`
   CDIO_CFLAGS=`pkg-config --cflags libcdio_cdda`
   AC_SUBST(CDIO_LIBS)
   AC_SUBST(CDIO_CFLAGS)],
   [AC_MSG_WARN([*** Cannot find libcdio_cdda 0.70 or newer, cdaudio-ng will not be built ***])
   enable_cdaudio_ng=no]
)
fi

if test "$enable_cdaudio_ng" = "yes"; then
   PKG_CHECK_MODULES(CDDB, [libcddb >= 1.2.1],
   [INPUT_PLUGINS="$INPUT_PLUGINS cdaudio-ng"
   GENERAL_PLUGINS="$GENERAL_PLUGINS cd-menu-items"
   CDDB_LIBS=`pkg-config --libs libcddb`
   CDDB_CFLAGS=`pkg-config --cflags libcddb`
   AC_SUBST(CDDB_LIBS)
   AC_SUBST(CDDB_CFLAGS)],
   [AC_MSG_WARN([*** Cannot find libcddb 1.2.1 or newer, cdaudio-ng will not be built ***])
   enable_cdaudio_ng=no]
)
fi

dnl Streambrowser
dnl =============

AC_ARG_ENABLE(streambrowser,
    [  --disable-streambrowser       disable streambrowser general plugin (default=enabled) ],
    [enable_streambrowser=$enableval],
    [enable_streambrowser=yes]
)

if test $enable_streambrowser = yes ; then
    AC_CHECK_FUNCS([strtok_r], [enable_streambrowser=yes],
     [AC_MSG_WARN([Cannot find strtok_r; disabling streambrowser plugin])
      enable_streambrowser=no])
fi

if test "$enable_streambrowser" = "yes"; then
   GENERAL_PLUGINS="$GENERAL_PLUGINS streambrowser"
fi

AM_CONDITIONAL(HAVE_CDROM, test "${ac_cv_header_linux_cdrom_h}" = "yes" || test "${ac_cv_header_sys_cdio_h}" = "yes")
AM_CONDITIONAL(HAVE_LINUX_JOYSTICK,test "${ac_cv_header_linux_joystick_h}" = "yes")

CPPFLAGS="$CPPFLAGS $AUDACIOUS_CFLAGS $DBUS_CFLAGS"

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

dnl *** scrobbler ***

AC_ARG_ENABLE(scrobbler,
 [  --disable-scrobbler     disable Scrobbler plugin (default=enabled)],
 [have_scrobbler=$enableval], [have_scrobbler="yes"])

if test "x$have_scrobbler" = "xyes"; then
    PKG_CHECK_MODULES(CURL, [libcurl >= 7.9.7],
     [have_scrobbler="$have_scrobbler"], [have_scrobbler="no"])
    AC_SUBST(CURL_CFLAGS)
    AC_SUBST(CURL_LIBS)
fi

if test "x$have_scrobbler" = "xyes"; then
	GENERAL_PLUGINS="$GENERAL_PLUGINS scrobbler"
fi

dnl *** neon http plugin ***

AC_ARG_ENABLE(neon,
[  --disable-neon          disable neon HTTP support (default=enabled)],
[have_neon=$enableval],
[have_neon=yes])

if test "x$have_neon" = "xyes"; then
    PKG_CHECK_MODULES(NEON, [neon >= 0.26], [TRANSPORT_PLUGINS="$TRANSPORT_PLUGINS neon" ], [have_neon="no"])
    AC_CHECK_LIB([neon], [ne_set_connect_timeout], [AC_DEFINE(HAVE_NE_SET_CONNECT_TIMEOUT, 1, [Whether we have ne_set_connect_timeout])], [], [$NEON_LIBS])
else
    have_neon="no"
fi
AC_SUBST([NEON_LIBS])
AC_SUBST([NEON_CFLAGS])


dnl *** MMS

AC_ARG_ENABLE(mms,
[  --disable-mms           disable mms support. (default=enabled)],
[have_mms=$enableval],
[have_mms=yes])

if test "x$have_mms" = "xyes"; then
    have_mms=yes
    PKG_CHECK_MODULES(MMS, [libmms >= 0.3], [TRANSPORT_PLUGINS="$TRANSPORT_PLUGINS mms" ], [have_mms="no"])
else
    have_mms=no
    AC_MSG_RESULT([*** mms plugin disabled by request ***])
fi


dnl *** MTP Upload

AC_ARG_ENABLE(mtp_up,
[  --disable-mtp_up           disable mtp upload support. (default=enabled)],
[have_mtp_up=$enableval],
[have_mtp_up=yes])

if test "x$have_mtp_up" = "xyes"; then
    have_mtp_up=yes
    PKG_CHECK_MODULES(MTP, [libmtp >= 0.1.3], [GENERAL_PLUGINS="$GENERAL_PLUGINS mtp_up" ], [have_mtp_up="no"])
    MTP_LIBS=`pkg-config --libs libmtp`
    MTP_CFLAGS=`pkg-config --cflags libmtp`
    dnl Check for libmtp 0.3.0+
    PKG_CHECK_EXISTS(libmtp >= 0.3.0, libmtp_030_pkg=yes, libmtp_030_pkg=no)
    if test x"$libmtp_030_pkg" = "xyes" ; then
       AC_DEFINE([HAVE_LIBMTP_030], 1, [indidaces if libmtp 0.3.0 is installed])
    fi
else
    have_mtp_up=no
    AC_MSG_RESULT([*** mtp upload plugin disabled by request ***])
fi
AC_SUBST(MTP_LIBS)
AC_SUBST(MTP_CFLAGS)

dnl *** Bluetooth audio support

AC_ARG_ENABLE(bluetooth,
[  --disable-bluetooth           disable bluetooth audio support. (default=enabled)],
[have_bluetooth=$enableval],
[have_bluetooth=yes])

dnl This has been broken by the output plugin on-demand work
have_bluetooth=no
if test "x$have_bluetooth" = "xyes" -a "x$have_alsa" = "xyes"; then
    have_bluetooth=yes
    PKG_CHECK_MODULES(BLUEZ, [bluez >= 2.22],
    [GENERAL_PLUGINS="$GENERAL_PLUGINS bluetooth" ], [have_bluetooth="no"])
    BLUEZ_LIBS=`pkg-config --libs bluez`
    BLUEZ_CFLAGS=`pkg-config --cflags bluez`
else
    have_bluetooth=no
    AC_MSG_RESULT([*** bluetooth audio support plugin disabled by request ***])
fi
AC_SUBST(BLUEZ_LIBS)
AC_SUBST(BLUEZ_CFLAGS)

dnl *** libparanormal checks

AC_ARG_ENABLE(paranormal,
[  --disable-paranormal    disable paranormal vis plugin. (default=enabled)],
[have_paranormal=$enableval],
[have_paranormal=yes])

if test "x$have_paranormal" = "xyes"; then
    have_paranormal=yes
    AM_PATH_SDL(1.2.5, , [
	AC_MSG_WARN([*** SDL is not installed. Paranormal will not be built. ***])
	have_paranormal="no"
    ])
else
    have_paranormal=no
    AC_MSG_RESULT([*** paranormal plugin disabled by request ***])
fi

if test "x$have_paranormal" = "xyes"; then
	VISUALIZATION_PLUGINS="$VISUALIZATION_PLUGINS paranormal"
fi


dnl XSPF
dnl ----

PKG_CHECK_MODULES(XML, [libxml-2.0], [have_xml=yes], [have_xml=no])

if test $have_xml = "no" ; then
    AC_MSG_ERROR([Please install libxml-2.0.])
fi

CONTAINER_PLUGINS="$CONTAINER_PLUGINS xspf"


dnl CUE
dnl ---

AC_ARG_ENABLE(cue,
[  --disable-cue           disable cue support. (default=enabled)],
[have_cue=$enableval],
[have_cue=yes])

if test "x$have_cue" = "xyes"; then
    have_cue=yes
    PKG_CHECK_MODULES(CUE, [libcue], [CONTAINER_PLUGINS="$CONTAINER_PLUGINS cue" ], [have_cue="no"])
else
    have_cue=no
    AC_MSG_RESULT([*** cue plugin disabled by request ***])
fi

dnl *** projectM (GL Milkdrop port)

AC_ARG_ENABLE(projectm,
[  --disable-projectm      disable projectM vis plugin. (default=enabled)],
[have_projectm=$enableval],
[have_projectm=yes])

if test "x$have_projectm" = "xyes"; then
	PKG_CHECK_MODULES(LIBPROJECTM, libprojectM < 1.0, [have_projectm="yes"], [have_projectm="no"])
	AM_PATH_SDL(1.2.5, , [
		AC_MSG_WARN([*** SDL is not installed. projectM will not be built. ***])
		have_projectm="no"
	])
	if test "x$libprjM" = "xno"; then
	        AC_MSG_WARN([*** libprojectM not found.
	                        libprojectM is needed to build this package.
	                        You can download libprojectM at http://projectm.sf.net/])
	fi
	LIBPROJECTM_datadir=`pkg-config --variable=pkgdatadir libprojectM`;
	LIBPROJECTM_sysconfdir=`pkg-config --variable=sysconfdir libprojectM`;
	LIBPROJECTM_CFLAGS="$LIBPROJECTM_CFLAGS -DPROJECTM_DATADIR=\\\"$LIBPROJECTM_datadir\\\" -DPROJECTM_SYSCONFDIR=\\\"$LIBPROJECTM_sysconfdir\\\"";
fi
AC_SUBST(LIBPROJECTM_LIBS)
AC_SUBST(LIBPROJECTM_CFLAGS)

if test "x$have_projectm" = "xyes"; then
	VISUALIZATION_PLUGINS="$VISUALIZATION_PLUGINS projectm"
fi

dnl *** projectM (GL Milkdrop port)

AC_ARG_ENABLE(projectm-1.0,
[  --disable-projectm-1.0      disable projectM 1.0+ vis plugin. (default=enabled)],
[have_projectm1=$enableval],
[have_projectm1=yes])

if test "$have_projectm1" = "yes"; then
	PKG_CHECK_MODULES(LIBPROJECTM1, [libprojectM >= 1.1], [have_projectm1="yes"], [have_projectm1="no"])
fi

if test "$have_projectm1" = "yes"; then
	PKG_CHECK_MODULES(GTKGLEXT, [gtkglext-1.0], [have_projectm1="yes"], [have_projectm1="no"])
fi

if test "$have_projectm1" = "yes"; then
	LIBPROJECTM1_datadir=`pkg-config --variable=pkgdatadir libprojectM`;
	LIBPROJECTM1_sysconfdir=`pkg-config --variable=sysconfdir libprojectM`;
        LIBPROJECTM1_prefix=`pkg-config --variable=prefix libprojectM`;
	LIBPROJECTM1_CFLAGS="$LIBPROJECTM1_CFLAGS -DPROJECTM_DATADIR=\\\"$LIBPROJECTM1_datadir\\\" -DPROJECTM_SYSCONFDIR=\\\"$LIBPROJECTM1_sysconfdir\\\" -DPROJECTM_PREFIX=\\\"$LIBPROJECTM1_prefix\\\"";
fi

AC_SUBST(LIBPROJECTM1_LIBS)
AC_SUBST(LIBPROJECTM1_CFLAGS)
AC_SUBST(GTKGLEXT_LIBS)
AC_SUBST(GTKGLEXT_CFLAGS)

if test "x$have_projectm1" = "xyes"; then
	VISUALIZATION_PLUGINS="$VISUALIZATION_PLUGINS projectm-1.0"
fi

dnl *** FileWriter

AC_ARG_ENABLE(filewriter,
    [  --disable-filewriter    disable FileWriter output plugin (default=enabled) ],
    [enable_filewriter=$enableval],
    [enable_filewriter=yes]
)

if test "x$enable_filewriter" = "xyes"; then
	OUTPUT_PLUGINS="$OUTPUT_PLUGINS filewriter"
fi


AC_ARG_ENABLE(filewriter_mp3,
    [  --disable-filewriter_mp3    disable FileWriter MP3 output part (default=enabled) ],
    [enable_filewriter_mp3=$enableval], [enable_filewriter_mp3=yes]
)

if test "x$enable_filewriter_mp3" = "xyes"; then
   AC_CHECK_LIB(mp3lame -lm, lame_get_id3v2_tag, [have_lame=yes], [have_lame=no])
   if test "x$have_lame" = "xyes"; then
      AC_DEFINE(FILEWRITER_MP3, 1, [Define if MP3 output part should be built])
	  FILEWRITER_LIBS="$FILEWRITER_LIBS -lmp3lame"
   fi
fi


AC_ARG_ENABLE(filewriter_vorbis,
    [  --disable-filewriter_vorbis disable FileWriter Vorbis output part (default=enabled) ],
    [enable_filewriter_vorbis=$enableval], [enable_filewriter_vorbis=yes]
)

if test "x$enable_filewriter_vorbis" = "xyes"; then
   PKG_CHECK_MODULES(OGG_VORBIS, [ogg >= 1.0 vorbis >= 1.0 vorbisfile >= 1.0],
   [have_vorbisenc=yes], [have_vorbisenc=no])
   if test "x$have_vorbisenc" = "xyes"; then
      AC_DEFINE(FILEWRITER_VORBIS, 1, [Define if Vorbis output part should be built])
      FILEWRITER_CFLAGS="$FILEWRITER_CFLAGS  `pkg-config --cflags vorbisenc`"
      FILEWRITER_LIBS="$FILEWRITER_LIBS  `pkg-config --libs vorbisenc`"
   fi
fi


AC_ARG_ENABLE(filewriter_flac,
    [  --disable-filewriter_flac   disable FileWriter FLAC output part (default=enabled) ],
    [enable_filewriter_flac=$enableval], [enable_filewriter_flac=yes]
)

if test "x$enable_filewriter_flac" = "xyes"; then
   PKG_CHECK_MODULES(FLAC, [flac >= 1.1.2],
      [have_writer_flac=yes], [have_writer_flac=no]
   )
   if test "x$have_writer_flac" = "xyes"; then
      AC_DEFINE(FILEWRITER_FLAC, 1, [Define if FLAC output part should be built])
	  FILEWRITER_CFLAGS="$FILEWRITER_CFLAGS  `pkg-config --cflags flac`"
	  FILEWRITER_LIBS="$FILEWRITER_LIBS  `pkg-config --libs flac`"
   fi
fi

if test "x$enable_filewriter" != "xyes"; then
   have_lame=no
   have_vorbisenc=no
   have_writer_flac=no
   FILEWRITER_LIBS=""
fi
AC_SUBST(FILEWRITER_CFLAGS)
AC_SUBST(FILEWRITER_LIBS)

dnl *** BS2B effect plugin

AC_ARG_ENABLE(bs2b,
    [  --disable-bs2b          disable BS2B effect plugin (default=enabled)],
    [enable_bs2b=$enableval],
    [enable_bs2b=yes]
)

if test "x$enable_bs2b" = "xyes"; then
    PKG_CHECK_MODULES([BS2B], [libbs2b >= 3.0.0], [have_bs2b=yes], [have_bs2b=no])
else
    AC_MSG_RESULT([*** BS2B effect plugin disabled per user request ***])
    have_bs2b=no
fi

if test "x$have_bs2b" = "xyes"; then
	EFFECT_PLUGINS="$EFFECT_PLUGINS bs2b"
fi


dnl *** Resample effect plugin

AC_ARG_ENABLE(resample,
 [  --disable-resample      disable resample effect plugin (default=enabled)],
 [enable_resample=$enableval], [enable_resample=yes]
)

if test "x$enable_resample" = "xyes"; then
    AC_CHECK_LIB([samplerate], [src_process], [enable_resample=yes],
     [enable_resample=no])
fi

if test "x$enable_resample" = "xyes"; then
	EFFECT_PLUGINS="$EFFECT_PLUGINS resample"
fi

dnl GTK Interface
dnl =============

AC_ARG_ENABLE(gtkui,
 [  --disable-gtkui         disable GTK interface (gtkui)],
 [enable_gtkui=$enableval], [enable_gtkui="yes"])

if test $enable_gtkui = yes ; then
    GENERAL_PLUGINS="$GENERAL_PLUGINS gtkui"
fi

dnl Winamp Classic Interface
dnl =============

AC_ARG_ENABLE(skins,
 [  --disable-skins         disable Winamp Classic interface (skins)],
 [enable_skins=$enableval], [enable_skins="yes"])

if test $enable_skins = yes ; then
    GENERAL_PLUGINS="$GENERAL_PLUGINS skins"
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
TRANSPORT_PLUGIN_DIR=Transport

AC_SUBST(INPUT_PLUGIN_DIR)
AC_SUBST(OUTPUT_PLUGIN_DIR)
AC_SUBST(EFFECT_PLUGIN_DIR)
AC_SUBST(GENERAL_PLUGIN_DIR)
AC_SUBST(VISUALIZATION_PLUGIN_DIR)
AC_SUBST(CONTAINER_PLUGIN_DIR)
AC_SUBST(TRANSPORT_PLUGIN_DIR)
AC_SUBST(pluginsubs)

dnl XXX Work around some autoconf bugs.
if test "x$prefix" = "xNONE"; then
        prefix="${ac_default_prefix}"
fi

if test -z "$datadir"; then
	datadir="${prefix}/share"
fi

localedir="$datadir/locale"
AC_SUBST(localedir)

CFLAGS="$CFLAGS $EXTRA_CFLAGS"

if test "x$enable_user_plugin_dir" = "xno"; then
    AC_DEFINE(DISABLE_USER_PLUGIN_DIR,, [Define to disable per user plugin directory])
fi

AC_SUBST(plugindir)

AC_SUBST(EFFECT_PLUGINS)
AC_SUBST(GENERAL_PLUGINS)
AC_SUBST(INPUT_PLUGINS)
AC_SUBST(OUTPUT_PLUGINS)
AC_SUBST(VISUALIZATION_PLUGINS)
AC_SUBST(CONTAINER_PLUGINS)
AC_SUBST(TRANSPORT_PLUGINS)
AC_SUBST(GCC42_CFLAGS)

AC_CONFIG_FILES([
	buildsys.mk
	extra.mk
])

BUILDSYS_TOUCH_DEPS
AC_OUTPUT

echo
echo "Configuration:"
echo
echo "  Install path:                           ${audacious_plugin_root}"
echo
echo "  Output Plugins"
echo "  --------------"
echo "  Open Sound System (oss):                $have_oss"
echo "  Open Sound System v4 (oss4):            $have_oss4"
echo "  Advanced Linux Sound Arch. (alsa):      $have_alsa"
echo "  PulseAudio (pulse):                     $have_pulse"
echo "  RoarAudio (roaraudio):                  $have_roar"
echo "  Jack Audio Connection Kit (jack):       $have_jack"
echo "  BSD/SUN audio output (sun):             $have_sun"
echo "  Mac OS X sound support (CoreAudio):     $have_coreaudio"
echo "  Simple DirectMedia Layer (sdlout):      $enable_sdlout"
echo "  FileWriter:                             $enable_filewriter"
echo "    -> FileWriter MP3 output part:        $have_lame"
echo "    -> FileWriter Vorbis output part:     $have_vorbisenc"
echo "    -> FileWriter FLAC output part:       $have_writer_flac"
echo "  Null Audio output (null):               yes"
echo
echo "  Input Plugins"
echo "  -------------"
echo "  MPEG-1 Layer I/II/III (mpg123):         $enable_mp3"
echo "  MPEG-2/4 AAC (aac):                     $enable_aac"
echo "  FFaudio (ffaudio):                      $have_ffaudio"
echo "  Module decoder (modplug):               $have_modplug"
echo "  MIDI modular plugin (amidi-plug):       $enable_amidiplug"
echo "    -> ALSA backend:                      $enable_amidiplug_alsa"
echo "    -> FluidSynth backend:                $enable_amidiplug_fluidsynth"
echo "  CD Digital Audio (cdaudio_ng):          $enable_cdaudio_ng"
echo "  sndfile extensions:                     $enable_sndfile"
echo "  Tone Generator:                         yes"
echo "  Ogg Vorbis (vorbis):                    $have_oggvorbis"
echo "  Free Lossless Audio Codec (flacng):     $have_flacng"
echo "  Commodore 64 audio (SID):               $have_sidplay $xs_have_sidplay2_api"
echo "    -> libSIDPlay1 support:               $xs_have_sidplay1"
echo "    -> libSIDPlay2 support:               $xs_have_sidplay2"
echo "    -> distortion patched libSIDPlay2:    $xs_have_distortion"
echo "  Game music (spc, nsf & gbs):            $enable_console"
echo "  PlayStation (psf/psf2) audio:           $enable_psf"
echo "  Nintendo 64 audio (usf):                $enable_usf"
echo "  Nintendo DS audio (xsf):                $enable_xsf"
echo "  AdLib synthesizer (adplug):             $have_adplug"
echo "  WavPack 4.31+ (wavpack):                $have_wavpack"
echo "  Metronom:                               yes"
echo
echo "  General"
echo "  -------"
echo "  Alarm:                                  yes"
echo "  Song Change:                            $enable_songchange"
echo "  Status Icon:                            $have_statusicon"
echo "  Audacious OSD:                          $have_aosd"
echo "    -> X Composite support:               $have_aosd_xcomp"
echo "  libnotify OSD:                          $have_notify"
echo "  Control via event device (evdev-plug):  $have_evdevplug"
echo "  Global Hotkey Plugin:                   $have_hotkey"
echo "  Gnome Shortcuts Plugin:                 $have_gnomeshortcuts"
echo "  LIRC:                                   $have_lirc"
echo "  AudioScrobbler Client:                  $have_scrobbler"
echo "  Upload to MTP device:                   $have_mtp_up"
echo "  Bluetooth audio support:                $have_bluetooth"
echo "  MacOS Dock Album Art plugin:            $have_dockalbumart"
echo "  Streambrowser:                          $enable_streambrowser"
echo "  LyricWiki viewer:                       yes"
echo
echo "  Effect"
echo "  ------"
echo "  Dynamic Range Compressor:               yes"
echo "  LADSPA effects host (ladspa):           yes"
echo "  Voice Removal:                          yes"
echo "  Extra Stereo:                           yes"
echo "  Echo/Surround:                          yes"
echo "  SndStretch:                             yes"
echo "  Crystalizer:                            yes"
echo "  Bauer stereophonic-to-binaural (bs2b):  $have_bs2b"
echo "  Sample Rate Converter (resample):       $enable_resample"
echo
echo "  Visualization"
echo "  -------------"
echo "  Blur Scope:                             yes"
echo "  Spectrum Analyzer:                      yes"
echo "  Paranormal Visualization Library:       $have_paranormal"
echo "  projectM 0.x (GL milkdrop):             $have_projectm"
echo "  projectM 1.x (GL milkdrop):             $have_projectm1"
echo
echo "  Transport"
echo "  ---------"
echo "  gio transport:                          $enable_gio"
echo "  neon-based http/https:                  $have_neon"
echo "  libmms-based mms:                       $have_mms"
echo
echo "  Container"
echo "  ---------"
echo "  Winamp PLS playlist format (pls):       yes"
echo "  M3U playlist format (m3u):              yes"
echo "  XML Sharable Playlist Format (xspf):    yes"
echo "  CUE playlist format (cue):              $have_cue"
echo
echo "  Interfaces"
echo "  ----------"
echo "  GTK (gtkui):                            $enable_gtkui"
echo "  Winamp Classic (skins):                 $enable_skins"
echo
