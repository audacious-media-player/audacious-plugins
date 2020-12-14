project('audacious-plugins', 'c', 'cpp',
        version: '4.1-devel',
        meson_version: '>= 0.50',
        default_options: [
          'c_std=gnu99',
          'cpp_std=gnu++11',
          'warning_level=1'
        ])


copyright = 'Copyright (C) 2001-2020 Audacious developers and others'


qt5 = import('qt5')


glib_req = '>= 2.32'
glib_dep = dependency('glib-2.0', version: glib_req, required: true)
gmodule_dep = dependency('gmodule-2.0', version: glib_req, required: true)


zlib_dep = dependency('zlib', required: true)


audacious_req = '>= 4.1'
audacious_dep = dependency('audacious', version: audacious_req, required: true)


# XXX - make this its own .pc file
audtag_dep = declare_dependency(link_args: [
  '-L@0@'.format(audacious_dep.get_pkgconfig_variable('lib_dir')),
  '-laudtag'
])


# XXX - make this its own .pc file
audqt_dep = declare_dependency(link_args: [
  '-L@0@'.format(audacious_dep.get_pkgconfig_variable('lib_dir')),
  '-laudqt'
])


# XXX - make this its own .pc file
audgui_dep = declare_dependency(link_args: [
  '-L@0@'.format(audacious_dep.get_pkgconfig_variable('lib_dir')),
  '-laudgui'
])


xml_dep = dependency('libxml-2.0')


if get_option('qt')
  qt_req = '>= 5.2'
  qt_dep = dependency('qt5', version: qt_req, required: true, modules: ['Core', 'Widgets', 'Gui'])
  qtx11extras_dep = dependency('qt5', version: qt_req, required: false, modules: ['X11Extras'])
endif


if get_option('gtk')
  gtk_req = '>= 2.24'
  gtk_dep = dependency('gtk+-2.0', version: gtk_req, required: true)
endif


cdaudio_enabled = false
if get_option('cdaudio')
  libcdio_dep = dependency('libcdio', version: '>= 0.70', required: false)
  libcdio_cdda_dep = dependency('libcdio_cdda', version: '>= 0.70', required: false)
  libcddb_dep = dependency('libcddb', version: '>= 1.2.1', required: false)

  if libcdio_dep.found() and libcdio_cdda_dep.found() and libcddb_dep.found()
    cdaudio_enabled = true
  endif
endif


cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')


math_dep = cxx.find_library('m', required: false)
samplerate_dep = dependency('samplerate', required: false)


if cc.get_id() == 'gcc' or cc.get_id() == 'clang'
  common_flags = [
    '-ffast-math',
    '-Wtype-limits',
    '-Wno-stringop-truncation'
  ]

  cxx_flags = [
    '-Wno-non-virtual-dtor',
    '-Woverloaded-virtual'
  ]

  check_cflags = common_flags
  check_cxxflags = common_flags + cxx_flags

  add_project_arguments(cc.get_supported_arguments(check_cflags), language: 'c')
  add_project_arguments(cxx.get_supported_arguments(check_cxxflags), language: 'cpp')
endif


conf = configuration_data()
conf.set_quoted('COPYRIGHT', copyright)
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('VERSION', meson.project_version())
conf.set('PACKAGE_VERSION', meson.project_version())
if host_machine.endian() == 'big'
  conf.set10('WORDS_BIGENDIAN', true)
endif


# XXX - investigate to see if we can do better
if host_machine.system() == 'windows'
  conf.set_quoted('PLUGIN_SUFFIX', '.dll')
elif host_machine.system() == 'darwin'
  conf.set_quoted('PLUGIN_SUFFIX', '.dylib')
else
  conf.set_quoted('PLUGIN_SUFFIX', '.so')
endif


if host_machine.system() == 'windows'
  conf.set('EXPORT', '__declspec(dllexport)')
elif cc.get_id() == 'gcc' or cc.get_id() == 'clang'
  add_project_arguments('-fvisibility=hidden', language: 'c')
  add_project_arguments('-fvisibility=hidden', language: 'cpp')
  conf.set('EXPORT', '__attribute__((visibility("default")))')
endif


# XXX - why do we have to define this manually?
if (cxx.has_header('libintl.h'))
  add_project_arguments('-DHAVE_GETTEXT', language: 'c')
  add_project_arguments('-DHAVE_GETTEXT', language: 'cpp')
endif


install_plugindir = audacious_dep.get_pkgconfig_variable('plugin_dir')


conf.set_quoted('INSTALL_PLUGINDIR', install_plugindir)
conf.set('plugindir', install_plugindir)


container_plugin_dir = join_paths(install_plugindir, 'Container')
effect_plugin_dir = join_paths(install_plugindir, 'Effect')
general_plugin_dir = join_paths(install_plugindir, 'General')
input_plugin_dir = join_paths(install_plugindir, 'Input')
output_plugin_dir = join_paths(install_plugindir, 'Output')
transport_plugin_dir = join_paths(install_plugindir, 'Transport')
visualization_plugin_dir = join_paths(install_plugindir, 'Visualization')


if get_option('dbus')
  conf.set10('USE_DBUS', true)
endif


if get_option('qt')
  conf.set10('USE_QT', true)
endif


if get_option('gtk')
  conf.set10('USE_GTK', true)
endif


subdir('src')
subdir('po')
