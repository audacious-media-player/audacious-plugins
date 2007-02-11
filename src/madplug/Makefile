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

CFLAGS   += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(ARCH_DEFINES) $(MAD_CFLAGS) -I../../intl -I../..

LDFLAGS += -Wl,-rpath=$(libdir)

LIBADD = -L$(libdir) -lid3tag $(MAD_LIBS) $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

include ../../mk/objective.mk
