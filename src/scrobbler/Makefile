include ../../mk/rules.mk
include ../../mk/init.mk

SUBDIRS = 

OBJECTIVE_LIBS = libscrobbler$(SHARED_SUFFIX)

imagesdir = $(datadir)/audacious/images

OBJECTIVE_DATA = audioscrobbler.png:$(imagesdir)	\
	   audioscrobbler_badge.png:$(imagesdir)

LIBDIR = $(plugindir)/$(GENERAL_PLUGIN_DIR)

LIBADD += $(GTK_LIBS) $(CURL_LIBS) $(MUSICBRAINZ_LIBS) $(AUDACIOUS_LIBS)

SOURCES = 			\
	fmt.c			\
	configure.c		\
	gtkstuff.c		\
	md5.c			\
	scrobbler.c		\
	gerpok.c		\
	hatena.c		\
	plugin.c

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(BEEP_DEFINES) $(AUDACIOUS_CFLAGS) $(CURL_CFLAGS) -I../../intl -I../..

OBJECTS = ${SOURCES:.c=.o}

include ../../mk/objective.mk
