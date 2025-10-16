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
        link_with: [entity_manager_lib, utils_lib],
        include_directories: test_include_dir,
    ),
)
