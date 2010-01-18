PLUGIN = sun${PLUGIN_SUFFIX}

SRCS =	sun.c	    \
	audio.c	    \
	convert.c   \
	mixer.c	    \
	configure.c \
	about.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CPPFLAGS +=  ${GTK_CFLAGS}  -I../..
LIBS += ${GTK_LIBS}
