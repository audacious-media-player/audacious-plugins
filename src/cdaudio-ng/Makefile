PLUGIN = cdaudio-ng${PLUGIN_SUFFIX}

SRCS = cdaudio-ng.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

# Headers in libcdio <= 0.83 redefine PACKAGE, VERSION, etc.
# This causes all sorts of trouble.  See for example:
#    http://redmine.audacious-media-player.org/issues/117
#    http://lists.gnu.org/archive/html/libcdio-devel/2012-10/msg00005.html
#    https://bugs.gentoo.org/show_bug.cgi?id=408321
#
# Here we force GCC to stop on the errors, which are hidden by default.
# Users/packagers of libcdio <= 0.83 should fix their headers or update.
CFLAGS += -Wp,-Wsystem-headers -Wp,-Werror

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS}  ${GTK_CFLAGS} ${GLIB_CFLAGS}  ${CDIO_CFLAGS} ${CDDB_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS}  ${CDIO_LIBS} ${CDDB_LIBS}
