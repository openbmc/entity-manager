test(
    'test_gpio_presence',
    executable(
        'test_gpio_presence',
        'test_gpio_presence.cpp',
        cpp_args: test_boost_args,
        include_directories: test_include_dir,
        dependencies: [boost, gtest, phosphor_logging_dep, libgpio_dep],
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
        dependencies: [boost, gtest, phosphor_logging_dep, libgpio_dep],
        link_with: gpio_presence_lib,
    ),
)
