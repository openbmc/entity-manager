unit_files = [
    ['xyz.openbmc_project.FruDevice.service', 'fru-device'],
    ['xyz.openbmc_project.EntityManager.service', ''],
    ['devicetree-vpd-parser.service', 'devicetree-vpd'],
    ['xyz.openbmc_project.gpiopresence.service', 'gpio-presence'],
]

foreach u : unit_files
    if u[1] == '' or get_option(u[1])
        configure_file(
            copy: true,
            input: u[0],
            install: true,
            install_dir: systemd_system_unit_dir,
            output: u[0],
        )
    endif
endforeach

dbus_system_bus_services_dir = dependency('dbus-1').get_variable(
    'system_bus_services_dir',
    pkgconfig_define: ['prefix', get_option('prefix')],
)
install_data(
    'dbus/xyz.openbmc_project.EntityManager.service',
    install_dir: dbus_system_bus_services_dir,
)
