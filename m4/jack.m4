# Configure paths for JACK

dnl AM_PATH_JACK([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for JACK, and define JACK_CFLAGS and JACK_LIBS
dnl
AC_DEFUN([AM_PATH_JACK],
[dnl 
dnl *** Check for JACK libraries
dnl
PKG_CHECK_MODULES([JACK], [jack >= 0.100.0], [have_jack=yes], [have_jack=no])

dnl *** Check for libsamplerate necessary for bio2jack
PKG_CHECK_MODULES([SAMPLERATE], [samplerate >= 0.0.15],
    [ac_cv_samplerate=1], [ac_cv_samplerate=0])

AC_DEFINE_UNQUOTED([HAVE_SAMPLERATE], ${ac_cv_samplerate},
    [Set to 1 if you have libsamplerate.])

dnl *** Make sure libsamplerate is found, we can't compile without it
if test "x${ac_cv_samplerate}" = "x0"; then
    AC_MSG_WARN([Could not find libsamplerate, necessary for jack output plugin.])
    have_jack=no
fi

dnl AC_SUBST(SAMPLERATE_CFLAGS)
dnl AC_SUBST(SAMPLERATE_LIBS)

if test "x$have_jack" = "xno"; then
  ifelse([$2], , :, [$2])
else
  ifelse([$1], , :, [$1])
fi

])
