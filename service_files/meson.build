unit_files = [
    'xyz.openbmc_project.FruDevice.service',
    'xyz.openbmc_project.EntityManager.service',
]

foreach u : unit_files
    configure_file(
        copy: true,
	input: u,
	install: true,
	install_dir: systemd_system_unit_dir,
	output: u,
    )
endforeach
