xrender_dep = dependency('xrender', required: false)
xcomposite_dep = dependency('xcomposite', required: false)

have_aosd = x11_dep.found() and \
            xrender_dep.found() and \
            xcomposite_dep.found()


aosd_sources = [
  'aosd.cc',
  'aosd_cfg.cc',
  'aosd_osd.cc',
  'aosd_style.cc',
  'aosd_trigger.cc',
  'aosd_ui.cc',
  'ghosd.c',
  'ghosd-main.c'
]


if have_aosd
  shared_module('aosd',
    aosd_sources,
    dependencies: [audacious_dep, audgui_dep, math_dep, gtk_dep, glib_dep, x11_dep, xrender_dep, xcomposite_dep],
    name_prefix: '',
    install: true,
    install_dir: general_plugin_dir
  )
endif
