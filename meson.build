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
    meson_version: '>=0.58.0',
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

nlohmann_json_dep = dependency('nlohmann_json', include_type: 'system')

sdbusplus = dependency('sdbusplus', required: false)
if not sdbusplus.found()
    sdbusplus_proj = subproject('sdbusplus', required: true)
    sdbusplus = sdbusplus_proj.get_variable('sdbusplus_dep')
    sdbusplus = sdbusplus.as_system('system')
endif
systemd = dependency('systemd')
systemd_system_unit_dir = systemd.get_variable(
    'systemdsystemunitdir',
    pkgconfig_define: ['prefix', get_option('prefix')])
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
    '1ux16_riser.json',
    '2ux8_riser.json',
    '3ypower_vast2112_psu.json',
    '8x25_hsbp.json',
    'a2ul16riser.json',
    'a2ux8x4riser.json',
    'acbell_rica_psu.json',
    'acbel_r1ca2122a_psu.json',
    'asrock_c3_medium_x86.json',
    'asrock_e3c246d4i.json',
    'asrock_e3c256d4i.json',
    'asrock_m3_small_x86.json',
    'asrock_n3_xlarge_x86.json',
    'asrock_romed8hm3.json',
    'asrock_spc621d8hm3.json',
    'bletchley_baseboard.json',
    'bletchley_chassis.json',
    'bletchley_frontpanel.json',
    'blyth.json',
    'ahw1um2riser.json',
    'aspower_u1a-d10550_psu.json',
    'aspower_u1a-d10800_psu.json',
    'aspower_u1a-d11200_psu.json',
    'aspower_u1a-d11600_psu.json',
    'aspower_u1d-d10800_psu.json',
    'axx1p100hssi_aic.json',
    'axx2prthdhd.json',
    'bmc_storage_module.json',
    'bnp_baseboard.json',
    'bellavista.json',
    'blueridge_1s4u_chassis.json',
    'blueridge_2u_chassis.json',
    'blueridge_4u_chassis.json',
    'bonnell.json',
    'brcm_100g_1p_ocp_mezz.json',
    'brcm_200g_1p_ocp_mezz.json',
    'catalina_fio.json',
    'catalina_hdd.json',
    'catalina_osfp.json',
    'catalina_pdb.json',
    'catalina_scm.json',
    'cx7_ocp.json',
    'delta_awf2dc3200w_psu.json',
    'delta_dps-750xb_psu.json',
    'delta_dps-1600ab_psu.json',
    'delta_dps-2000ab_psu.json',
    'supermicro-pws-920p-sq_psu.json',
    'everest.json',
    'f1u12x25_hsbp.json',
    'f1u4x25_hsbp.json',
    'f2u12x35_hsbp.json',
    'f2u8x25_hsbp.json',
    'fbtp.json',
    'fbyv2.json',
    'fbyv35.json',
    'fbyv35_nic_mellanox.json',
    'flextronics_s-1100adu00-201_psu.json',
    'fp5280g3_chassis.json',
    'fp5280g3_fanboard.json',
    'fuji.json',
    'genesis3_psu.json',
    'genesis3_baseboard.json',
    'genesis3_chassis.json',
    'gospower_g1136-1300wna_psu.json',
    'greatlakes.json',
    'greatlakes_nic_mellanox.json',
    'harma_bsm.json',
    'harma_fanboard.json',
    'harma_mb.json',
    'harma_mb_vr_infineon.json',
    'harma_scm.json',
    'ibm_tacoma_rack_controller.json',
    'ingraham.json',
    'intel_front_panel.json',
    'kudo_bmc.json',
    'kudo_motherboard.json',
    'micron_7450.json',
    'minerva_cmm.json',
    'minerva_cmm_bsm.json',
    'minerva_cmm_scm.json',
    'minerva_fanboard.json',
    'minerva_pdb.json',
    'minerva_pttv.json',
    'minerva_sitv.json',
    'mori_bmc.json',
    'mori_motherboard.json',
    'mtjade.json',
    'mtmitchell_bmc.json',
    'mtmitchell_mb.json',
    'mudflap.json',
    'nf5280m7_baseboard.json',
    'nisqually.json',
    'nuvoton_npcm8xx_evb.json',
    'nvme_intel_p_series.json',
    'nvme_p4500_p5500.json',
    'pcie_ssd_retimer.json',
    'pennybacker.json',
    'pssf132202a.json',
    'pssf162205a.json',
    'pssf212201a.json',
    'pssf222201a.json',
    'rainier_2u_chassis.json',
    'rainier_4u_chassis.json',
    'rainier_1s4u_chassis.json',
    'r1000_chassis.json',
    'r2000_chassis.json',
    'sas_module.json',
    'sbp1_baseboard.json',
    'sbp1_chassis.json',
    'sbp1_nvme.json',
    'sbp1_psu.json',
    'sbp1_rssd.json',
    'sbp1_hbm.json',
    'system1_baseboard.json',
    'system1_chassis.json',
    'solum_pssf162202_psu.json',
    'storm_king.json',
    'stp_baseboard.json',
    'stp_p4000_chassis.json',
    'terminus_2x100g_nic_tsff.json',
    'tola.json',
    'twinlake.json',
    'tyan_s7106_baseboard.json',
    'tyan_s8036_baseboard.json',
    'vegman_n110_baseboard.json',
    'vegman_rx20_baseboard.json',
    'vegman_sx20_baseboard.json',
    'wft_baseboard.json',
    'yosemite4_fanboard_adc_ti.json',
    'yosemite4_fanboard_fsc_max_adc_ti.json',
    'yosemite4_fanboard_fsc_max.json',
    'yosemite4_fanboard_fsc_nct.json',
    'yosemite4_sentineldome.json',
    'yosemite4_floatingfalls.json',
    'yosemite4_wailuafalls.json',
    'yosemite4.json',
    'yosemite4_medusaboard.json',
    'yosemite4_spiderboard.json',
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

if get_option('validate-json')
    validate_script = files('scripts/validate_configs.py')
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
endif

schemas = [
    'global.json',
    'legacy.json',
    'openbmc-dbus.json',
    'ibm.json',
    'intel.json',
    'pid.json',
    'pid_zone.json',
    'stepwise.json',
    'virtual_sensor.json',
    'satellite_controller.json',
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
    gmock = dependency('gmock', disabler: true, required: false)
    if not (gtest.found() and gmock.found()) and build_tests.enabled()
        cmake = import('cmake')
        gtest_subproject = cmake.subproject('gtest')
        cm_gtest = gtest_subproject.dependency('gtest')
        cm_gtest_main = gtest_subproject.dependency('gtest_main')
        gtest = declare_dependency(dependencies: [cm_gtest, cm_gtest_main, threads])
        gmock = gtest_subproject.dependency('gmock')

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
            include_directories: 'src',
        )
    )

    test(
        'test_fru_utils',
        executable(
            'test_fru_utils',
            'test/test_fru-utils.cpp',
            'src/fru_utils.cpp',
            'src/fru_reader.cpp',
            cpp_args: test_boost_args,
            dependencies: [
                boost,
                gtest,
                sdbusplus,
            ],
            include_directories: 'src',
        )
    )

    test(
        'test_topology',
        executable(
            'test_topology',
            'test/test_topology.cpp',
            'src/topology.cpp',
            cpp_args: test_boost_args,
            dependencies: [
                gtest,
                gmock,
                nlohmann_json_dep,
            ],
            include_directories: 'src',
        )
    )
endif
