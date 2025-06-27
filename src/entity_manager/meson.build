cpp_args_em = cpp_args + ['-DBOOST_ASIO_DISABLE_THREADS']

if get_option('runtime-validate-json')
    cpp_args_em += ['-DENABLE_RUNTIME_VALIDATE_JSON=true']
else
    cpp_args_em += ['-DENABLE_RUNTIME_VALIDATE_JSON=false']
endif

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
    'log_device_inventory.cpp',
    '../utils.cpp',
    'main.cpp',
    cpp_args: cpp_args_em,
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

