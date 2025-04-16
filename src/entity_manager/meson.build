executable(
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
    '../utils.cpp',
    cpp_args: cpp_args + ['-DBOOST_ASIO_DISABLE_THREADS'],
    dependencies: [
        boost,
        nlohmann_json_dep,
        phosphor_logging_dep,
        sdbusplus,
        valijson,
    ],
    install: true,
    install_dir: installdir,
)

