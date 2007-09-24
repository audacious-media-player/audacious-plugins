include ../../../extra.mk

STATIC_LIB_NOINST = libmp4ff.a

SRCS = mp4ff.c mp4atom.c mp4meta.c mp4sample.c mp4util.c \
   mp4tagupdate.c

include ../../../buildsys.mk

CFLAGS += $(PLUGIN_CFLAGS) -DUSE_TAGGING=1 -fsigned-char -I../../.. -Wall
