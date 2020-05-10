#!/usr/bin/env python

import sys
from setuptools import setup, Extension

sources = [
	'gpxmodule.c',
	'../shared/machine_config.c',
	'../shared/opt.c',
	'../gpx/vector.c',
	'../gpx/gpx.c',
	'../gpx/gpx-main.c',
	'../gpx/gpxresp.c',
	]
if sys.platform == 'win32':
	sources.append('../gpx/winsio.c')

def params():
	name='gcodex3g'
	version='1.0'
	description='Translates to and from gcode to x3g, more or less'
	author='Mark Walker'
	author_email='markwal@hotmail.com'
	url='https://www.github.com/markwal/GPX'
	ext_modules = [
		Extension('gcodex3g',
		sources = sources,
		extra_compile_args = ['-DGPX_VERSION="\\"Python\\""', '-DSERIAL_SUPPORT', '-fvisibility=hidden', '-I../../build/src/shared', '-I../shared', '-I../gpx'],
		extra_link_args = ['-fvisibility=hidden'])
		]
	return locals()

setup(**params())
