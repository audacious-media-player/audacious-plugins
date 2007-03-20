include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libsndstretch$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(EFFECT_PLUGIN_DIR)

SOURCES = sndstretch_xmms.c sndstretch.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) \
	-I../../intl -I../..

include ../../mk/objective.mk
