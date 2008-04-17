PLUGIN = libshnplug${PLUGIN_SUFFIX}

SRCS = array.c		\
       convert.c	\
       fixio.c		\
       gtk.c		\
       id3v2.c		\
       misc.c		\
       output.c		\
       seek.c		\
       shn.c		\
       shorten.c	\
       sulawalaw.c	\
       vario.c		\
       wave.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
LDFLAGS += ${AUDLDFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${PICFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${MOWGLI_CFLAGS} \
	     ${ARCH_DEFINES} ${SIMD_CFLAGS} -I../.. -Wall
LIBS += ${GTK_LIBS} ${GLIB_LIBS} 

