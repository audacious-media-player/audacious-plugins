include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libwav$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

SOURCES = wav$(WAV_SNDFILE).c

LIBADD = -L/usr/local/lib $(SNDFILE_LIBS) $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../.. -Wall

OBJECTS = ${SOURCES:.c=.o}

include ../../mk/objective.mk
