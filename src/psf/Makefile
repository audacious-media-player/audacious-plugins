PLUGIN = psf2${PLUGIN_SUFFIX}

SRCS = corlett.c \
       plugin.c \
       psx.c \
       psx_hw.c \
       eng_psf.c \
       eng_psf2.c \
       eng_spx.c \
       peops/spu.c \
       peops2/dma.c \
       peops2/registers.c \
       peops2/spu.c \

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS} -O0
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS}  -I../.. -Ispu/ -I.
LIBS += ${GTK_LIBS} ${GLIB_LIBS}  -lz
