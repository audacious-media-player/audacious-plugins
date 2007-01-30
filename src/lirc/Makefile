include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = liblirc$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(GENERAL_PLUGIN_DIR)

LIBADD = -llirc_client $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)
SOURCES = about.c lirc.c

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../..

OBJECTS = ${SOURCES:.c=.o}

include ../../mk/objective.mk
