curl_dep = dependency('libcurl', version: '>= 7.9.7', required: false)


if curl_dep.found()
  shared_module('scrobbler',
    'scrobbler.cc',
    'scrobbler_communication.cc',
    'scrobbler_xml_parsing.cc',
    'config_window.cc',
    dependencies: [audacious_dep, glib_dep, curl_dep, xml_dep],
    install: true,
    install_dir: general_plugin_dir
  )
endif
