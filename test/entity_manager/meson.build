
tests = ['test_match_probe', 'test_template_char_replace']

em_test_deps = [
    boost,
    gtest,
    nlohmann_json_dep,
    phosphor_logging_dep,
    sdbusplus,
    valijson,
]

foreach t : tests
    test(
        t,
        executable(
            t,
            t + '.cpp',
            cpp_args: test_boost_args,
            dependencies: em_test_deps,
            link_with: entity_manager_lib,
            include_directories: test_include_dir,
        ),
    )
endforeach
