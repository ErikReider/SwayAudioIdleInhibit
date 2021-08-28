project(
  'AudioListener',
  'c', 'cpp',
  default_options: ['warning_level=3'],
)

deps = [
  dependency('libpulse', version: '>= 15.0'),
]

executable(
  'audioListener',
  './src/main.cpp',
  dependencies: deps,
  install: true,
)
