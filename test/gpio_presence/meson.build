gpio_presence_test_deps = [
    boost,
    gtest,
    gmock,
    phosphor_logging_dep,
    libgpio_dep,
]

test(
    'test_gpio_presence',
    executable(
        'test_gpio_presence',
        'test_gpio_presence.cpp',
        cpp_args: test_boost_args,
        include_directories: test_include_dir,
        dependencies: gpio_presence_test_deps,
        link_with: gpio_presence_lib,
    ),
)

test(
    'test_gpio_presence_state',
    executable(
        'test_gpio_presence_state',
        'test_gpio_presence_state.cpp',
        cpp_args: test_boost_args,
        include_directories: test_include_dir,
        dependencies: gpio_presence_test_deps,
        link_with: gpio_presence_lib,
    ),
)
