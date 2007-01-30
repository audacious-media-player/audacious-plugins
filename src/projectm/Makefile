include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libprojectm$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(VISUALIZATION_PLUGIN_DIR)

LIBADD = $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) $(SDL_LIBS) $(LIBPROJECTM_LIBS) -lprojectM

SOURCES = main.c video_init.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(ARCH_DEFINES) $(XML_CPPFLAGS) $(SDL_CFLAGS) $(LIBPROJECTM_CFLAGS) -I../../intl -I../..

include ../../mk/objective.mk
