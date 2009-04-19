#site_scons
import boost
import gtkmm
import sqlite3
import system
import tommath

#std
import sys

#gui only
src_gui = [
	'gui.cpp',
	'gui_about.cpp',
	'gui_statusbar_main.cpp',
	'gui_vbox_search.cpp',
	'gui_window_download.cpp',
	'gui_window_download_status.cpp',
	'gui_window_preferences.cpp',
	'gui_window_upload.cpp',
	'main_gui.cpp'
]

#nogui only
src_nogui = [
	'main_nogui.cpp'
]

#command to all
src_common = [
	'block_arbiter.cpp',
	'database_init.cpp',
	'database_table_blacklist.cpp',
	'database_table_download.cpp',
	'database_table_hash.cpp',
	'database_table_preferences.cpp',
	'database_table_prime.cpp',
	'database_table_search.cpp',
	'database_table_share.cpp',
	'encryption.cpp',
	'hash_tree.cpp',
	'number_generator.cpp',
	'p2p.cpp',
	'request_generator.cpp',
	'share_index.cpp'
]

#base (needed by all)
env_base = Environment()
system.setup(env_base)

#export base environment that all targets use
Export('env_base')

#gui
env_gui = env_base.Clone()
gtkmm.setup(env_gui)
sqlite3.include(env_gui)
system.random_number(env_gui)
system.networking(env_gui)
tommath.include(env_gui)

#nogui
env_nogui = env_base.Clone()
sqlite3.include(env_nogui)
system.random_number(env_nogui)
system.networking(env_nogui)
tommath.include(env_nogui)

#nogui_static
env_nogui_static = env_nogui.Clone()
system.setup_static(env_nogui_static)

#sub scons files
SConscript([
	'include/SConstruct',
	'libsqlite3/SConstruct',
	'libtommath/SConstruct',
	'site_scons/SConstruct',
#	'unit_tests/SConstruct'
])

static_libs = [
	boost.static_library('date_time'),
	boost.static_library('filesystem'),
	boost.static_library('system'),
	boost.static_library('thread'),
	sqlite3.static_library(),
	tommath.static_library()
]

print 'building with',env_base.GetOption('num_jobs'),'threads'

#compile different object groups
gui_obj = env_gui.Object(src_gui)
nogui_obj = env_nogui.Object(src_nogui)
common_obj = env_base.Object(src_common)

#link
env_gui.Program('p2p_gui', gui_obj + common_obj + static_libs)
env_nogui.Program('p2p_nogui', nogui_obj + common_obj + static_libs)
env_nogui_static.Program('p2p_nogui_static', nogui_obj + common_obj + static_libs)
