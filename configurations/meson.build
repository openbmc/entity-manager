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
        c,
        install_dir: join_paths(
            packagedir,
            'configurations',
        )
    )
endforeach
