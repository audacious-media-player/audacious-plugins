include ../../../extra.mk

STATIC_LIB_NOINST = libarchive.a

SRCS = archive.cxx     \
          open.cxx        \
          arch_raw.cxx    \
          arch_gzip.cxx   \
          arch_zip.cxx    \
          arch_rar.cxx    \
          arch_bz2.cxx

include ../../../buildsys.mk

CPPFLAGS += -I../../.. $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(AUDACIOUS_CFLAGS)
CXXFLAGS += $(PLUGIN_CFLAGS) 
