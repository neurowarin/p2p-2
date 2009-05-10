#site_scons
import environment
import search

#std
import fnmatch
import os
import re
import sys

#Returns include path for boost. Terminates program if no boost headers found.
def include():
	if sys.platform == 'linux2':
		#possible locations of headers
		possible_locations = ['/usr/local/include', '/usr/include']

		pattern = 'boost-[0-9]{1}_[0-9]{2}'
		for location in possible_locations:
			boost_dir = search.locate_dir(location, pattern)
			if boost_dir:
				return boost_dir

		print 'boost error: headers not found'
		exit(1)

	if sys.platform == 'win32':
		#possible locations of headers
		possible_locations = ['/Program Files/boost']

		pattern = 'boost_[0-9]{1}_[0-9]{2}.*'
		for location in possible_locations:
			boost_dir = search.locate_dir(search_dir, pattern)
			if boost_dir:
				return boost_dir

		print 'boost error: could not locate include directory'
		exit(1)		

#Returns name of library that is suitable to give to linker.
#Example: system -> boost_system-gcc43-mt-1_39
def library(lib_name):
	#possible location of library
	if sys.platform == 'linux2':
		possible_locations = ['/usr/local/lib', '/usr/lib']
	if sys.platform == 'win32':
		possible_locations = ['/Program Files/boost/']

	pattern = 'libboost_' + lib_name + '.*'
	for location in possible_locations:
		lib_path = search.locate_file_recurse(location, pattern)
		if lib_path:
			(head, tail) = os.path.split(lib_path)
			lib_name = re.sub('lib', '', tail)      #remove start of file name 'lib'
			lib_name = re.sub('\..*', '', lib_name) #remove end of file name .*
			return lib_name

	print 'boost error: could not locate static library with pattern ' + pattern
	exit(1)
