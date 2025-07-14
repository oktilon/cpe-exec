project(
    'executor',
    'c',
    version : '1.0.1',
    meson_version : '>= 0.57.0'
)

# Setup variables
version_arr = meson.project_version().split('.')
prj_name = meson.project_name()

# D-Bus names
base_name = 'com.agrocorp.control'
base_path = '/com/agrocorp/control'

# Project configuration
conf_data = configuration_data()
conf_data.set('version_major',      version_arr[0])
conf_data.set('version_minor',      version_arr[1])
conf_data.set('version_rev',        version_arr[2])
conf_data.set('log_file',           '/var/log/' + prj_name)
conf_data.set('db_serv',            get_option('db_host'))
conf_data.set('db_port',            get_option('db_port'))
conf_data.set('db_base',            get_option('db_base'))
conf_data.set('db_user',            get_option('db_user'))
conf_data.set('db_pass',            get_option('db_pass'))
conf_data.set('path',               get_option('scripts_path'))
conf_data.set('git_path',           get_option('git_path'))
conf_data.set('out_path',           get_option('out_path'))
conf_data.set('tg_key',             get_option('tg_key'))
conf_data.set('tg_chat',            get_option('tg_chat'))
conf_data.set('php_log',            get_option('php_log'))
conf_data.set('user',               get_option('user'))
conf_data.set('bus_srv_name',       base_name)
conf_data.set('bus_srv_path',       base_path)


# cc = meson.get_compiler('c')


# Dependencies
deps = [
    dependency('threads'),
    dependency('libsystemd'),
    dependency('libcurl'),
    dependency('jansson'),
    dependency('mysqlclient')
    # cc.find_library('m', required : false)
]


# Includes
inc = include_directories([
    'inc',
])

# Sources
src = [
    'src/storage.c',
    'src/report.c',
    'src/sys.c',
    'src/bus.c',
    'src/main.c'
]

# Main header file config
configure_file(
    input         : 'config.hh',
    output        : 'config.h',
    configuration : conf_data
)

configure_file(
    input         : 'policy.xml',
    output        : base_name + '.conf',
    install       : true,
    install_dir   : '/etc/dbus-1/system.d',
    configuration : conf_data
)

# Build executable
executable(
    prj_name,
    src,
    include_directories : inc,
    dependencies        : deps,
    install             : true,
)