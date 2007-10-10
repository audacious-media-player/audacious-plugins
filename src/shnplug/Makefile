PLUGIN = libshnplug$(PLUGIN_SUFFIX)

SRCS = array.c		\
       convert.c	\
       fixio.c		\
       gtk.c		\
       id3v2.c		\
       misc.c		\
       output.c		\
       seek.c		\
       shn.c		\
       shorten.c	\
       sulawalaw.c	\
       vario.c		\
       wave.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := $(plugindir)/$(INPUT_PLUGIN_DIR)

CFLAGS += $(PLUGIN_CFLAGS)
LDFLAGS += $(AUDLDFLAGS)
CPPFLAGS += $(PLUGIN_CPPFLAGS) $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(MOWGLI_CFLAGS) \
	    $(PANGO_CFLAGS) $(ARCH_DEFINES) $(SIMD_CFLAGS) -I../../intl -I../.. -Wall
LIBS += $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

# These were the additional link flags in the old Makefile:
# -L/opt/local/lib -L$(plugindir) -laudid3tag -Wl,-rpath=$(plugindir)
