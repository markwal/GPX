#!/usr/bin/env python

from setuptools import setup, Extension

def params():
	name='gpx'
	version='1.0'
	description='Translates to and from gcode to x3g, more or less'
	author='Mark Walker'
	author_email='markwal@hotmail.com'
	url='https://www.github.com/markwal/GPX'
	ext_modules = [
		Extension('gpx',
		sources = [
			'gpxmodule.c',
			'../shared/config.c',
			'../shared/opt.c',
			'../gpx/gpx.c',
			'../gpx/gpx-main.c',
			],
		extra_compile_args = ['-DSERIAL_SUPPORT', '-fvisibility=hidden', '-I../shared', '-I../gpx'],
		extra_link_args = ['-fvisibility=hidden'])
		]
	return locals()

setup(**params())
