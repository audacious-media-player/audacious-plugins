STATIC_LIB_NOINST = libcalc.a

SRCS = dict.c  execute.c  function.c  parser.c  storage.c

include ../../../extra.mk
include ../../../buildsys.mk

CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(PLUGIN_CPPFLAGS) $(GTK_CFLAGS) $(ARCH_DEFINES) $(XML_CPPFLAGS) $(SDL_CFLAGS) -I../../intl -I../..
