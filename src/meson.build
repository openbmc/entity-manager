cpp_args = boost_args + ['-DPACKAGE_DIR="' + packagedir + '"']

overlay_args = []
if get_option('use-overlays')
    overlay_args = ['-DOVERLAYS=1']
endif

executable(
    'entity-manager',
    'EntityManager.cpp',
    'Overlay.cpp',
    'Utils.cpp',
    cpp_args: cpp_args + overlay_args,
    dependencies: [
        sdbusplus,
        threads,
    ],
    implicit_include_directories: false,
    include_directories: '../include',
    install: true,
)

executable(
    'fru-device',
    'FruDevice.cpp',
    'Utils.cpp',
    cpp_args: cpp_args,
    dependencies: [
        i2c,
        sdbusplus,
        threads,
    ],
    implicit_include_directories: false,
    include_directories: '../include',
    install: true,
)
