test(
    'test_fru_utils',
    executable(
        'test_fru_utils',
        'test_fru-utils.cpp',
        '../../src/fru_device/fru_utils.cpp',
        '../../src/fru_device/fru_reader.cpp',
        '../../src/fru_device/gzip_utils.cpp',
        cpp_args: test_boost_args,
        dependencies: [
            boost,
            gtest,
            gmock,
            libxml2_dep,
            phosphor_logging_dep,
            sdbusplus,
            zlib_dep,
        ],
        include_directories: test_include_dir,
    ),
)

test(
    'test_gzip_utils',
    executable(
        'test_gzip_utils',
        'test_gzip-utils.cpp',
        '../../src/fru_device/gzip_utils.cpp',
        cpp_args: test_boost_args,
        dependencies: [
            boost,
            gtest,
            gmock,
            libxml2_dep,
            phosphor_logging_dep,
            sdbusplus,
            zlib_dep,
        ],
        include_directories: test_include_dir,
    ),
)
