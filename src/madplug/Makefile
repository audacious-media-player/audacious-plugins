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

CFLAGS   += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(ARCH_DEFINES) $(SIMD_CFLAGS) -I../../intl -I../.. -Wall

LDFLAGS += $(AUDLDFLAGS)
LIBADD = -L/opt/local/lib -L$(libdir) -laudid3tag -lmad $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

include ../../mk/objective.mk
