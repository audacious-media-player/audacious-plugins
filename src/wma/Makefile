PLUGIN = wma${PLUGIN_SUFFIX}

SRCS = wma.c			\
       libffwma/allcodecs.c	\
       libffwma/allformats.c	\
       libffwma/asf.c		\
       libffwma/avio.c		\
       libffwma/aviobuf.c	\
       libffwma/common.c	\
       libffwma/cutils.c	\
       libffwma/dsputil.c	\
       libffwma/fft.c		\
       libffwma/file.c		\
       libffwma/futils.c	\
       libffwma/mdct.c		\
       libffwma/os_support.c	\
       libffwma/parser.c	\
       libffwma/simple_idct.c	\
       libffwma/utils.c		\
       libffwma/wmadec.c	\
       libffwma/mem.c		\
       libffwma/uri.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS}  ${GTK_CFLAGS} ${GLIB_CFLAGS}  ${SIMD_CFLAGS} -I../.. -I./libffwma -std=c99 ${GCC42_CFLAGS} -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE
LIBS += ${GTK_LIBS} ${GLIB_LIBS} -laudtag
