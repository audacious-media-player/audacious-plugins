include ../../mk/rules.mk
include ../../mk/init.mk

sunsources =	sun.c	    \
		audio.c	    \
		convert.c   \
		mixer.c	    \
		configure.c \
		about.c

LIBDIR = $(plugindir)/$(OUTPUT_PLUGIN_DIR)
OBJECTIVE_LIBS = libsun$(SHARED_SUFFIX)

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../..
LIBADD = $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)
SOURCES= $(sunsources)
OBJECTS= ${SOURCES:.c=.o}

include ../../mk/objective.mk
