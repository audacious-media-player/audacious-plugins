include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libstereo$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(EFFECT_PLUGIN_DIR)

LIBADD = $(GTK_LIBS) $(AUDACIOUS_LIBS)

SOURCES = stereo.c

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(AUDACIOUS_CFLAGS) -I../../intl -I../..

OBJECTS = ${SOURCES:.c=.o}

include ../../mk/objective.mk
