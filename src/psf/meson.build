plugin_sources = [
  'corlett.cc',
  'plugin.cc',
  'eng_psf.cc',
  'eng_psf2.cc',
  'eng_spx.cc',
  'psx.cc',
  'psx_hw.cc'
]


peops_sources = [
  'peops/spu.cc'
]


peops2_sources = [
  'peops2/dma.cc',
  'peops2/registers.cc',
  'peops2/spu.cc'
]


shared_module('psf',
  plugin_sources,
  peops_sources,
  peops2_sources,
  dependencies: [audacious_dep, zlib_dep],
  install: true,
  install_dir: input_plugin_dir
)

