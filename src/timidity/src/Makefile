include ../../../extra.mk

PLUGIN = timidity$(PLUGIN_SUFFIX)
plugindir = audacious/$(INPUT_PLUGIN_DIR)

SRCS = \
	callbacks.c \
	interface.c \
	xmms-timidity.c \

LIBDEP = ../libtimidity/libtimidity.a

include ../../../buildsys.mk

LIBS += ../libtimidity/libtimidity.a $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)
CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(MOWGLI_CFLAGS) $(DBUS_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../.. -I../libtimidity
