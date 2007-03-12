include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libffmpeg$(SHARED_SUFFIX)

SUBDIRS = libavcodec libavformat libavutil

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

LIBADD = ./libavcodec/libavcodec.a ./libavutil/libavutil.a ./libavformat/libavformat.a $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

SOURCES = ffmpeg.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../.. -I./libffwma -std=c99

include ../../mk/objective.mk
