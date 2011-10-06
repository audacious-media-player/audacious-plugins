
AC_DEFUN([AM_PATH_SIDPLAY],
[
XS_ARG_WITH([sidplay1], [libSIDPlay1], [XS_PATH_LIBSIDPLAY1])
if test "x$xs_have_sidplay1" = "xyes"; then
	AC_DEFINE([HAVE_SIDPLAY1])
fi

XS_ARG_WITH([sidplay2], [libSIDPlay2], [XS_PATH_LIBSIDPLAY2])
if test "x$xs_have_sidplay2" = "xyes"; then
	AC_DEFINE([HAVE_SIDPLAY2])
	AC_LANG_PUSH([C++])

	XS_BUILDERS_CHECK
        xs_builders_available="no"
	XS_BUILDERS_FIND([reSID], [resid-builder], [resid.h], [ReSIDBuilder])
	XS_BUILDERS_FIND([HardSID], [hardsid-builder], [hardsid.h], [HardSIDBuilder])
	if test "x$xs_builders_available" = "xno"; then
		AC_MSG_WARN([No builder modules were found in the sidbuilders directory!]);
		xs_have_sidplay2=no
	fi
fi

if test "x$xs_have_sidplay2" = "xyes"; then
	dnl *** Check for new COMI API
	AC_MSG_CHECKING([libSIDPlay2 API style])
	tmp_CPPFLAGS="$CPPFLAGS"
	if test -d "$xs_sidplay2_includes"; then
		CPPFLAGS="$CPPFLAGS -I${xs_sidplay2_includes}"
	fi
	AC_EGREP_HEADER([::Iid], [sidplay/sidplay2.h],
		[xs_have_sidplay2_api="yes"],
		[
		AC_EGREP_HEADER([SIDIID], [sidplay/sidplay2.h],
			[xs_have_sidplay2_api="yes"],
			[xs_have_sidplay2_api="(old style API)"])
		]
	)
	if test "x$xs_have_sidplay2_api" = "xyes"; then
	AC_LINK_IFELSE([
		AC_LANG_PROGRAM(
		[#include <sidplay/sidunknown.h>],
		[ISidUnknown *i;])
	], [
		xs_have_sidplay2_api="(new COMI style API)"
		AC_DEFINE([HAVE_SIDPLAY2_COMI], [1], [Define if using new libSIDPlay2 with COM-style API.])
	],[
		xs_have_sidplay2_api="(broken version!)"
	])
	fi
	AC_MSG_RESULT([$xs_have_sidplay2_api])

	AC_LANG_POP([C++])

	dnl *** Check for distortion patch
	AC_MSG_CHECKING([libSIDPlay2+reSID distortion patch])
	AC_EGREP_HEADER([minimumfetresistance], [sidplay/sidtypes.h], [
		xs_have_distortion="yes"
		AC_DEFINE([HAVE_SIDPLAY2_DISTORTION], [1], [Define if using libSIDPlay+reSID with Antti Lankila's distortion patches.])
	],[
		xs_have_distortion="no"
	])
	AC_MSG_RESULT([$xs_have_distortion])
	CPPFLAGS="$tmp_CPPFLAGS"
fi

if test "x$xs_have_sidplay1" = "xyes" || test "x$xs_have_sidplay2" = "xyes"; then
	m4_ifvaln([$1], [$1], [:])
	m4_ifvaln([$2], [else $2])
fi
])
