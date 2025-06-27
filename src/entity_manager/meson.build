em_deps = [boost, nlohmann_json_dep, phosphor_logging_dep, sdbusplus, valijson]

entity_manager_lib = static_library(
    'entity-manager',
    'entity_manager.cpp',
    'configuration.cpp',
    'expression.cpp',
    'dbus_interface.cpp',
    'perform_scan.cpp',
    'perform_probe.cpp',
    'overlay.cpp',
    'topology.cpp',
    'utils.cpp',
    'log_device_inventory.cpp',
    '../utils.cpp',
    cpp_args: cpp_args + ['-DBOOST_ASIO_DISABLE_THREADS'],
    dependencies: em_deps,
)

executable(
    'entity-manager',
    'main.cpp',
    cpp_args: cpp_args + ['-DBOOST_ASIO_DISABLE_THREADS'],
    dependencies: em_deps,
    link_with: entity_manager_lib,
    install: true,
    install_dir: installdir,
)

