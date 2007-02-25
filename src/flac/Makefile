include ../../mk/rules.mk
include ../../mk/init.mk

SUBDIRS = libflac plugin_common

noinst_HEADERS = \
	charset.h \
	configure.h \
	plugin.h \
	tag.h \
	fast_float_math_hack.h \
	replaygain_analysis.h \
	grabbag.h \
	replaygain_synthesis.h

OBJECTIVE_LIBS = libflac$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

LIBADD = $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) -L./libflac -L./plugin_common -lflac -logg -lplugin_common

SOURCES = \
	charset.c \
	configure.c \
	fileinfo.c \
	plugin.c \
	tag.c \
	replaygain_synthesis.c \
	replaygain.c \
	replaygain_analysis.c \
	file.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../.. -I./libflac

include ../../mk/objective.mk
