have_lyrics = xml_dep.found()

json_glib_dep = dependency('json-glib-1.0', version: '>= 1.0', required: true)

lyrics_deps = [audacious_dep, audgui_dep, gtk_dep, glib_dep, json_glib_dep, xml_dep]
lyrics_src = [
  '../lyrics-common/chart_lyrics_provider.cc',
  '../lyrics-common/file_provider.cc',
  '../lyrics-common/lrclib_provider.cc',
  '../lyrics-common/lyrics_ovh_provider.cc',
  '../lyrics-common/utils.cc',
  'lyrics-gtk.cc'
]

if have_lyrics
  shared_module('lyrics-gtk',
    lyrics_src,
    dependencies: lyrics_deps,
    name_prefix: '',
    install: true,
    install_dir: general_plugin_dir
  )
endif
