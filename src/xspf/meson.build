have_xspf = xml_dep.found()


if have_xspf
  shared_module('xspf',
    'xspf.cc',
    dependencies: [audacious_dep, xml_dep, glib_dep],
    name_prefix: '',
    install: true,
    install_dir: container_plugin_dir
  )
endif
