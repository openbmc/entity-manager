cpp_args_em = cpp_args + ['-DBOOST_ASIO_DISABLE_THREADS']

if get_option('runtime-validate-json')
    cpp_args_em += ['-DENABLE_RUNTIME_VALIDATE_JSON=true']
else
    cpp_args_em += ['-DENABLE_RUNTIME_VALIDATE_JSON=false']
endif

allowed = get_option('new-device-detection')
cpp_args_em += '-DEM_CACHE_CONFIGURATION=' + allowed.to_string()

em_deps = [boost, nlohmann_json_dep, phosphor_logging_dep, sdbusplus, valijson]

entity_manager_lib = static_library(
    'entity-manager',
    'entity_manager.cpp',
    'configuration.cpp',
    'expression.cpp',
    'dbus_interface.cpp',
    '../dbus_regex.cpp',
    'perform_scan.cpp',
    'perform_probe.cpp',
    'power_status_monitor.cpp',
    'overlay.cpp',
    'topology.cpp',
    'utils.cpp',
    'log_device_inventory.cpp',
    '../utils.cpp',
    cpp_args: cpp_args_em,
    dependencies: em_deps,
)

executable(
    'entity-manager',
    'main.cpp',
    cpp_args: cpp_args_em,
    dependencies: em_deps,
    link_with: entity_manager_lib,
    install: true,
    install_dir: installdir,
)

