include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libevdev-plug$(SHARED_SUFFIX)

noinst_HEADERS = ed.h ed_ui.h ed_types.h ed_internals.h ed_actions.h ed_bindings_store.h ed_common.h

LIBDIR = $(plugindir)/$(GENERAL_PLUGIN_DIR)

LIBADD = $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)
SOURCES = ed.c ed_ui.c ed_internals.c ed_bindings_store.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) \
	-I../../intl -I../.. -I..

include ../../mk/objective.mk
