PLUGIN = alsa${PLUGIN_SUFFIX}

SRCS = alsa.cc \
       config.cc \
       plugin.cc \

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${GTK_CFLAGS} ${ALSA_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${ALSA_LIBS}
