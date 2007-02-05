include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libmadplug$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

SOURCES = \
  configure.c \
  dither.c \
  input.c \
  replaygain.c \
  decoder.c \
  fileinfo.c \
  plugin.c \
  xing.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS   += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(TAGLIB_CFLAGS) $(ARCH_DEFINES) $(LIBNMS_CFLAGS) -I../../intl -I../..
CXXFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(TAGLIB_CFLAGS) $(ARCH_DEFINES) -I../../intl -I../..

LIBADD = -lmad -lid3tag $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) $(TAGLIB_LIBS) $(LIBNMS_LIBS)

include ../../mk/objective.mk
