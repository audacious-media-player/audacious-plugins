PLUGIN = vorbis${PLUGIN_SUFFIX}

SRCS = vcupdate.cc \
       vcedit.cc		\
       vorbis.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${VORBIS_CFLAGS} ${GLIB_CFLAGS}  -I../..
LIBS += ${VORBIS_LIBS} ${GLIB_LIBS}
