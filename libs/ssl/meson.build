mbedtls_dep = dependency('mbedtls')
mbedcrypto_dep = dependency('mbedcrypto')
mbedx509_dep = dependency('mbedx509')

shared_library(
	'ssl',
	['ssl.c'],
	name_prefix: '',
	name_suffix: 'hdll',
	dependencies: [libhl_dep, mbedtls_dep, mbedcrypto_dep, mbedx509_dep],
	install: true
)