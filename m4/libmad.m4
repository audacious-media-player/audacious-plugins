dnl Configure paths for libmad
dnl derived from libFLAC.m4

dnl AM_PATH_LIBMAD([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for libmad, and define MAD_CFLAGS and MAD_LIBS
dnl
AC_DEFUN([AM_PATH_LIBMAD],
[dnl 
dnl Get the cflags and libraries
dnl
AC_ARG_WITH([libmad],
[AS_HELP_STRING([--with-libmad=PFX], [Prefix where libmad is installed])],
[libmad_prefix="$withval"], [libmad_prefix=""])

AC_ARG_WITH([libmad-libraries],
[AS_HELP_STRING([--with-libmad-libraries=DIR], [Directory where libmad library is installed])],
[libmad_libraries="$withval"], [libmad_libraries=""])

AC_ARG_WITH([libmad-includes],
[AS_HELP_STRING([--with-libmad-includes=DIR], [Directory where libmad header files are installed])],
[libmad_includes="$withval"], [libmad_includes=""])

AC_ARG_ENABLE([libmadtest],
[AS_HELP_STRING([--disable-libmadtest], [Do not try to compile and run a test libmad program])],,
[enable_libmadtest="yes"])

  if test "x$libmad_libraries" != "x" ; then
    MAD_LIBS="-L$libmad_libraries"
  elif test "x$libmad_prefix" != "x" ; then
    MAD_LIBS="-L$libmad_prefix/lib"
  elif test "x$prefix" != "xNONE" ; then
    MAD_LIBS="-L$libdir"
  fi

  MAD_LIBS="$MAD_LIBS -lmad -lm"

  if test "x$libmad_includes" != "x" ; then
    MAD_CFLAGS="-I$libmad_includes"
  elif test "x$libmad_prefix" != "x" ; then
    MAD_CFLAGS="-I$libmad_prefix/include"
  elif test "x$prefix" != "xNONE"; then
    MAD_CFLAGS="-I$prefix/include"
  fi

  no_libmad=""


  if test "x$enable_libmadtest" = "xyes" ; then
    ac_save_CFLAGS="$CFLAGS"
    ac_save_CXXFLAGS="$CXXFLAGS"
    ac_save_LIBS="$LIBS"
    CFLAGS="$CFLAGS $MAD_CFLAGS"
    CXXFLAGS="$CXXFLAGS $MAD_CFLAGS"
    LIBS="$LIBS $MAD_LIBS"
dnl
dnl Now check if the installed libmad is sufficiently new.
dnl
      AC_MSG_CHECKING([for libmad])
      rm -f conf.libmadtest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mad.h>

int main ()
{
  mad_timer_t t;
  mad_timer_add(&t, t);
  system("touch conf.libmadtest");
  return 0;
}

],, [no_libmad="yes"],[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
  fi

  if test "x$no_libmad" = "x" ; then
     AC_MSG_RESULT([yes])
     ifelse([$1], , :, [$1])     
  else
     AC_MSG_RESULT([no])
     if test -f conf.libmadtest ; then
       :
     else
       echo "*** Could not run libmad test program, checking why..."
       CFLAGS="$CFLAGS $MAD_CFLAGS"
       LIBS="$LIBS $MAD_LIBS"
       AC_TRY_LINK([
#include <stdio.h>
],     [ return 0; ],
       [ echo "*** The test program compiled, but did not run. This usually means"
       echo "*** that the run-time linker is not finding libmad or finding the wrong"
       echo "*** version of libmad. If it is not finding libmad, you'll need to set your"
       echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
       echo "*** to the installed location  Also, make sure you have run ldconfig if that"
       echo "*** is required on your system"
       echo "***"
       echo "*** If you have an old version installed, it is best to remove it, although"
       echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
       [ echo "*** The test program failed to compile or link. See the file config.log for the"
       echo "*** exact error that occured. This usually means libmad was incorrectly installed"
       echo "*** or that you have moved libmad since it was installed. In the latter case, you"
       echo "*** may want to edit the libmad-config script: $LIBMAD_CONFIG" ])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
     MAD_CFLAGS=""
     MAD_LIBS=""
     ifelse([$2], , :, [$2])
  fi
  AC_SUBST([MAD_CFLAGS])
  AC_SUBST([MAD_LIBS])
  rm -f conf.libmadtest
])
