include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = librootvis.so

CFLAGS += -I. $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(IMLIB2_CFLAGS)

LIBDIR = $(plugindir)/$(VISUALIZATION_PLUGIN_DIR)

LIBADD += $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) $(IMLIB2_LIBS)

SOURCES = \
	config_backend.c \
	config_definition.c \
	config_frontend.c \
	config_frontend_widgets.c \
	getroot.c \
	rootvis.c

OBJECTS = ${SOURCES:.c=.o}

include ../../mk/objective.mk
