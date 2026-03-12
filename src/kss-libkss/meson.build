libkss_sources = [
  'libkss/src/filters/dc_filter.c',
  'libkss/src/filters/filter.c',
  'libkss/src/filters/rc_filter.c',
  'libkss/src/kss/bgm2kss.c',
  'libkss/src/kss/kss.c',
  'libkss/src/kss/kss2kss.c',
  'libkss/src/kss/kssload.c',
  'libkss/src/kss/mbm2kss.c',
  'libkss/src/kss/mgs2kss.c',
  'libkss/src/kss/mpk2kss.c',
  'libkss/src/kss/opx2kss.c',
  'libkss/src/kss2vgm.c',
  'libkss/src/kssplay.c',
  'libkss/src/rconv/psg_rconv.c',
  'libkss/src/vm/detect.c',
  'libkss/src/vm/mmap.c',
  'libkss/src/vm/vm.c',
  'libkss/modules/emu2149/emu2149.c',
  'libkss/modules/emu2212/emu2212.c',
  'libkss/modules/emu2413/emu2413.c',
  'libkss/modules/emu76489/emu76489.c',
  'libkss/modules/emu8950/emu8950.c',
  'libkss/modules/emu8950/emuadpcm.c',
  'libkss/modules/kmz80/kmdmg.c',
  'libkss/modules/kmz80/kmevent.c',
  'libkss/modules/kmz80/kmr800.c',
  'libkss/modules/kmz80/kmz80.c',
  'libkss/modules/kmz80/kmz80c.c',
  'libkss/modules/kmz80/kmz80t.c',
]


c_args = cc.get_supported_arguments([
  '-Wno-incompatible-pointer-types',
  '-Wno-int-conversion',
  '-Wno-enum-int-mismatch',
])


shared_module('kss-libkss',
  libkss_sources,
  'kss-plugin.cc',
  c_args: c_args,
  include_directories: [
    src_inc,
    include_directories('libkss/src', 'libkss/modules'),
  ],
  dependencies: [audacious_dep],
  name_prefix: '',
  install: true,
  install_dir: input_plugin_dir,
)
