dnl *** These m4-macros are originally based on XSIDPLAY v1.6.5's acinclude.m4
dnl *** (C) Copyright 2002 Michael Schwendt, licensed under GNU GPL v2.
dnl ***
dnl *** Updated for new autotools, plus namespace, added portability
dnl *** fixes and other minor changes by Matti 'ccr' Hamalainen
dnl *** (C) Copyright 2003-2008 Tecnic Software productions (TNSP)

AC_DEFUN([XS_FIND_FILE],
[
    $1=""
    for i in $3; do
        for j in $2; do
            if test -r "$i/$j"; then
                $1="$i"
                break 2
            fi
        done
    done
])


AC_DEFUN([XS_TRY_LIBRARY],
[
# xs_try_library ( $1:$2:$3:$4:$5:$6 )
    AC_LANG_ASSERT([C++])
    m4_ifvaln([$2], [xs_cxxflags_save="$CXXFLAGS"
    CXXFLAGS="$CXXFLAGS $2"])dnl
    m4_ifvaln([$3], [xs_ldflags_save="$LDFLAGS"
    LDFLAGS="$LDFLAGS $3"])dnl
    m4_ifvaln([$4], [xs_libs_save="$LIBS"
    LIBS="$LIBS -l$4"])dnl
    AC_LINK_IFELSE(
        [
        AC_LANG_PROGRAM(
            [#include <$5>],
            [$6]
        )],
        [xs_$1_works="yes"],
        [xs_$1_works="no"]
    )
    m4_ifvaln([$2], [CXXFLAGS="$xs_cxxflags_save"])dnl
    m4_ifvaln([$3], [LDFLAGS="$xs_ldflags_save"])dnl
    m4_ifvaln([$4], [LIBS="$xs_libs_save"])dnl
])


AC_DEFUN([XS_LIB_RESULT],
[
# xs_lib_result ( $1:$2:$3 )
    define([Name], [translit([$1], [a-z], [A-Z])])dnl
    if test "x$xs_have_$1" = "xyes"; then
        if test "x$xs_$1_cxxflags" != "x" || test "x$xs_$1_ldflags" != "x"; then
            AC_MSG_RESULT([$xs_$1_cxxflags $2 $xs_$1_ldflags $3])
        else
            AC_MSG_RESULT([yes])
        fi

        Name[]_LIBS="$xs_$1_ldflags $2"
        Name[]_CFLAGS="$xs_$1_cxxflags $3"
        AC_SUBST(Name[]_LIBS)dnl
        AC_SUBST(Name[]_CFLAGS)dnl
    else
        AC_MSG_RESULT([no])
    fi
])


AC_DEFUN([XS_PROG_PKGCONFIG],
[
    AC_PATH_PROG([PKG_CONFIG], [pkg-config])dnl
])


AC_DEFUN([XS_ARG_WITH], [dnl
# xs_arg_with ( $1:$2:$3 )
    define([Name], [translit([$1], [a-z], [A-Z])])dnl
    define([roflCopter], [Enable $2 with install-PREFIX])dnl
    AH_TEMPLATE(HAVE_[]Name, [Define if you have and want to use $2])dnl
    AC_ARG_WITH([$1],
        [AS_HELP_STRING([--with-$1=PREFIX], roflCopter)], [dnl
        if test "x$withval" = "xyes"; then
            xs_use_$1="yes"
            xs_$1_library=""
            xs_$1_includes=""
        else
            if test "x$withval" = "xno"; then
                xs_use_$1="no"
            else
                xs_use_$1="yes"
                xs_$1_includes="$withval/include"
                xs_$1_library="$withval/lib"
            fi
        fi
    ],[dnl
        xs_use_$1="try"
        xs_$1_library=""
        xs_$1_includes=""
    ])dnl
    define([roflCopter], [Where the $2 headers are located])dnl
    AC_ARG_WITH([$1-inc],
    [AS_HELP_STRING([--with-$1-inc=DIR], roflCopter)],
    [xs_$1_includes="$withval"])dnl
    define([roflCopter], [Where the $2 library is installed])dnl
    AC_ARG_WITH([$1-lib],
    [AS_HELP_STRING([--with-$1-lib=DIR], roflCopter)],
    [xs_$1_library="$withval"])dnl

    dnl Okay, handle this
    xs_have_$1="no"
    if test "x$xs_use_$1" = "xtry"; then
        $3
    elif test "x$xs_use_$1" = "xyes"; then
        $3
        if test "x$xs_have_$1" = "xno"; then
            AC_MSG_ERROR([$2 library and/or headers were not found!])
        fi
    fi
])


dnl -------------------------------------------------------------------------
dnl Try to find SIDPLAY library and header files.
dnl $xs_have_sidplay1 will be "yes" or "no"
dnl @SIDPLAY1_LIBS@ will be substituted with linker parameters
dnl @SIDPLAY1_INCLUDES@ will be substituted with compiler parameters
dnl -------------------------------------------------------------------------

AC_DEFUN([XS_PATH_LIBSIDPLAY1],
[
    AC_MSG_CHECKING([for working SIDPlay1 library and headers])

    AC_LANG_PUSH([C++])
    
    dnl Use include path given by user (if any).
    if test "x$xs_sidplay1_includes" != "x"; then
        xs_sidplay1_cxxflags="-I$xs_sidplay1_includes"
    else
        xs_sidplay1_cxxflags=""
    fi

    dnl Use library path given by user (if any).
    if test "x$xs_sidplay1_library" != "x"; then
        xs_sidplay1_ldflags="-L$xs_sidplay1_library"
    else
        xs_sidplay1_ldflags=""
    fi

    AC_CACHE_VAL([xs_cv_have_sidplay1],
    [
        dnl Run test compilation with either standard search path
        dnl or user-defined paths.
        XS_TRY_LIBSIDPLAY1
        if test "x$xs_sidplay1_works" = "xyes"; then
          xs_cv_have_sidplay1="xs_have_sidplay1=yes  \
            xs_sidplay1_cxxflags=\"$xs_sidplay1_cxxflags\"  \
            xs_sidplay1_ldflags=\"$xs_sidplay1_ldflags\"  "
        else
            XS_FIND_LIBSIDPLAY1        
        fi
    ])
    eval "$xs_cv_have_sidplay1"
    XS_LIB_RESULT([sidplay1], [-lsidplay], [])
    AC_LANG_POP([C++])
])

dnl Functions used by XS_PATH_LIBSIDPLAY1.

AC_DEFUN([XS_FIND_LIBSIDPLAY1],
[
    dnl Search common locations where header files might be stored.
    XS_FIND_FILE([xs_sidplay1_includes], [sidplay/sidtune.h],
    [$xs_sidplay1_includes /usr/include /usr/local/include /usr/lib/sidplay/include /usr/local/lib/sidplay/include /opt/sfw/include /opt/csw/include])

    dnl Search common locations where library might be stored.
    XS_FIND_FILE([xs_sidplay1_library],
    [libsidplay.a libsidplay.so libsidplay.so.1 libsidplay.so.1.36 libsidplay.so.1.37],
    [$xs_sidplay1_library /usr/lib /usr/lib/sidplay /usr/local/lib/sidplay /opt/sfw/lib /opt/csw/lib])

    if test "x$xs_sidplay1_includes" = "x" || test "x$xs_sidplay1_library" = "x"; then
        xs_cv_have_sidplay1="xs_have_sidplay1=no  \
          xs_sidplay1_ldflags=\"\" xs_sidplay1_cxxflags=\"\"  "
    else
        dnl Test compilation with found paths.
        xs_sidplay1_ldflags="-L$xs_sidplay1_library"
        xs_sidplay1_cxxflags="-I$xs_sidplay1_includes"
        XS_TRY_LIBSIDPLAY1
        xs_cv_have_sidplay1="xs_have_sidplay1=$xs_sidplay1_works  \
          xs_sidplay1_ldflags=\"$xs_sidplay1_ldflags\"  \
          xs_sidplay1_cxxflags=\"$xs_sidplay1_cxxflags\"  "
    fi
])

AC_DEFUN([XS_TRY_LIBSIDPLAY1],[
    XS_TRY_LIBRARY([sidplay1], [$xs_sidplay1_cxxflags], [$xs_sidplay1_ldflags],
    [sidplay], [sidplay/sidtune.h], [sidTune* myTest = new sidTune(0);])
])


dnl -------------------------------------------------------------------------
dnl Try to find SIDPLAY2 library and header files.
dnl $xs_have_sidplay2 will be "yes" or "no"
dnl @SIDPLAY_LIBS@ will be substituted with linker parameters
dnl @SIDPLAY_INCLUDES@ will be substituted with compiler parameters
dnl -------------------------------------------------------------------------
AC_DEFUN([XS_PATH_LIBSIDPLAY2],
[
    AC_MSG_CHECKING([for working SIDPlay2 library and headers])

    AC_LANG_PUSH([C++])
    
    AC_REQUIRE([XS_PROG_PKGCONFIG])
    if test "x$PKG_CONFIG" != "x" && $PKG_CONFIG --atleast-version m4_ifval([$1], [$1], [2.1.0]) libsidplay2; then
        xs_pkgcfg_knows="yes"
    else
        xs_pkgcfg_knows="no"
    fi

    dnl Derive sidbuilders path from libsidplay2 root.
    if test "x$xs_sidplay2_library" != "x"; then
        xs_sidplay2_builders="$xs_sidplay2_library/sidplay/builders"
    elif test "x$xs_pkgcfg_knows" = "xyes" ; then
        xs_sidplay2_builders=`$PKG_CONFIG --variable=builders libsidplay2`
    fi

    AC_CACHE_VAL([xs_cv_have_sidplay2],
    [
        dnl Run test compilation with either standard search path
        dnl or user-defined paths.
        xs_sidplay2_ldflags="-lsidplay2"
        XS_TRY_LIBSIDPLAY2
        if test "x$xs_sidplay2_works" = "xyes"; then
          xs_cv_have_sidplay2="xs_have_sidplay2=yes  \
            xs_sidplay2_cxxflags=\"$xs_sidplay2_cxxflags\"  \
            xs_sidplay2_ldflags=\"$xs_sidplay2_ldflags\"  \
            xs_sidplay2_builders=\"$xs_sidplay2_builders\"  "
        else
            XS_FIND_LIBSIDPLAY2
        fi
    ])
    eval "$xs_cv_have_sidplay2"
    XS_LIB_RESULT([sidplay2], [], [])
    AC_LANG_POP([C++])
])


dnl Functions used by XS_PATH_LIBSIDPLAY2.
AC_DEFUN([XS_FIND_LIBSIDPLAY2],
[
    dnl See whether user didn't provide paths.
    if test "x$xs_sidplay2_includes" = "x"; then
        if test "x$xs_pkgcfg_knows" = "xyes" ; then
            xs_sidplay2_includes=`$PKG_CONFIG --variable=includedir libsidplay2`
            xs_sidplay2_cxxflags=`$PKG_CONFIG --cflags libsidplay2`
        else
            dnl Search common locations where header files might be stored.
            XS_FIND_FILE([xs_sidplay2_includes], [sidplay/sidplay2.h],
            [$xs_sidplay2_includes $xs_sidplay2_includes/include /usr/include /usr/local/include /usr/lib/sidplay/include /usr/local/lib/sidplay/include /opt/sfw/include /opt/csw/include])
            xs_sidplay2_cxxflags="-I$xs_sidplay2_includes"
        fi
    else
        xs_sidplay2_cxxflags="-I$xs_sidplay2_includes"
    fi
    if test "x$xs_sidplay2_library" = "x"; then
        if test "x$xs_pkgcfg_knows" = "xyes" ; then
            xs_sidplay2_library=`$PKG_CONFIG --variable=libdir libsidplay2`
            xs_sidplay2_ldflags=`$PKG_CONFIG --libs libsidplay2`
            xs_sidplay2_builders=`$PKG_CONFIG --variable=builders libsidplay2`
        else
            dnl Search common locations where library might be stored.
            XS_FIND_FILE([xs_sidplay2_library], [libsidplay2.la],
            [$xs_sidplay2_library $xs_sidplay2_library/lib $xs_sidplay2_library/src /usr/lib /usr/lib/sidplay /usr/local/lib/sidplay /opt/sfw/lib /opt/csw/lib])
            xs_sidplay2_ldflags="-L$xs_sidplay2_library -lsidplay2"
            xs_sidplay2_builders="$xs_sidplay2_library/sidplay/builders"
        fi
    else
        xs_sidplay2_ldflags="-L$xs_sidplay2_library -lsidplay2"
    fi
    if test "x$xs_sidplay2_includes" = "x" || test "x$xs_sidplay2_library" = "x"; then
        xs_cv_have_sidplay2="xs_have_sidplay2=no \
          xs_sidplay2_ldflags=\"\" xs_sidplay2_cxxflags=\"\" \
          xs_sidplay2_builders=\"\" "
    else
        dnl Test compilation with found paths.
        xs_sidplay2_ldflags="-L$xs_sidplay2_library -lsidplay2"
        xs_sidplay2_cxxflags="-I$xs_sidplay2_includes"
        XS_TRY_LIBSIDPLAY2
        xs_cv_have_sidplay2="xs_have_sidplay2=$xs_sidplay2_works \
          xs_sidplay2_ldflags=\"$xs_sidplay2_ldflags\" \
          xs_sidplay2_cxxflags=\"$xs_sidplay2_cxxflags\" \
          xs_sidplay2_builders=\"$xs_sidplay2_builders\" "
    fi
])


AC_DEFUN([XS_TRY_LIBSIDPLAY2],
[
    XS_TRY_LIBRARY([sidplay2],
        [$xs_sidplay2_cxxflags -DHAVE_UNIX], [$xs_sidplay2_ldflags],
        [sidplay2], [sidplay/sidplay2.h], [sidplay2 *myEngine;])
])


dnl -------------------------------------------------------------------------
dnl Find libsidplay2 builders (sidbuilders) dir.
dnl @BUILDERS_CFLAGS@
dnl @BUILDERS_LDFLAGS@
dnl -------------------------------------------------------------------------
AC_DEFUN([XS_BUILDERS_CHECK],
[
    AC_MSG_CHECKING([for SIDPlay2 builders directory])
    AC_LANG_ASSERT([C++])
    AC_REQUIRE([XS_PATH_LIBSIDPLAY2])

    dnl Sidbuilder headers are included with "builders" prefix.
    builders_includedir="$xs_sidplay2_includes"
    builders_libdir="$xs_sidplay2_builders"

    dnl If libsidplay2 is in standard library search path, we need
    dnl to get an argument whether /usr, /usr/local, etc. Else we
    dnl can only use ${libdir}/sidplay/builders, but then are
    dnl unable to check whether files exist as long as ${exec_prefix}
    dnl is not defined in the configure script. So, this is a bit
    dnl ugly, but a satisfactory fallback default for those who
    dnl define ${prefix} and ${exec_prefix}.
    if test "x$builders_libdir" = "x"; then
        eval "builders_libdir=$libdir/sidplay/builders"
    fi

    AC_ARG_WITH([sidbuilders],
        [AS_HELP_STRING([--with-sidbuilders=DIR], [What the SIDPlay2 builders install PREFIX is])],
        [builders_includedir="$withval/include"
         builders_libdir="$withval/lib/sidplay/builders"])

    AC_ARG_WITH([builders-inc],
        [AS_HELP_STRING([--with-builders-inc=DIR], [Where the SIDPlay2 builders headers are located])],
        [builders_includedir="$withval"])

    AC_ARG_WITH([builders-lib],
        [AS_HELP_STRING([--with-builders-lib=DIR], [Where the SIDPlay2 builders libraries are installed])],
        [builders_libdir="$withval"])
    
    if test "x$builders_includedir" != "x"; then
        BUILDERS_CFLAGS="-I$builders_includedir"
    fi

    if test "x$builders_libdir" != "x"; then
        BUILDERS_LDFLAGS="-L$builders_libdir"
    fi

    if test -d "$builders_libdir"; then
        xs_have_sidbuilders_dir="yes"
        AC_MSG_RESULT([$builders_libdir])
    else
        xs_have_sidbuilders_dir="no"
        AC_MSG_RESULT([$xs_have_sidbuilders_dir])
        AC_MSG_ERROR([$builders_libdir not found!
Check --help on how to specify SIDPlay2 and/or builders library and
header path, or set --exec-prefix to the same prefix as your installation
of libsidplay2.
        ])
    fi

    AC_SUBST([BUILDERS_CFLAGS])
    AC_SUBST([BUILDERS_LDFLAGS])
])


dnl -------------------------------------------------------------------------
dnl Test for working builders
dnl -------------------------------------------------------------------------
AC_DEFUN([XS_BUILDERS_FIND], [dnl
    define([Name], [translit([$1], [a-z], [A-Z])])dnl
    AH_TEMPLATE(HAVE_[]Name[]_BUILDER, [Define if you have $1 for libSIDPlay 2])
    AC_MSG_CHECKING([for $1 builder module])
    XS_TRY_LIBRARY([builders], [$BUILDERS_CFLAGS], [$BUILDERS_LDFLAGS],
    [$2], [sidplay/builders/$3], [$4 *sid;])
    if test "x$xs_builders_works" = "xyes"; then
        xs_builders_available="yes"
        xs_builders="$1 $xs_builders"
        AC_DEFINE(HAVE_[]Name[]_BUILDER)
        BUILDERS_LIBS="$BUILDERS_LIBS -l$2"
    fi
    AC_MSG_RESULT([$xs_builders_works])
    AC_SUBST([BUILDERS_LIBS])
])
