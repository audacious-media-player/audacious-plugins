PLUGIN = madplug${PLUGIN_SUFFIX}

SRCS = mpg123.c \
       id3skip.c \
       libmpg123/compat.c \
       libmpg123/dct64.c \
       libmpg123/equalizer.c \
       libmpg123/feature.c \
       libmpg123/format.c \
       libmpg123/frame.c \
       libmpg123/icy2utf8.c \
       libmpg123/icy.c \
       libmpg123/id3.c \
       libmpg123/index.c \
       libmpg123/layer1.c \
       libmpg123/layer2.c \
       libmpg123/layer3.c \
       libmpg123/libmpg123.c \
       libmpg123/optimize.c \
       libmpg123/parse.c \
       libmpg123/readers.c \
       libmpg123/stringbuf.c \
       libmpg123/synth.c \
       libmpg123/tabinit.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${MOWGLI_CFLAGS} ${ARCH_DEFINES} ${SIMD_CFLAGS} -I../..
LDFLAGS += ${AUDLDFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${MOWGLI_LIBS} -laudtag -lm
