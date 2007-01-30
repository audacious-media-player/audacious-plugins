include ../../mk/rules.mk
include ../../mk/init.mk

SOURCES = jack.c configure.c bio2jack.c

OBJECTIVE_LIBS = libjackout$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(OUTPUT_PLUGIN_DIR)

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -Wall $(SAMPLERATE_CFLAGS) -I../../intl -I../..
LIBADD = $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) $(JACK_LIBS) $(SAMPLERATE_LIBS) -lpthread

OBJECTS = ${SOURCES:.c=.o}

include ../../mk/objective.mk
