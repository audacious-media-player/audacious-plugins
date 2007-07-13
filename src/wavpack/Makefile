include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libwavpackplugin$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

SOURCES = tags.cxx ui.cxx libwavpack.cxx

LIBADD = -L/usr/local/lib $(WAVPACK_LIBS) $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

CXXFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(WAVPACK_CFLAGS) -I../../intl -I../..

LDFLAGS += -lstdc++

OBJECTS = ${SOURCES:.cxx=.o}

include ../../mk/objective.mk
