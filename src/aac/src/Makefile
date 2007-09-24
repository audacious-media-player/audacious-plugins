include ../../../extra.mk

PLUGIN = aac$(PLUGIN_SUFFIX)
plugindir = audacious/$(INPUT_PLUGIN_DIR)

SRCS = libmp4.c mp4_utils.c aac_utils.c tagging_mp4.c

include ../../../buildsys.mk

CFLAGS += $(PLUGIN_CFLAGS)

LIBS += $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)
LIBS += ../libfaad2/libfaad.a ../mp4ff/libmp4ff.a -lstdc++

OBJECTS = ${SOURCES:.c=.o}

LIBDEP = ../libfaad2/libfaad.a ../mp4ff/libmp4ff.a

CPPFLAGS += $(MOWGLI_CFLAGS) $(DBUS_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../.. -I../include -I../libfaad -I../mp4ff -Wall
