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
    'test_em_config',
    executable(
        'test_em_config',
        'test_em_config.cpp',
        cpp_args: test_boost_args,
        dependencies: [
            boost,
            gtest,
            nlohmann_json_dep,
            phosphor_logging_dep,
            sdbusplus,
        ],
        link_with: entity_manager_lib,
        include_directories: test_include_dir,
    ),
)
