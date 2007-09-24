PLUGIN = projectm$(PLUGIN_SUFFIX)
plugindir = audacious/$(VISUALIZATION_PLUGIN_DIR)
SRCS = main.c video_init.c

include ../../buildsys.mk
include ../../extra.mk

LIBS += $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) $(SDL_LIBS) $(LIBPROJECTM_LIBS) -lprojectM
CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(PLUGIN_CPPFLAGS) $(MOWGLI_CFLAGS) $(DBUS_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(ARCH_DEFINES) $(XML_CPPFLAGS) $(SDL_CFLAGS) $(LIBPROJECTM_CFLAGS) -I../../intl -I../..
