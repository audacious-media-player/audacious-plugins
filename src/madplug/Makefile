include ../../extra.mk

PLUGIN = madplug$(PLUGIN_SUFFIX)
plugindir = audacious/$(INPUT_PLUGIN_DIR)

SRCS = \
  configure.c \
  dither.c \
  input.c \
  replaygain.c \
  decoder.c \
  fileinfo.c \
  plugin.c \
  xing.c

include ../../buildsys.mk

CFLAGS   += $(PICFLAGS) $(SIMD_CFLAGS) -Wall
CPPFLAGS += $(DBUS_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(MOWGLI_CFLAGS) $(ARCH_DEFINES) -I../..
LIBS += -L/opt/local/lib -L$(libdir) -laudid3tag -lmad $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)
