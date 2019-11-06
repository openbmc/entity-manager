project(
    'entity-manager',
    'cpp',
    default_options: [
        'warning_level=3',
        'werror=true',
        'cpp_std=c++17'
    ],
    license: 'Apache-2.0',
    version: '0.1',
)

boost_args = [
    '-DBOOST_SYSTEM_NO_DEPRECATED',
    '-DBOOST_ERROR_CODE_HEADER_ONLY',
    '-DBOOST_NO_RTTI',
    '-DBOOST_NO_TYPEID',
    '-DBOOST_ALL_NO_LIB',
]
build_tests = get_option('tests')
i2c = meson.get_compiler('cpp').find_library('i2c')
sdbusplus = dependency('sdbusplus')
systemd = dependency('systemd')
systemd_system_unit_dir = systemd.get_pkgconfig_variable(
    'systemdsystemunitdir',
    define_variable: ['prefix', get_option('prefix')])
packagedir = join_paths(
    get_option('datadir'),
    meson.project_name(),
)
threads = dependency('threads')

install_data('blacklist.json')

configs = [
    '1Ux16 Riser.json',
    '2Ux8 Riser.json',
    '8X25 HSBP.json',
    'A2UL16RISER.json',
    'A2UX8X4RISER.json',
    'AHW1UM2RISER.json',
    'AXX1P100HSSI_AIC.json',
    'AXX2PRTHDHD.json',
    'BNP Baseboard.json',
    'Delta DPS-750XB PSU.json',
    'F1U12X25 HSBP.json',
    'F1U4X25 HSBP.json',
    'F2U12X35 HSBP.json',
    'F2U8X25 HSBP.json',
    'FBTP.json',
    'Flextronics S-1100ADU00-201 PSU.json',
    'Intel Front Panel.json',
    'NVME P4000.json',
    'PCIE SSD Retimer.json',
    'PSSF132202A.json',
    'PSSF162205A.json',
    'PSSF212201A.json',
    'PSSF222201A.json',
    'R1000 Chassis.json',
    'R2000 Chassis.json',
    'SAS Module.json',
    'SOLUM_PSSF162202_PSU.json',
    'STP Baseboard.json',
    'STP P4000 Chassis.json',
]

foreach c : configs
    install_data(
        join_paths('configurations', c),
        install_dir: join_paths(
            packagedir,
            'configurations',
        )
    )
endforeach

schemas = [
    'global.json',
    'Pid.json',
    'Pid.Zone.json',
    'Stepwise.json',
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
subdir('test')
