curl_dep = dependency('libcurl', version: '>= 7.9.7', required: false)
have_scrobbler2 = curl_dep.found() and xml_dep.found()


if have_scrobbler2
  shared_module('scrobbler',
    'scrobbler.cc',
    'scrobbler_communication.cc',
    'scrobbler_xml_parsing.cc',
    'config_window.cc',
    dependencies: [audacious_dep, glib_dep, curl_dep, xml_dep],
    name_prefix: '',
    install: true,
    install_dir: general_plugin_dir
  )
endif
