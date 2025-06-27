tests = ['utils', 'topology']
em_test_deps = [
    boost,
    gtest,
    gmock,
    nlohmann_json_dep,
    phosphor_logging_dep,
    sdbusplus,
    valijson,
]
foreach t : tests
    test(
        'test_' + t,
        executable(
            'test_' + t,
            t + '.cpp',
            cpp_args: test_boost_args,
            dependencies: em_test_deps,
            link_with: entity_manager_lib,
            include_directories: test_include_dir,
        ),
    )
endforeach
