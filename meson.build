project(
    'entity-manager',
    'cpp',
    default_options: [
        'warning_level=3',
        'werror=true',
        'cpp_std=c++20'
    ],
    license: 'Apache-2.0',
    version: '0.1',
    meson_version: '>=0.57.0',
)
add_project_arguments('-Wno-psabi', language: 'cpp')

boost_args = [
    '-DBOOST_SYSTEM_NO_DEPRECATED',
    '-DBOOST_ERROR_CODE_HEADER_ONLY',
    '-DBOOST_NO_RTTI',
    '-DBOOST_NO_TYPEID',
    '-DBOOST_ALL_NO_LIB',
    '-DBOOST_ALLOW_DEPRECATED_HEADERS'
]
build_tests = get_option('tests')
cpp = meson.get_compiler('cpp')
boost = dependency('boost', required: false)
if not boost.found()
     subproject('boost', required: false)
     boost = declare_dependency(
         include_directories: 'subprojects/boost_1_71_0',
     )
     boost = boost.as_system('system')
endif
if get_option('fru-device')
    i2c = cpp.find_library('i2c')
endif

nlohmann_json_dep = dependency('nlohmann_json',
                               fallback: [ 'nlohmann_json', 'nlohmann_json_dep'])

sdbusplus = dependency('sdbusplus', required: false)
if not sdbusplus.found()
    sdbusplus_proj = subproject('sdbusplus', required: true)
    sdbusplus = sdbusplus_proj.get_variable('sdbusplus_dep')
    sdbusplus = sdbusplus.as_system('system')
endif
systemd = dependency('systemd')
systemd_system_unit_dir = systemd.get_pkgconfig_variable(
    'systemdsystemunitdir',
    define_variable: ['prefix', get_option('prefix')])
packagedir = join_paths(
    get_option('prefix'),
    get_option('datadir'),
    meson.project_name(),
)
sysconfdir = join_paths(
    get_option('prefix'),
    get_option('sysconfdir'),
    meson.project_name(),
)
threads = dependency('threads')
if cpp.has_header('valijson/validator.hpp')
    valijson = declare_dependency()
else
    subproject('valijson', required: false)
    valijson = declare_dependency(
        include_directories: 'subprojects/valijson/include'
    )
    valijson = valijson.as_system('system')
endif

install_data('blacklist.json')

configs = [
    '1Ux16_Riser.json',
    '2Ux8_Riser.json',
    '8X25_HSBP.json',
    'A2UL16RISER.json',
    'A2UX8X4RISER.json',
    'ACBELL_RICA_PSU.json',
    'ASRock_E3C246D4I.json',
    'ASRock_ROMED8HM3.json',
    'Bletchley_Baseboard.json',
    'Bletchley_Chassis.json',
    'Blyth.json',
    'AHW1UM2RISER.json',
    'ASPOWER_U1A-D10550_PSU.json',
    'ASPOWER_U1A-D10800_PSU.json',
    'ASPOWER_U1A-D11200_PSU.json',
    'ASPOWER_U1A-D11600_PSU.json',
    'ASPOWER_U1D-D10800_PSU.json',
    'AXX1P100HSSI_AIC.json',
    'AXX2PRTHDHD.json',
    'BNP_Baseboard.json',
    'Bellavista.json',
    'Delta_AWF2DC3200W_PSU.json',
    'Delta_DPS-750XB_PSU.json',
    'Delta_DPS-1600AB_PSU.json',
    'Delta_DPS-2000AB_PSU.json',
    'Everest.json',
    'F1U12X25_HSBP.json',
    'F1U4X25_HSBP.json',
    'F2U12X35_HSBP.json',
    'F2U8X25_HSBP.json',
    'FBTP.json',
    'FBYV2.json',
    'Flextronics_S-1100ADU00-201_PSU.json',
    'IBM_1000W_CFFPS.json',
    'IBM_1400W_CFFPS.json',
    'IBM_1600W_CFFPS.json',
    'IBM_2000W_CFFPS.json',
    'IBM_2300W_CFFPS.json',
    'Intel_Front_Panel.json',
    'Kudo_BMC.json',
    'Kudo_Motherboard.json',
    'Mt_Jade.json',
    'Nisqually.json',
    'NVME_P4000.json',
    'PCIE_SSD_Retimer.json',
    'PSSF132202A.json',
    'PSSF162205A.json',
    'PSSF212201A.json',
    'PSSF222201A.json',
    'Rainier_2U_Chassis.json',
    'Rainier_4U_Chassis.json',
    'Rainier_1S4U_Chassis.json',
    'R1000_Chassis.json',
    'R2000_Chassis.json',
    'SAS_Module.json',
    'SOLUM_PSSF162202_PSU.json',
    'Storm_King.json',
    'STP_Baseboard.json',
    'STP_P4000_Chassis.json',
    'Tyan_S7106_Baseboard.json',
    'Tyan_S8036_Baseboard.json',
    'WFT_Baseboard.json',
]
filepaths = []
foreach c : configs
    file = join_paths('configurations', c)
    install_data(
        file,
        install_dir: join_paths(
            packagedir,
            'configurations',
        )
    )
    filepaths += [file]
endforeach

validate_script = files('scripts/validate-configs.py')
autojson = custom_target(
  'check_syntax',
  command: [
    validate_script,
    '-v',
    '-k',
  ],
  depend_files: files(filepaths),
  build_by_default: true,
  capture: true,
  output: 'validate_configs.log',
)

schemas = [
    'global.json',
    'legacy.json',
    'openbmc-dbus.json',
    'IBM.json',
    'Intel.json',
    'Pid.json',
    'Pid.Zone.json',
    'Stepwise.json',
    'VirtualSensor.json',
]

foreach s : schemas
    install_data(
        join_paths('schemas', s),
        install_dir: join_paths(
            packagedir,
            'configurations',
            'schemas',
        )
    )
endforeach

subdir('service_files')
subdir('src')

if not build_tests.disabled()
    test_boost_args = boost_args + ['-DBOOST_ASIO_DISABLE_THREADS']
    gtest = dependency('gtest', main: true, disabler: true, required: false)
    if not gtest.found() and build_tests.enabled()
        cmake = import('cmake')
        gtest_subproject = cmake.subproject('gtest')
        cm_gtest = gtest_subproject.dependency('gtest')
        cm_gtest_main = gtest_subproject.dependency('gtest_main')
        gtest = declare_dependency(dependencies: [cm_gtest, cm_gtest_main, threads])
    endif

    test(
        'test_entity_manager',
        executable(
            'test_entity_manager',
            'test/test_entity-manager.cpp',
            'src/expression.cpp',
            'src/utils.cpp',
            cpp_args: test_boost_args,
            dependencies: [
                boost,
                gtest,
                nlohmann_json_dep,
                sdbusplus,
                valijson,
            ],
            implicit_include_directories: false,
            include_directories: 'include',
        )
    )

    test(
        'test_fru_utils',
        executable(
            'test_fru_utils',
            'test/test_fru-utils.cpp',
            'src/fru_utils.cpp',
            cpp_args: test_boost_args,
            dependencies: [
                boost,
                gtest,
            ],
            implicit_include_directories: false,
            include_directories: 'include',
        )
    )
endif
