test(
    'test_utils',
    executable(
        'test_utils',
        'utils.cpp',
        cpp_args: test_boost_args,
        dependencies: [
            boost,
            gtest,
            gmock,
            nlohmann_json_dep,
            sdbusplus,
            phosphor_logging_dep,
        ],
        link_with: [entity_manager_lib, utils_lib],
        include_directories: test_include_dir,
    ),
)

test(
    'test_topology',
    executable(
        'test_topology',
        'topology.cpp',
        cpp_args: test_boost_args,
        dependencies: [
            boost,
            gtest,
            gmock,
            nlohmann_json_dep,
            sdbusplus,
            phosphor_logging_dep,
        ],
        link_with: [entity_manager_lib, utils_lib],
        include_directories: test_include_dir,
    ),
)
