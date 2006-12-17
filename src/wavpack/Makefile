include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libwavpack$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

SOURCES = tags.cxx ui.cxx libwavpack.cxx

LIBADD = -L/usr/local/lib $(WAVPACK_LIBS)

CXXFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(WAVPACK_CFLAGS) -I../../intl -I../..

LDFLAGS += -lstdc++

OBJECTS = ${SOURCES:.cxx=.o}

include ../../mk/objective.mk
