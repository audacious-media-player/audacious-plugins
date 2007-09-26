SRCS =	sun.c	    \
	audio.c	    \
	convert.c   \
	mixer.c	    \
	configure.c \
	about.c

PLUGIN = sun$(PLUGIN_SUFFIX)
plugindir = audacious/$(OUTPUT_PLUGIN_DIR)

include ../../buildsys.mk
include ../../extra.mk

CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(PLUGIN_CPPFLAGS) $(MOWGLI_CFLAGS) $(DBUS_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../..
LIBS += $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

