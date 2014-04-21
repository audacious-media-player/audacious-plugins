PLUGIN = psf2${PLUGIN_SUFFIX}

SRCS = corlett.cc \
       plugin.cc \
       psx.cc \
       psx_hw.cc \
       eng_psf.cc \
       eng_psf2.cc \
       eng_spx.cc \
       peops/spu.cc \
       peops2/dma.cc \
       peops2/registers.cc \
       peops2/spu.cc \

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}

CXXFLAGS += ${PLUGIN_CFLAGS} -O0 -Wno-narrowing -Wno-sign-compare
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} -I../.. -Ispu/ -I.
LIBS += -lz ${GLIB_LIBS}
