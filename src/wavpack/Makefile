include ../../extra.mk

PLUGIN = wavpack$(PLUGIN_SUFFIX)
plugindir = audacious/$(INPUT_PLUGIN_DIR)
SRCS = tags.cxx ui.cxx libwavpack.cxx

include ../../buildsys.mk

CPPFLAGS += $(DBUS_CFLAGS) $(MOWGLI_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(WAVPACK_CFLAGS) -I../../intl -I../..
CXXFLAGS += $(PLUGIN_CFLAGS)

LIBS += -L/usr/local/lib $(WAVPACK_LIBS) $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) -lstdc++

