
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
	XS_BUILDERS_FIND([reSID], [resid-builder], [resid.h], [ReSIDBuilder])
	XS_BUILDERS_FIND([HardSID], [hardsid-builder], [hardsid.h], [HardSID])
	AC_LANG_POP([C++])
	if test "x$xs_builders_available" = "xno"; then
		AC_MSG_ERROR([No builder modules were found in the sidbuilders directory!]);
	fi
fi

if test "x$xs_have_sidplay1" = "xyes" || test "x$xs_have_sidplay2" = "xyes"; then
	m4_ifvaln([$1], [$1], [:])
	m4_ifvaln([$2], [else $2])
fi
])
