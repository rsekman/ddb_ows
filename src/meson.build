add_global_arguments('-DDDB_OWS_LOGLEVEL=' + get_option('log-level'), language : 'cpp')

fmt_dep = dependency('fmt')
gio_dep = dependency('giomm-2.4')
nlohmann_dep = dependency('nlohmann_json')
spdlog_dep = dependency('spdlog')
sqlite_dep = dependency('sqlite3')
uuid_dep = dependency('uuid')

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
  'cancellationtoken.cpp',
  include_directories: incdir,
  dependencies : [
    fmt_dep,
    gio_dep,
    nlohmann_dep,
    spdlog_dep,
    sqlite_dep,
    uuid_dep,
  ],
  name_prefix: ''
)

shared_module('ddb_ows',
  'ddb_ows.cpp',
  resources,
  include_directories: incdir,
  install: true,
  install_dir: destdir,
  dependencies : [gio_dep, fmt_dep, uuid_dep, spdlog_dep],
  link_with: lib,
  name_prefix: ''
)

subdir('gui')
