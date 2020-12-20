ampache_dep = dependency('ampache_browser_1', required: false)
have_ampache = ampache_dep.found()


if have_ampache
  shared_module('ampache',
    'ampache.cc',
    dependencies: [audacious_dep, qt_dep, ampache_dep],
    name_prefix: '',
    install: true,
    install_dir: general_plugin_dir
  )
endif
