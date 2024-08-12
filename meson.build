project('hashlink', 'c', version: '1.14.0', default_options: ['c_std=c11'])
thread_dep = dependency('threads')

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)
libdl_dep = cc.find_library('dl', required: false)
add_project_arguments(cc.get_supported_arguments('-fno-omit-frame-pointer'), language: 'c')

subdir('include')

libhl_src = [
	'src/gc.c',
	'src/std/array.c',
	'src/std/buffer.c',
	'src/std/bytes.c',
	'src/std/cast.c',
	'src/std/date.c',
	'src/std/debug.c',
	'src/std/error.c',
	'src/std/file.c',
	'src/std/fun.c',
	'src/std/maps.c',
	'src/std/math.c',
	'src/std/obj.c',
	'src/std/process.c',
	'src/std/random.c',
	'src/std/regexp.c',
	'src/std/socket.c',
	'src/std/string.c',
	'src/std/sys_android.c',
	'src/std/sys.c',
	'src/std/thread.c',
	'src/std/track.c',
	'src/std/types.c',
	'src/std/ucs2.c',
]

hl_incdir = include_directories('src')

pcre_dep = dependency('libpcre2-16')

libhl_deps = [thread_dep, pcre_dep, m_dep]
if host_machine.system() == 'darwin'
	libhl_deps += mdbg_dep
endif

libhl = library(
	'hl',
	libhl_src,
	include_directories: [hl_incdir],
	dependencies: libhl_deps,
	c_args: ['-DLIBHL_EXPORTS'],
	install: true,
)

install_headers('src/hl.h')
install_headers('src/hlc.h')
install_headers('src/hlc_main.c')

libhl_dep = declare_dependency(link_with: [libhl], include_directories: [hl_incdir])

hl_src = [
	'src/code.c',
	'src/jit.c',
	'src/main.c',
	'src/module.c',
	'src/debugger.c',
	'src/profile.c',
]

hl_exe = executable(
	'hl',
	hl_src,
	include_directories: hl_incdir,
	link_with: libhl,
	dependencies: [m_dep, thread_dep, libdl_dep],
	install: true,
	install_rpath: get_option('prefix') / get_option('libdir')
)

test('hl --version', hl_exe, args: ['--version'])

subdir('libs')