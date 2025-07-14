project(
	'SwayAudioIdleInhibit',
	'cpp',
	version: '0.2.0',
	default_options: [
		'c_std=c11',
		'cpp_std=c++11',
		'warning_level=2',
		'werror=true',
	],
)

add_project_arguments(
	[
		'-Wno-unused-parameter',
		'-Wno-unused-result',
		'-Wno-missing-braces',
		'-Wno-format-zero-length',
		'-Wno-missing-field-initializers',
		'-Wundef',
		'-Wvla',
		'-Wlogical-op',
		'-Wmissing-include-dirs',
		'-Wpointer-arith',
		'-Winit-self',
		'-Wimplicit-fallthrough=2',
		'-Wendif-labels',
		'-Wstrict-aliasing=2',
		'-Woverflow',
		'-Walloca',
	],
	language: 'cpp',
)

logind_dep = dependency('lib' + get_option('logind-provider'), required: true)

add_project_arguments('-D', 'HAVE_' + get_option('logind-provider').to_upper(), language: 'cpp')

executable(
	'sway-audio-idle-inhibit',
	files(
		'./src/data.cpp',
		'./src/idle.cpp',
		'./src/main.cpp',
		'./src/pulse.cpp',
	),
	include_directories: include_directories('include'),
	dependencies: [
		dependency('libpulse', version: '>= 15.0'),
		logind_dep,
	],
	install: true,
)
