PLUGIN = madplug${PLUGIN_SUFFIX}

SRCS = mpg123.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} ${MPG123_CFLAGS} -I../..
LIBS += ${GLIB_LIBS} ${MPG123_LIBS} -laudtag -lm
