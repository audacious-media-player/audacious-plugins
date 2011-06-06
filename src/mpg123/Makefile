PLUGIN = madplug${PLUGIN_SUFFIX}

SRCS = mpg123.c \
       id3skip.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += -std=gnu99 ${PLUGIN_CPPFLAGS} ${MPG123_CFLAGS} -I../..
LIBS += ${MPG123_LIBS} -laudtag -lm
