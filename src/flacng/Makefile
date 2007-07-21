include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libflacng$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

LIBADD = $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) $(LIBFLAC_LIBS)

SOURCES = plugin.c tools.c seekable_stream_callbacks.c flac_compat112.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../.. $(LIBFLAC_CFLAGS) -Wall

include ../../mk/objective.mk
