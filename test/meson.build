gtest = dependency('gtest', main: true, disabler: true, required: build_tests)

test(
    'test_entity_manager',
    executable(
        'test_entity_manager',
        'test_entity-manager.cpp',
        '../src/Utils.cpp',
        dependencies: [
            gtest,
            sdbusplus,
        ],
        implicit_include_directories: false,
        include_directories: '../include',
    )
)
