#site_scons
import boost
import environment
import search

#std
import os
import sys

#link in network support
def networking(env):
	environment.define_keys(env)
	if sys.platform == 'win32':
		env['LIBS'].append('ws2_32')

#link in random number generator support
def random_number(env):
	environment.define_keys(env)
	if sys.platform == 'win32':
		env['LIBS'].append('advapi32')

#stuff every targets needs
def setup(env):
	environment.define_keys(env)

	#start minimum of 2 threads, more if > 2 CPUs
	num_cpu = int(os.environ.get('NUM_CPU', 2))
	env.SetOption('num_jobs', num_cpu)

	env['CPPPATH'].append(['#include', '#libsqlite3', '#libtommath'])
	env['LIBPATH'].append(['#libsqlite3', '#libtommath'])

	#boost headers are default for all targets
	boost.include(env)

	#platform specific options
	if sys.platform == 'linux2':
		env['CCFLAGS'].append('-O3')
		env['LIBS'].append('pthread')
	if sys.platform == 'win32':
		env['LIBPATH'].append(__win32_library_dir())
		env['CPPPATH'].append(__win32_include_dir())
		env['CCFLAGS'].append('/EHsc')   #exception support
		env['CCFLAGS'].append('/w')      #disable warnings
		env['CCFLAGS'].append('/Ox')     #max optimizations
		env['CCFLAGS'].append('/DWIN32') #make sure this is defined

#can be run in addition to setup to do static linking
#DEBUG, windows not supported
def setup_static(env):
	environment.define_keys(env)
	if sys.platform == 'linux2':
		env['LINKFLAGS'].append('-static')
		env['LINKFLAGS'].append('-static-libgcc')
		env['LINKFLAGS'].append('`g++ -print-file-name=libstdc++.a`')
	if sys.platform == 'win32':
		print 'windows static linking not supported'
		exit(1)

#returns path of windows API library directory
def __win32_library_dir():
	search_dir = '/Program Files/Microsoft SDKs/Windows/'
	found_dir = search.locate_dir(search_dir, 'Lib')
	if found_dir == '':
		print 'could not locate the windows API headers'
		exit(1)
	else:
		return found_dir

#returns a path to the windows API include path
def __win32_include_dir():
	search_dir = '/Program Files/Microsoft SDKs/Windows/'
	found_dir = search.locate_dir(search_dir, 'Include')
	if found_dir == '':
		print 'could not locate the windows API headers'
		exit(1)
	else:
		return found_dir
