PLUGIN = madplug${PLUGIN_SUFFIX}

SRCS = configure.c	\
       input.c		\
       replaygain.c	\
       decoder.c	\
       tuple.c	        \
       plugin.c		\
       xing.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CPPFLAGS +=  ${GTK_CFLAGS}  ${ARCH_DEFINES} ${SIMD_CFLAGS} ${MAD_CFLAGS} -I../..
LIBS += ${MAD_LIBS} -laudid3tag ${GTK_LIBS}
