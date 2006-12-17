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

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(AUDACIOUS_CFLAGS) -I../../intl -I../..
LIBADD = $(GTK_LIBS) $(AUDACIOUS_LIBS)
SOURCES= $(sunsources)
OBJECTS= ${SOURCES:.c=.o}

include ../../mk/objective.mk
