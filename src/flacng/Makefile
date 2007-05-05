include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libflacng$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

LIBADD = $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) $(FLAC_LIBS)

SOURCES = plugin.c tools.c seekable_stream_callbacks.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../.. $(FLAC_CFLAGS)

include ../../mk/objective.mk
