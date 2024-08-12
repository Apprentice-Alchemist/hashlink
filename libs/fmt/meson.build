png_dep = dependency('libpng')
jpg_dep = dependency('libturbojpeg')
zlib_dep = dependency('zlib')
ogg_dep = dependency('vorbisfile')

shared_library(
	'fmt',
	['dxt.c', 'fmt.c', 'mikkt.c', 'sha1.c'],
	name_prefix: '',
	name_suffix: 'hdll',
	dependencies: [libhl_dep, png_dep, jpg_dep, minimp3_dep, mikktspace_dep, zlib_dep, ogg_dep],
	install: true,
)