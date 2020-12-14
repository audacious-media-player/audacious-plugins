ampache_dep = dependency('ampache_browser_1', required: false)


if ampache_dep.found()
  shared_module('ampache',
    'ampache.cc',
    dependencies: [audacious_dep, qt_dep, ampache_dep],
    install: true,
    install_dir: general_plugin_dir
  )
endif
