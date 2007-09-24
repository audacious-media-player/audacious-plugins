include ../../extra.mk

PLUGIN = scrobbler$(SHARED_SUFFIX)
plugindir = audacious/$(GENERAL_PLUGIN_DIR)

DATA = images/audioscrobbler.png images/audioscrobbler_badge.png

SRCS = 			\
	fmt.c			\
	configure.c		\
	gtkstuff.c		\
	md5.c			\
	scrobbler.c		\
	gerpok.c		\
	plugin.c

include ../../buildsys.mk

# XXX: used to get install paths right
PACKAGE = audacious

CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(PLUGIN_CPPFLAGS) $(MOWGLI_CFLAGS) $(DBUS_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(BEEP_DEFINES) $(CURL_CFLAGS) \
	-I../../intl -I../..
LIBS += $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) $(CURL_LIBS) $(MUSICBRAINZ_LIBS)

