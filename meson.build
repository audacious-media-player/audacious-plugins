project('audacious-plugins', 'c', 'cpp',
        version: '3.11-devel',
        meson_version: '>= 0.50')


copyright = 'Copyright (C) 2001-2018 Audacious developers and others'


qt5 = import('qt5')
gnome = import('gnome')


glib_req = '>= 2.32'
glib_dep = dependency('glib-2.0', version: glib_req, required: true)
gmodule_dep = dependency('gmodule-2.0', version: glib_req, required: true)


audacious_req = '>= 3.11'
audacious_dep = dependency('audacious', version: audacious_req, required: true)


# XXX - make this it's own .pc file
audtag_dep = declare_dependency(link_args: [
  '-L@0@'.format(audacious_dep.get_pkgconfig_variable('libdir')),
  '-laudtag'
])


# XXX - make this it's own .pc file
audqt_dep = declare_dependency(link_args: [
  '-L@0@'.format(audacious_dep.get_pkgconfig_variable('libdir')),
  '-laudqt'
])


xml_dep = dependency('libxml-2.0')


if get_option('qt')
  qt_req = '>= 5.2'
  qt_dep = dependency('qt5', version: qt_req, required: true, modules: ['Core', 'Widgets', 'Gui'])
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


if cc.get_id() == 'gcc' or cc.get_id() == 'clang'
  common_flags = [
    '-pipe',
    '-ffast-math',
    '-Wall',
    '-Wtype-limits',
    '-Wno-stringop-truncation'
  ]

  cxx_flags = [
    '-Wno-non-virtual-dtor',
    '-Woverloaded-virtual'
  ]

  check_cflags = ['-std=gnu99'] + common_flags
  check_cxxflags = ['-std=gnu++11'] + common_flags + cxx_flags

  foreach arg : check_cflags
    if cc.has_argument(arg)
      add_project_arguments(arg, language: 'c')
    endif
  endforeach

  foreach arg : check_cxxflags
    if cxx.has_argument(arg)
      add_project_arguments(arg, language: 'cpp')
    endif
  endforeach
endif


conf = configuration_data()
conf.set_quoted('BUILDSTAMP', '???')
conf.set_quoted('COPYRIGHT', copyright)
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('VERSION', meson.project_version())
conf.set('PACKAGE_VERSION', meson.project_version())
if host_machine.endian() == 'big'
  conf.set10('WORDS_BIGENDIAN', true)
endif


# XXX - investigate to see if we can do better
conf.set_quoted('PLUGIN_SUFFIX', '.so')
if host_machine.system() == 'windows'
  conf.set_quoted('PLUGIN_SUFFIX', '.dll')
elif host_machine.system() == 'darwin'
  conf.set_quoted('PLUGIN_SUFFIX', '.dylib')
endif


if host_machine.system() == 'windows'
  conf.set('EXPORT', '__declspec(dllexport)')
  conf.set_quoted('PLUGIN_SUFFIX', '.dll')
elif cc.get_id() == 'gcc' or cc.get_id() == 'clang'
  add_project_arguments('-fvisibility=hidden', language: 'c')
  add_project_arguments('-fvisibility=hidden', language: 'cpp')
  conf.set('EXPORT', '__attribute__((visibility("default")))')
endif


install_plugindir = join_paths(get_option('libdir'), 'audacious')


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


subdir('src')
subdir('po')
