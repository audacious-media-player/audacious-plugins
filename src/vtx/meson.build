vtx_sources = [
  'ay8912.cc',
  'info.cc',
  'lh5dec.cc',
  'vtx.cc',
  'vtxfile.cc'
]


vtx_deps = [audacious_dep]


if (conf.has('USE_GTK'))
  vtx_deps += [gtk_dep, audgui_dep]
endif


shared_module('vtx',
  vtx_sources,
  dependencies: vtx_deps,
  install: true,
  install_dir: input_plugin_dir
)
