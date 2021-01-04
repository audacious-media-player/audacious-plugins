have_asx3 = xml_dep.found()


if have_asx3
  shared_module('asx3',
    'asx3.cc',
    dependencies: [audacious_dep, xml_dep],
    name_prefix: '',
    install: true,
    install_dir: container_plugin_dir
  )
endif
