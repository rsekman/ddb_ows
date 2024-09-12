add_global_arguments('-DLIBDIR="'  + get_option('libdir') + '"', language : 'cpp')
add_global_arguments('-DDDB_OWS_LOGLEVEL=3', language : 'cpp')

gio_dep = dependency('giomm-2.4')
gtk_dep = dependency('gtkmm-3.0')
uuid_dep = dependency('uuid')
fmt_dep = dependency('fmt')

incdir = include_directories('../include')

gnome = import('gnome')
resources = gnome.compile_resources('ddb_ows_resources',
  'resources.xml'
)

lib = static_library('libddb_ows',
  'config.cpp',
  'database.cpp',
  'job.cpp',
  'jobsqueue.cpp',
  'logger.cpp',
  'playlist_uuid.cpp',
  include_directories: incdir,
  dependencies : [gio_dep, fmt_dep, uuid_dep],
  name_prefix: ''
)

shared_module('ddb_ows',
  'ddb_ows.cpp',
  resources,
  include_directories: incdir,
  install: true,
  install_dir: destdir,
  dependencies : [gio_dep, fmt_dep, uuid_dep],
  link_with: lib,
  name_prefix: ''
)

subdir('gui')
