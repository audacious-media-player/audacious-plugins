PLUGIN = console${PLUGIN_SUFFIX}

SRCS = Ay_Apu.cc              \
       Ay_Cpu.cc              \
       Ay_Emu.cc              \
       Blip_Buffer.cc         \
       Classic_Emu.cc         \
       Data_Reader.cc         \
       Dual_Resampler.cc      \
       Effects_Buffer.cc      \
       Fir_Resampler.cc       \
       Gbs_Emu.cc             \
       Gb_Apu.cc              \
       Gb_Cpu.cc              \
       Gb_Oscs.cc             \
       gme.cc                 \
       Gme_File.cc            \
       Gym_Emu.cc             \
       Gzip_Reader.cc         \
       Hes_Apu.cc             \
       Hes_Cpu.cc             \
       Hes_Emu.cc             \
       Kss_Cpu.cc             \
       Kss_Emu.cc             \
       Kss_Scc_Apu.cc         \
       M3u_Playlist.cc        \
       Multi_Buffer.cc        \
       Music_Emu.cc           \
       Nes_Apu.cc             \
       Nes_Cpu.cc             \
       Nes_Fme7_Apu.cc        \
       Nes_Namco_Apu.cc       \
       Nes_Oscs.cc            \
       Nes_Vrc6_Apu.cc        \
       Nsfe_Emu.cc            \
       Nsf_Emu.cc             \
       Sap_Apu.cc             \
       Sap_Cpu.cc             \
       Sap_Emu.cc             \
       Sms_Apu.cc             \
       Snes_Spc.cc            \
       Spc_Cpu.cc             \
       Spc_Dsp.cc             \
       Spc_Emu.cc             \
       Spc_Filter.cc          \
       Vfs_File.cc            \
       Vgm_Emu.cc             \
       Vgm_Emu_Impl.cc        \
       Ym2413_Emu.cc          \
       Ym2612_Emu.cc          \
       Zlib_Inflater.cc       \
       Audacious_Driver.cc    \
       configure.cc             \
       plugin.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CXXFLAGS += ${PLUGIN_CFLAGS} -Wno-shift-negative-value
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../..
LIBS += -lz
