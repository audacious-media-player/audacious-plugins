include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libshnplug$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

SOURCES = \
  array.c \
  convert.c \
  fixio.c \
  gtk.c \
  id3v2.c \
  misc.c \
  output.c \
  seek.c \
  shn.c \
  shorten.c \
  sulawalaw.c \
  vario.c \
  wave.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS   += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(ARCH_DEFINES) $(SIMD_CFLAGS) -I../../intl -I../.. -Wall

LDFLAGS += $(AUDLDFLAGS)
LIBADD = -L/opt/local/lib -L$(plugindir) -Wl,-rpath=$(plugindir) -laudid3tag $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

include ../../mk/objective.mk
