include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libsmb$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(GENERAL_PLUGIN_DIR)
LIBADD = $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) -lsmbclient

SOURCES = smb.c
OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../..

include ../../mk/objective.mk
