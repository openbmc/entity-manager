schemas = [
    'global.json',
    'Pid.json',
    'Pid.Zone.json',
    'Stepwise.json',
]

foreach s : schemas
    install_data(
        s,
        install_dir: join_paths(
            packagedir,
            'configurations',
            'schemas',
        )
    )
endforeach
