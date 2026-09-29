cpp_args_em = cpp_args + ['-DBOOST_ASIO_DISABLE_THREADS']

if get_option('runtime-validate-json')
    cpp_args_em += ['-DENABLE_RUNTIME_VALIDATE_JSON=true']
else
    cpp_args_em += ['-DENABLE_RUNTIME_VALIDATE_JSON=false']
endif

allowed = get_option('new-device-detection')
cpp_args_em += '-DEM_CACHE_CONFIGURATION=' + allowed.to_string()

em_deps = [
    boost,
    nlohmann_json_dep,
    phosphor_dbus_interfaces_dep,
    phosphor_logging_dep,
    sdbusplus,
    valijson,
]

entity_manager_lib = static_library(
    'entity-manager',
    'entity_manager.cpp',
    'configuration.cpp',
    'expression.cpp',
    'dbus_interface.cpp',
    'perform_scan.cpp',
    'perform_probe.cpp',
    'object_mapper.cpp',
    'probe_lexer.cpp',
    'power_status_monitor.cpp',
    'overlay.cpp',
    'topology.cpp',
    'utils.cpp',
    'log_device_inventory.cpp',
    cpp_args: cpp_args_em,
    dependencies: em_deps,
    link_with: utils_lib,
)

# This tool runs on the build machine, including when Entity Manager is
# cross-compiled. Keep the validation implementation separate so it can also
# be linked into the runtime after its results have been compared with CI.
native_json_dep = dependency(
    'nlohmann_json',
    native: true,
    include_type: 'system',
)
native_threads_dep = dependency('threads', native: true)
native_logging_dep = dependency('phosphor-logging', native: true)
config_validation_native_lib = static_library(
    'config-validation-native',
    'config_validation.cpp',
    native: true,
    dependencies: [native_json_dep, valijson],
)

config_validator = executable(
    'validate-configs',
    'validate_configs.cpp',
    native: true,
    cpp_args: ['-DEM_SOURCE_ROOT="' + meson.project_source_root() + '"'],
    dependencies: [
        native_json_dep,
        valijson,
        native_threads_dep,
        native_logging_dep,
    ],
    link_with: config_validation_native_lib,
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
