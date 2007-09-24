include ../../extra.mk

PLUGIN = flacng$(SHARED_SUFFIX)
plugindir = audacious/$(INPUT_PLUGIN_DIR)

SRCS = plugin.c tools.c seekable_stream_callbacks.c flac_compat112.c

include ../../buildsys.mk

LIBS += $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) $(LIBFLAC_LIBS)

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(MOWGLI_CFLAGS) $(DBUS_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../.. $(LIBFLAC_CFLAGS) -Wall
