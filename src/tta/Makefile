include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libtta$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

SOURCES = libtta.c ttadec.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += -fPIC -DPIC $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../..

LDFLAGS += $(AUDLDFLAGS)
LIBADD = -L$(plugindir) -laudid3tag $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

include ../../mk/objective.mk
