PLUGIN = mac-now-playing${PLUGIN_SUFFIX}

SRCS = MacNowPlaying.mm

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${OBJCXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../..
LIBS += -framework AppKit -framework MediaPlayer
