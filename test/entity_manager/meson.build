test(
    'test_entity_manager',
    executable(
        'test_entity_manager',
        'test_entity-manager.cpp',
        cpp_args: test_boost_args,
        dependencies: [
            boost,
            gtest,
            nlohmann_json_dep,
            phosphor_logging_dep,
            sdbusplus,
            valijson,
        ],
        link_with: entity_manager_lib,
        include_directories: test_include_dir,
    ),
)

test(
    'test_log_device_inventory',
    executable(
        'test_log_device_inventory',
        'log_device_inventory.cpp',
        cpp_args: test_boost_args,
        dependencies: [
            boost,
            gtest,
            nlohmann_json_dep,
            phosphor_logging_dep,
            sdbusplus,
            valijson,
        ],
        link_with: entity_manager_lib,
        include_directories: test_include_dir,
    ),
)

test(
    'test_entity_manager_class',
    executable(
        'test_entity_manager_class',
        'test_entity_manager_class.cpp',
        cpp_args: test_boost_args,
        dependencies: [
            boost,
            gtest,
            nlohmann_json_dep,
            phosphor_logging_dep,
            sdbusplus,
            valijson,
        ],
        link_with: entity_manager_lib,
        include_directories: test_include_dir,
    ),
)

test(
    'test_dbus_interface',
    executable(
        'test_dbus_interface',
        'test_dbus_interface.cpp',
        cpp_args: test_boost_args,
        dependencies: [
            boost,
            gtest,
            nlohmann_json_dep,
            phosphor_logging_dep,
            sdbusplus,
            valijson,
        ],
        link_with: entity_manager_lib,
        include_directories: test_include_dir,
    ),
)
