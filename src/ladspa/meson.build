ladspa_sources = [
  'effect.cc',
  'loaded-list.cc',
  'plugin.cc',
  'plugin-list.cc'
]


shared_module('ladspa',
  ladspa_sources,
  dependencies: [audacious_dep, math_dep, gtk_dep, gmodule_dep, audgui_dep],
  install: true,
  install_dir: effect_plugin_dir
)
