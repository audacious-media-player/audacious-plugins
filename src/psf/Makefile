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

CXXFLAGS += ${PLUGIN_CFLAGS} -Wno-sign-compare
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../.. -Ispu/ -I.
LIBS += -lz
