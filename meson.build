project(
  'SwayAudioIdleInhibit',
  'c', 'cpp',
  version: '0.1',
  default_options: ['warning_level=3', 'cpp_std=c++20'],
)

wayland_protos = dependency('wayland-protocols')
wayland_client = dependency('wayland-client', version: '>=1.14.91')

wl_protocol_dir = wayland_protos.get_pkgconfig_variable('pkgdatadir')

wayland_scanner = find_program('wayland-scanner')
wayland_scanner_code = generator(
  wayland_scanner,
  output: '@BASENAME@-protocol.c',
  arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
)
wayland_scanner_client = generator(
  wayland_scanner,
  output: '@BASENAME@-client-protocol.h',
  arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
)

xml = join_paths([wl_protocol_dir, 'unstable/idle-inhibit/idle-inhibit-unstable-v1.xml'])
client_protos_src = wayland_scanner_code.process(xml)
client_protos_headers = wayland_scanner_client.process(xml)

lib_client_protos = static_library(
  'client_protos',
  [ client_protos_src, client_protos_headers ],
)

client_protos = declare_dependency(
  link_with: lib_client_protos,
  sources: client_protos_headers,
)

executable(
  'sway-audio-idle-inhibit',
  [
    './src/main.cpp',
    './src/idle.cpp',
  ],
  dependencies: [
    dependency('libpulse', version: '>= 15.0'),
    wayland_protos,
    wayland_client,
    client_protos,
  ],
  install: true,
)
