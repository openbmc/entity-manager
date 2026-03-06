#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
Verify that every FruDevice probe statement includes a Manufacturer check
and that the manufacturer value matches the vendor folder the config lives in.
"""

import json
import os
import re
import sys

# The following is a list of config files that currently fail tests.  The
# intent is that these are slowly fixed over time, and this list can be
# removed.
known_violations = [
    "3y-power/3ypower_vast2112_psu.json",
    "acbel/r1ca2122a_psu.json",
    "acbel/rica_psu.json",
    "ampere/mtjade.json",
    "ampere/mtjefferson_bmc.json",
    "ampere/mtjefferson_bp.json",
    "ampere/mtjefferson_mb.json",
    "ampere/mtjefferson_riser.json",
    "ampere/mtmitchell_bmc.json",
    "ampere/mtmitchell_bp.json",
    "ampere/mtmitchell_mb.json",
    "ampere/mtmitchell_riser.json",
    "aspower/u1a-d10550_psu.json",
    "aspower/u1a-d10800_psu.json",
    "aspower/u1a-d11200_psu.json",
    "aspower/u1a-d11600_psu.json",
    "aspower/u1d-d10800_psu.json",
    "asrock/c3_medium_x86.json",
    "asrock/e3c246d4i.json",
    "asrock/e3c256d4i.json",
    "asrock/m3_small_x86.json",
    "asrock/n3_xlarge_x86.json",
    "asrock/nf5280m7_baseboard.json",
    "asrock/romed8hm3.json",
    "asrock/spc621d8hm3.json",
    "broadcomm/100g_1p_ocp_mezz.json",
    "broadcomm/200g_1p_ocp_mezz.json",
    "delta/awf2dc3200w_psu.json",
    "delta/dps-1600ab_psu.json",
    "delta/dps-2000ab_psu.json",
    "delta/dps-750xb_psu.json",
    "flextronics/s-1100adu00-201_psu.json",
    "foxconn-industrial-internet/kudo_bmc.json",
    "foxconn-industrial-internet/kudo_motherboard.json",
    "foxconn-industrial-internet/mori_bmc.json",
    "foxconn-industrial-internet/mori_motherboard.json",
    "gigabyte/msx4_mg1.json",
    "gospower/g1136-1300wna_psu.json",
    "ibm/genesis3_baseboard.json",
    "ibm/genesis3_chassis.json",
    "ibm/mudflap.json",
    "ibm/sbp1_baseboard.json",
    "ibm/sbp1_chassis.json",
    "ibm/sbp1_hbm.json",
    "ibm/sbp1_nvme.json",
    "ibm/sbp1_rssd.json",
    "intel/1ux16_riser.json",
    "intel/2ux8_riser.json",
    "intel/8x25_hsbp.json",
    "intel/a2ul16riser.json",
    "intel/a2ux8x4riser.json",
    "intel/ahw1um2riser.json",
    "intel/axx1p100hssi_aic.json",
    "intel/axx2prthdhd.json",
    "intel/bnp_baseboard.json",
    "intel/f1u12x25_hsbp.json",
    "intel/f1u4x25_hsbp.json",
    "intel/f2u12x35_hsbp.json",
    "intel/f2u8x25_hsbp.json",
    "intel/front_panel.json",
    "intel/pcie_ssd_retimer.json",
    "intel/r1000_chassis.json",
    "intel/r2000_chassis.json",
    "intel/stp_baseboard.json",
    "intel/stp_p4000_chassis.json",
    "intel/wft_baseboard.json",
    "meta/anacapa/anacapa_bridge_l_t16.json",
    "meta/anacapa/anacapa_bridge_l_t20.json",
    "meta/anacapa/anacapa_bridge_r_t16.json",
    "meta/anacapa/anacapa_bridge_r_t20.json",
    "meta/anacapa/anacapa_mb.json",
    "meta/anacapa/anacapa_pdb_l.json",
    "meta/anacapa/anacapa_pdb_l_evt1.json",
    "meta/anacapa/anacapa_pdb_r.json",
    "meta/anacapa/anacapa_pdb_r_evt1.json",
    "meta/anacapa/anacapa_scm.json",
    "meta/bletchley/bletchley_baseboard.json",
    "meta/bletchley15/bletchley15_baseboard.json",
    "meta/bletchley15/bletchley15_chassis.json",
    "meta/bmc_storage_module.json",
    "meta/catalina/catalina_fio.json",
    "meta/catalina/catalina_hdd_adc_ina.json",
    "meta/catalina/catalina_hdd_adc_isl.json",
    "meta/catalina/catalina_osfp.json",
    "meta/catalina/catalina_pdb_hsc_ltc_fsc_max_vr_delta_gndsen_ina_p12vsen_ina_p12vfan_mps.json",
    "meta/catalina/catalina_pdb_hsc_ltc_fsc_max_vr_raa_gndsen_ina_p12vsen_ina_p12vfan_mps.json",
    "meta/catalina/catalina_pdb_hsc_xdp_fsc_nct_vr_raa_gndsen_max_p12vsen_isl_p12vfan_rtt.json",
    "meta/catalina/catalina_scm.json",
    "meta/clemente/clemente_fio.json",
    "meta/clemente/clemente_hdd.json",
    "meta/clemente/clemente_hdd_nvme.json",
    "meta/clemente/clemente_interposer.json",
    "meta/clemente/clemente_osfp.json",
    "meta/clemente/clemente_pdb.json",
    "meta/clemente/clemente_scm.json",
    "meta/fbtp.json",
    "meta/fbyv2.json",
    "meta/fbyv35.json",
    "meta/greatlakes.json",
    "meta/harma/harma_bsm.json",
    "meta/harma/harma_mb.json",
    "meta/harma/harma_mb_vr_infineon.json",
    "meta/harma/harma_scm.json",
    "meta/minerva/minerva_aegis.json",
    "meta/minerva/minerva_cmm.json",
    "meta/minerva/minerva_cmm_bsm.json",
    "meta/minerva/minerva_cmm_hsc_infineon.json",
    "meta/minerva/minerva_cmm_scm.json",
    "meta/minerva/minerva_fanboard_adc_silergy.json",
    "meta/minerva/minerva_fanboard_adc_ti.json",
    "meta/minerva/minerva_janga_smb.json",
    "meta/minerva/minerva_pdb.json",
    "meta/minerva/minerva_pdb_hsc_xdp.json",
    "meta/minerva/minerva_pttv.json",
    "meta/minerva/minerva_sitv.json",
    "meta/minerva/minerva_tahan_smb.json",
    "meta/santabarbara/santabarbara_e1s_bp.json",
    "meta/santabarbara/santabarbara_evb.json",
    "meta/santabarbara/santabarbara_evb_pdb2.json",
    "meta/santabarbara/santabarbara_mb.json",
    "meta/santabarbara/santabarbara_mb_evt1.json",
    "meta/santabarbara/santabarbara_mb_vr_rtt.json",
    "meta/santabarbara/santabarbara_pcie_switch_board.json",
    "meta/santabarbara/santabarbara_pdb1.json",
    "meta/santabarbara/santabarbara_pdb1_vr_sni.json",
    "meta/santabarbara/santabarbara_pdb2.json",
    "meta/santabarbara/santabarbara_pttv.json",
    "meta/santabarbara/santabarbara_rainbow.json",
    "meta/santabarbara/santabarbara_scm.json",
    "meta/santabarbara/santabarbara_sitv_eth.json",
    "meta/santabarbara/santabarbara_sitv_pcie.json",
    "meta/terminus_2x100g_nic_tsff.json",
    "meta/ventura/ventura_fanboard_adc_max.json",
    "meta/ventura/ventura_fanboard_adc_ocp_max.json",
    "meta/ventura/ventura_fanboard_adc_ocp_tic.json",
    "meta/ventura/ventura_fanboard_adc_tic.json",
    "meta/ventura/ventura_ioboard.json",
    "meta/ventura/ventura_ledboard.json",
    "meta/ventura/ventura_rmc_hsc_ltc_fsc_max_p24vsen_ina_gndsen_ina_p12vbrick_delta.json",
    "meta/ventura/ventura_rmc_hsc_xdp_fsc_nct_p24vsen_isl_gndsen_isl_p12vbrick_flex.json",
    "meta/ventura/ventura_scm.json",
    "meta/ventura2/ventura2_aalc.json",
    "meta/ventura2/ventura2_canbus.json",
    "meta/ventura2/ventura2_fan_tic.json",
    "meta/ventura2/ventura2_leak_legacy.json",
    "meta/ventura2/ventura2_management.json",
    "meta/ventura2/ventura2_minisas_bottom.json",
    "meta/ventura2/ventura2_minisas_top.json",
    "meta/ventura2/ventura2_rmcv2_mb.json",
    "meta/ventura2/ventura2_scm.json",
    "meta/ventura2/ventura2_vt1_2.json",
    "meta/ventura2/ventura2_vt3.json",
    "meta/yv4/fanboard/yosemite4_fanboard_fsc_max_adc_ti_led_nxp_ons_efuse_max.json",
    "meta/yv4/fanboard/yosemite4_fanboard_fsc_max_adc_ti_led_nxp_ons_efuse_mps.json",
    "meta/yv4/fanboard/yosemite4_fanboard_fsc_nct_adc_max_led_nxp_ons_efuse_max.json",
    "meta/yv4/fanboard/yosemite4_fanboard_fsc_nct_adc_max_led_nxp_ons_efuse_mps.json",
    "meta/yv4/medusaboard/yosemite4_medusaboard_adc_rns_12vhsc_adi_48vhsc_inf.json",
    "meta/yv4/medusaboard/yosemite4_medusaboard_adc_rns_12vhsc_mps_48vhsc_adi.json",
    "meta/yv4/medusaboard/yosemite4_medusaboard_adc_rns_12vhsc_mps_48vhsc_inf.json",
    "meta/yv4/medusaboard/yosemite4_medusaboard_adc_rns_hsc_adi.json",
    "meta/yv4/medusaboard/yosemite4_medusaboard_adc_rns_isl_12vhsc_adi_48vhsc_inf.json",
    "meta/yv4/medusaboard/yosemite4_medusaboard_adc_rns_isl_12vhsc_mps_48vhsc_adi.json",
    "meta/yv4/medusaboard/yosemite4_medusaboard_adc_rns_isl_12vhsc_mps_48vhsc_inf.json",
    "meta/yv4/medusaboard/yosemite4_medusaboard_adc_rns_isl_hsc_adi.json",
    "meta/yv4/medusaboard/yosemite4_medusaboard_adc_silergy_12vhsc_adi_48vhsc_inf.json",
    "meta/yv4/medusaboard/yosemite4_medusaboard_adc_silergy_12vhsc_mps_48vhsc_adi.json",
    "meta/yv4/medusaboard/yosemite4_medusaboard_adc_silergy_12vhsc_mps_48vhsc_inf.json",
    "meta/yv4/medusaboard/yosemite4_medusaboard_adc_silergy_hsc_adi.json",
    "meta/yv4/medusaboard/yosemite4_medusaboard_adc_ti_12vhsc_adi_48vhsc_inf.json",
    "meta/yv4/medusaboard/yosemite4_medusaboard_adc_ti_12vhsc_mps_48vhsc_adi.json",
    "meta/yv4/medusaboard/yosemite4_medusaboard_adc_ti_12vhsc_mps_48vhsc_inf.json",
    "meta/yv4/medusaboard/yosemite4_medusaboard_adc_ti_hsc_adi.json",
    "meta/yv4/spiderboard/yosemite4_spiderboard_adc_max_pwr_rt.json",
    "meta/yv4/spiderboard/yosemite4_spiderboard_adc_max_pwr_silergy.json",
    "meta/yv4/spiderboard/yosemite4_spiderboard_adc_max_pwr_ti.json",
    "meta/yv4/spiderboard/yosemite4_spiderboard_adc_ti_pwr_rt.json",
    "meta/yv4/spiderboard/yosemite4_spiderboard_adc_ti_pwr_silergy.json",
    "meta/yv4/spiderboard/yosemite4_spiderboard_adc_ti_pwr_ti.json",
    "meta/yv4/yosemite4.json",
    "meta/yv4/yosemite4_chassis.json",
    "meta/yv4/yosemite4_cpu.json",
    "meta/yv4/yosemite4_floatingfalls.json",
    "meta/yv4/yosemite4_sentineldome_chassis.json",
    "meta/yv4/yosemite4_sentineldome_t1.json",
    "meta/yv4/yosemite4_sentineldome_t1_retimer.json",
    "meta/yv4/yosemite4_sentineldome_t2.json",
    "meta/yv4/yosemite4_sentineldome_t2_retimer.json",
    "meta/yv4/yosemite4_wailuafalls.json",
    "meta/yv4/yosemite4n.json",
    "meta/yv5/yosemite5_1kw_paddle_board.json",
    "meta/yv5/yosemite5_cxl_ddr4_board_vr_mps.json",
    "meta/yv5/yosemite5_cxl_ddr4_board_vr_raa.json",
    "meta/yv5/yosemite5_cxl_ddr5_board_vr_mps.json",
    "meta/yv5/yosemite5_cxl_ddr5_board_vr_raa.json",
    "meta/yv5/yosemite5_cxl_ddr5_evt_board_vr_raa.json",
    "meta/yv5/yosemite5_e1s_expansion_board_adc_qns_pmon_sgy.json",
    "meta/yv5/yosemite5_e1s_expansion_board_adc_tic_pmon_tic.json",
    "meta/yv5/yosemite5_e1s_x4_expansion_board_adc_ons_pmon_rtk.json",
    "meta/yv5/yosemite5_e1s_x4_expansion_board_adc_tic_pmon_tic.json",
    "meta/yv5/yosemite5_mb_vr_mps_pvdd18vr_mps_adc_tic_i3chub_rtt.json",
    "meta/yv5/yosemite5_mb_vr_rtt_pvdd18vr_mps_adc_tic_i3chub_rtt.json",
    "meta/yv5/yosemite5_mb_vr_rtt_pvdd18vr_sni_adc_tic_i3chub_rtt.json",
    "meta/yv5/yosemite5_mb_vr_sni_pvdd18vr_sni_adc_tic_i3chub_rtt.json",
    "meta/yv5/yosemite5_medusa_board_brick.json",
    "meta/yv5/yosemite5_medusa_board_discrete.json",
    "meta/yv5/yosemite5_scm.json",
    "meta/yv5/yosemite5_scm_2700.json",
    "micron/7450.json",
    "nuvoton/npcm8xx_evb.json",
    "ocp/cx7_ocp.json",
    "ocp/cx8_ocp.json",
    "solum/pssf132202a.json",
    "solum/pssf162202_psu.json",
    "solum/pssf162205a.json",
    "solum/pssf212201a.json",
    "solum/pssf222201a.json",
    "supermicro/pws-920p-sq_psu.json",
    "tyan/s7106_baseboard.json",
    "tyan/s8036_baseboard.json",
    "yadro/vegman_n110_baseboard.json",
    "yadro/vegman_rx20_baseboard.json",
    "yadro/vegman_sx20_baseboard.json",
]


def remove_c_comments(string):
    pattern = r"(\".*?(?<!\\)\"|\'.*?(?<!\\)\')|(/\*.*?\*/|//[^\r\n]*$)"
    regex = re.compile(pattern, re.MULTILINE | re.DOTALL)

    def _replacer(match):
        if match.group(2) is not None:
            return ""
        else:
            return match.group(1)

    return regex.sub(_replacer, string)


MANUFACTURER_RE = re.compile(
    r"(?:BOARD_MANUFACTURER|PRODUCT_MANUFACTURER)'\s*:\s*'([^']+)'"
)


def _normalize(name):
    return re.sub(r"[^a-z0-9]", "", name.lower())


def _extract_alternatives(value):
    """Split a regex-style '(A|B|C)' value into individual names."""
    stripped = value.strip()
    stripped = re.sub(r"^\((.+)\)$", r"\1", stripped)
    stripped = re.sub(r"\.\*$", "", stripped)
    return [part.strip() for part in stripped.split("|") if part.strip()]


def manufacturer_matches_folder(manufacturer_value, folder_name):
    norm_folder = _normalize(folder_name)
    allowed_manufacturers = [norm_folder]

    # Meta gets their systems manufacturered by other ODMs, which doesn't fit
    # the 1:1 mapping.  Meta also has active maintainers that are capable of
    #  keeping this mapping in check.
    if folder_name == "meta":
        allowed_manufacturers.append("wiwynn")
        allowed_manufacturers.append("quanta")

    for alt in _extract_alternatives(manufacturer_value):
        norm_alt = _normalize(alt)
        for allowed_manufacturer in allowed_manufacturers:
            if allowed_manufacturer in norm_alt:
                return True
    return False


def get_vendor_folder(config_file, configs_dir):
    """Return the top-level vendor folder name for a config file."""
    rel = os.path.relpath(config_file, configs_dir)
    return rel.split(os.sep)[0]


def probe_has_model(probe_str):
    MODEL_KEYS = {"BOARD_MODEL", "PRODUCT_MODEL", "PRODUCT_NAME", "BOARD_NAME"}
    return any(key in probe_str for key in MODEL_KEYS)


def get_entries(config):
    """Normalize config into a flat list of entry dicts."""
    if isinstance(config, dict):
        return [config]
    if isinstance(config, list):
        return [item for item in config if isinstance(item, dict)]
    return []


def check_probes(config_file, config, configs_dir):
    failures = []
    vendor_folder = get_vendor_folder(config_file, configs_dir)

    config_file_relative = os.path.relpath(config_file, configs_dir)

    for entry in get_entries(config):
        probe = entry.get("Probe")
        if probe is None:
            continue

        probe_strings = [probe] if isinstance(probe, str) else probe

        for probe_str in probe_strings:
            if not isinstance(probe_str, str):
                continue
            if "xyz.openbmc_project.FruDevice" not in probe_str:
                continue

            name = entry["Name"]

            if not probe_has_model(probe_str):
                failures.append(
                    (
                        config_file,
                        f"{config_file_relative}: '{name}' has FruDevice probe "
                        f"without a Model check: {probe_str}",
                    )
                )
                continue

            manufacturers = MANUFACTURER_RE.finditer(probe_str)
            manufacturer_found = False

            for match in manufacturers:
                mfr_value = match.group(1)
                manufacturer_found = True
                if not manufacturer_matches_folder(mfr_value, vendor_folder):
                    failures.append(
                        (
                            config_file,
                            f"{config_file_relative}: '{name}' has manufacturer "
                            f"'{mfr_value}' which does not match folder "
                            f"'{vendor_folder}'",
                        )
                    )
            if not manufacturer_found:
                failures.append(
                    (
                        config_file,
                        f"{config_file_relative}: '{name}' has FruDevice probe "
                        f"without a Manufacturer check: {probe_str}",
                    )
                )
                continue
    return failures


def main():
    source_dir = os.path.realpath(__file__).split(os.sep)[:-2]
    configs_dir = os.sep + os.path.join(*source_dir, "configurations")

    config_files = []
    for root, _, files in os.walk(configs_dir):
        for f in files:
            if f.endswith(".json"):
                config_files.append(os.path.join(root, f))

    all_failures = []

    for config_file in sorted(config_files):
        with open(config_file) as fd:
            config = json.loads(remove_c_comments(fd.read()))

        all_failures.extend(check_probes(config_file, config, configs_dir))

    if all_failures:
        failed = False
        for config_file, msg in all_failures:
            print(msg, file=sys.stderr)

            if config_file not in known_violations:
                continue
            failed = True
        if failed:
            raise SystemExit("Validation failed.")

    print(
        f"Validation complete.  Checked {len(config_files)} files.  {len(all_failures)} failures."
    )


if __name__ == "__main__":
    main()
