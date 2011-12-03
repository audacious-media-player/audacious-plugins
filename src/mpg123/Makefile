PLUGIN = madplug${PLUGIN_SUFFIX}

SRCS = mpg123.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MPG123_CFLAGS} -I../..
LIBS += ${MPG123_LIBS} -laudtag -lm
