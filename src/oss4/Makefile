PLUGIN = oss4${PLUGIN_SUFFIX}

SRCS = plugin.c     \
       oss.c        \
       configure.c  \
       utils.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} -I/usr/lib/oss/include -I../.. ${GTK_CFLAGS} ${GLIB_CFLAGS}
LIBS += ${OSS_LIBS} ${GTK_LIBS} ${GLIB_LIBS}
