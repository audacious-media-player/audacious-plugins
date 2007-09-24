include ../../extra.mk

PLUGIN = cuesheet$(SHARED_SUFFIX)
plugindir = audacious/$(INPUT_PLUGIN_DIR)

SRCS = cuesheet.c

include ../../buildsys.mk

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(ARCH_DEFINES) -I../../intl -I../.. -Wall
CPPFLAGS = $(CFLAGS)
LIBS += $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)
