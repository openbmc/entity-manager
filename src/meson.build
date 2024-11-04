cpp_args = boost_args + ['-DPACKAGE_DIR="' + packagedir + '/"']
cpp_args += ['-DSYSCONF_DIR="' + sysconfdir + '/"']
installdir = join_paths(get_option('libexecdir'), 'entity-manager')

subdir('entity_manager')

if get_option('fru-device')
    subdir('fru_device')
endif

if get_option('devicetree-vpd')
    subdir('devicetree_vpd_parser')
endif

if get_option('gpio-presence')
    subdir('gpio-presence')
endif
