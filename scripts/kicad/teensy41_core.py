#!/usr/bin/env python3
"""Spin up a SKiDL netlist for a Teensy 4.1 style core with on-board silicon.

This generator riffs off Jens Chr. Brynildsen's open hardware reference
(<https://github.com/jenschr/Teensy-4.1-example>) so we can prototype the
"Teensy guts + SGTL5000" dream without juggling multiple dev boards. It drops
in the i.MX RT1062, the PJRC bootloader MCU, the QSPI flash, USB-C for power,
and the debug/programming headers you need to bring the board to life.

A few guiding principles:

* Keep the signal names readable so they line up with the firmware and our
  audio-front-end harness (`MCLK`, `BCLK`, etc.).
* Bring every breakout pin into 2xN headers so you can hand-route or tweak the
  connector placement later.
* Provide stub symbols for headless previews, but nag loudly so real builds
  still use KiCad's official libraries.

Usage::

    python scripts/kicad/teensy41_core.py \\
        --output build/hw/teensy41_core.net

Pair this netlist with ``sgtl5000_frontend.py`` if you want the codec wired in
from the jump. You can import :func:`build_core` from another SKiDL script when
stitching a combined layout.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from skidl import (
    ERC,
    KICAD,
    POWER,
    SKIDL,
    Net,
    Part,
    Pin,
    TEMPLATE,
    generate_netlist,
    set_default_tool,
)

set_default_tool(KICAD)


def _power_net(name: str) -> Net:
    net = Net(name)
    net.drive = POWER
    return net


def _label_pin(pin, label: str) -> None:
    if hasattr(pin, "alias"):
        pin.alias(label)
    else:
        pin.name = label


_COMMON_STUBS: dict[tuple[str, str], dict[str, object]] = {
    ("power", "+5V"): {"pins": [Pin(num="1", name="VCC")], "ref_prefix": "PWR"},
    ("power", "+3V3"): {"pins": [Pin(num="1", name="3V3")], "ref_prefix": "PWR"},
    ("power", "GND"): {"pins": [Pin(num="1", name="GND")], "ref_prefix": "PWR"},
    ("power", "PWR_FLAG"): {"pins": [Pin(num="1", name="PWR")], "ref_prefix": "PWR"},
    ("Connector_Generic", "Conn_02x20_Odd_Even"): {
        "pins": [
            Pin(num=str(idx), name=f"Pin_{idx}")
            for idx in range(1, 41)
        ],
        "ref_prefix": "J",
    },
    ("Connector_Generic", "Conn_02x07_Odd_Even"): {
        "pins": [
            Pin(num=str(idx), name=f"Pin_{idx}")
            for idx in range(1, 15)
        ],
        "ref_prefix": "J",
    },
    ("Connector_Generic", "Conn_01x06"): {
        "pins": [
            Pin(num=str(idx), name=f"Pin_{idx}")
            for idx in range(1, 7)
        ],
        "ref_prefix": "J",
    },
    ("Connector_USB", "USB_C_Receptacle_USB2.0"): {
        "pins": [
            Pin(num="A1", name="VBUS"),
            Pin(num="A4", name="VBUS"),
            Pin(num="A5", name="CC1"),
            Pin(num="A6", name="D+"),
            Pin(num="A7", name="D-"),
            Pin(num="A8", name="SBU1"),
            Pin(num="A9", name="VBUS"),
            Pin(num="A12", name="GND"),
            Pin(num="B1", name="VBUS"),
            Pin(num="B4", name="VBUS"),
            Pin(num="B5", name="CC2"),
            Pin(num="B6", name="D+"),
            Pin(num="B7", name="D-"),
            Pin(num="B8", name="SBU2"),
            Pin(num="B9", name="VBUS"),
            Pin(num="B12", name="GND"),
            Pin(num="SH", name="SHIELD"),
        ],
        "ref_prefix": "J",
    },
    ("Device", "R"): {
        "pins": [Pin(num="1", name="1"), Pin(num="2", name="2")],
        "ref_prefix": "R",
    },
    ("Device", "C"): {
        "pins": [Pin(num="1", name="1"), Pin(num="2", name="2")],
        "ref_prefix": "C",
    },
    ("Device", "L"): {
        "pins": [Pin(num="1", name="1"), Pin(num="2", name="2")],
        "ref_prefix": "L",
    },
}

_STUB_PARTS: dict[tuple[str, str], dict[str, object]] = {
    **_COMMON_STUBS,
    ("MCU_NXP", "MIMXRT1062DVJ6A"): {
        "pins": [
            Pin(num="1", name="VIN"),
            Pin(num="2", name="3V3_CORE"),
            Pin(num="3", name="GND0"),
            Pin(num="4", name="USB_DM"),
            Pin(num="5", name="USB_DP"),
            Pin(num="6", name="USBH_DM"),
            Pin(num="7", name="USBH_DP"),
            Pin(num="8", name="BOOT_MODE0"),
            Pin(num="9", name="BOOT_MODE1"),
            Pin(num="10", name="PROGRAM"),
            Pin(num="11", name="RESET_B"),
            Pin(num="12", name="ON_OFF"),
            Pin(num="13", name="SWDIO"),
            Pin(num="14", name="SWDCLK"),
            Pin(num="15", name="QSPI_SCK"),
            Pin(num="16", name="QSPI_CS"),
            Pin(num="17", name="QSPI_DATA0"),
            Pin(num="18", name="QSPI_DATA1"),
            Pin(num="19", name="QSPI_DATA2"),
            Pin(num="20", name="QSPI_DATA3"),
            Pin(num="21", name="MCLK"),
            Pin(num="22", name="BCLK"),
            Pin(num="23", name="LRCLK"),
            Pin(num="24", name="I2S_TX"),
            Pin(num="25", name="I2S_RX"),
            Pin(num="26", name="I2C_SCL"),
            Pin(num="27", name="I2C_SDA"),
            Pin(num="28", name="AUDIO_MUTE"),
            Pin(num="29", name="PIO00"),
            Pin(num="30", name="PIO01"),
            Pin(num="31", name="PIO02"),
            Pin(num="32", name="PIO03"),
            Pin(num="33", name="PIO04"),
            Pin(num="34", name="PIO05"),
            Pin(num="35", name="PIO06"),
            Pin(num="36", name="PIO07"),
            Pin(num="37", name="PIO08"),
            Pin(num="38", name="PIO09"),
            Pin(num="39", name="PIO10"),
            Pin(num="40", name="PIO11"),
            Pin(num="41", name="PIO12"),
            Pin(num="42", name="PIO13"),
            Pin(num="43", name="PIO14"),
            Pin(num="44", name="PIO15"),
            Pin(num="45", name="PIO16"),
            Pin(num="46", name="PIO17"),
            Pin(num="47", name="PIO18"),
            Pin(num="48", name="PIO19"),
            Pin(num="49", name="PIO20"),
            Pin(num="50", name="PIO21"),
            Pin(num="51", name="PIO22"),
            Pin(num="52", name="PIO23"),
            Pin(num="53", name="PIO24"),
            Pin(num="54", name="PIO25"),
            Pin(num="55", name="PIO26"),
            Pin(num="56", name="PIO27"),
            Pin(num="57", name="PIO28"),
            Pin(num="58", name="PIO29"),
            Pin(num="59", name="PIO30"),
            Pin(num="60", name="PIO31"),
            Pin(num="61", name="PIO32"),
            Pin(num="62", name="PIO33"),
            Pin(num="63", name="PIO34"),
            Pin(num="64", name="PIO35"),
            Pin(num="65", name="PIO36"),
            Pin(num="66", name="PIO37"),
            Pin(num="67", name="PIO38"),
            Pin(num="68", name="PIO39"),
            Pin(num="69", name="PIO40"),
            Pin(num="70", name="PIO41"),
            Pin(num="71", name="PIO42"),
            Pin(num="72", name="PIO43"),
            Pin(num="73", name="PIO44"),
            Pin(num="74", name="PIO45"),
            Pin(num="75", name="PIO46"),
            Pin(num="76", name="PIO47"),
            Pin(num="77", name="PIO48"),
            Pin(num="78", name="PIO49"),
            Pin(num="79", name="PIO50"),
            Pin(num="80", name="PIO51"),
            Pin(num="81", name="PIO52"),
            Pin(num="82", name="PIO53"),
            Pin(num="83", name="PIO54"),
            Pin(num="84", name="PIO55"),
            Pin(num="85", name="PIO56"),
            Pin(num="86", name="PIO57"),
            Pin(num="87", name="PIO58"),
            Pin(num="88", name="PIO59"),
            Pin(num="89", name="PIO60"),
            Pin(num="90", name="PIO61"),
            Pin(num="91", name="PIO62"),
            Pin(num="92", name="PIO63"),
            Pin(num="93", name="PIO64"),
            Pin(num="94", name="PIO65"),
            Pin(num="95", name="PIO66"),
            Pin(num="96", name="PIO67"),
        ],
        "ref_prefix": "U",
        "name": "MIMXRT1062_STUB",
    },
    ("MCU_NXP", "MKL02Z32VFG4"): {
        "pins": [
            Pin(num="1", name="VDD"),
            Pin(num="2", name="GND"),
            Pin(num="3", name="SWDIO"),
            Pin(num="4", name="SWDCLK"),
            Pin(num="5", name="RESET_B"),
            Pin(num="6", name="PROG"),
            Pin(num="7", name="ON_OFF"),
            Pin(num="8", name="PIO_A"),
            Pin(num="9", name="PIO_B"),
            Pin(num="10", name="PIO_C"),
        ],
        "ref_prefix": "U",
        "name": "MKL02_BOOT_STUB",
    },
    ("Memory_Flash", "W25Q64JV"): {
        "pins": [
            Pin(num="1", name="/CS"),
            Pin(num="2", name="DO"),
            Pin(num="3", name="/WP"),
            Pin(num="4", name="GND"),
            Pin(num="5", name="DI"),
            Pin(num="6", name="CLK"),
            Pin(num="7", name="/HOLD"),
            Pin(num="8", name="VCC"),
        ],
        "ref_prefix": "U",
        "name": "W25Q64_STUB",
    },
    ("Connector_Generic", "Conn_01x04"): {
        "pins": [
            Pin(num=str(idx), name=f"Pin_{idx}")
            for idx in range(1, 5)
        ],
        "ref_prefix": "J",
    },
}


def _make_stub_part(lib: str, name: str, **kwargs) -> Part:
    spec = _STUB_PARTS.get((lib, name))
    if not spec:
        raise RuntimeError(
            "Unable to load symbol {}:{}; install KiCad libraries or extend the "
            "stub table.".format(lib, name)
        )

    part_kwargs: dict[str, object] = {
        "tool": SKIDL,
        "name": spec.get("name", name),
        "ref_prefix": spec.get("ref_prefix"),
        "pins": spec["pins"],
    }

    if "value" in kwargs:
        part_kwargs["value"] = kwargs["value"]
    if "footprint" in kwargs:
        part_kwargs["footprint"] = kwargs["footprint"]

    part = Part(**part_kwargs)
    part.dest = kwargs.get("dest", TEMPLATE)
    part.footprint = kwargs.get("footprint", "~")

    print(
        f"[stub-lib] Falling back to internal symbol for {lib}:{name}; install KiCad libraries for production runs.",
        file=sys.stderr,
    )
    return part


def _require_part(*args, **kwargs) -> Part:
    try:
        return Part(*args, **kwargs)
    except Exception:
        lib = args[0] if args else "<unknown>"
        name = args[1] if len(args) > 1 else kwargs.get("name", "<unknown>")
        return _make_stub_part(lib, name, **kwargs)


def build_core() -> dict[str, object]:
    vusb = _power_net("VUSB")
    v5 = _power_net("USB5V_SWITCHED")
    v3v3 = _power_net("3V3_MAIN")
    vbat = _power_net("VBAT")
    gnd = Net("GND")
    gnd.drive = POWER

    pwr_flag_vusb = _require_part("power", "PWR_FLAG")
    pwr_flag_vusb[1] += vusb
    pwr_flag_v5 = _require_part("power", "PWR_FLAG")
    pwr_flag_v5[1] += v5
    pwr_flag_v3 = _require_part("power", "PWR_FLAG")
    pwr_flag_v3[1] += v3v3

    gnd_flag = _require_part("power", "GND")
    gnd_flag[1] += gnd

    # === Core silicon ===
    mcu = _require_part(
        "MCU_NXP",
        "MIMXRT1062DVJ6A",
        footprint="Package_QFP:LQFP-144_20x20mm_P0.5mm",
        dest=TEMPLATE,
    )

    boot_mcu = _require_part(
        "MCU_NXP",
        "MKL02Z32VFG4",
        footprint="Package_QFN:QFN-16-1EP_3x3mm_P0.5mm",
        dest=TEMPLATE,
    )

    qspi = _require_part(
        "Memory_Flash",
        "W25Q64JV",
        footprint="Package_SO:SOIC-8_3.9x4.9mm_P1.27mm",
        dest=TEMPLATE,
    )

    usb = _require_part(
        "Connector_USB",
        "USB_C_Receptacle_USB2.0",
        footprint="Connector_USB:USB_C_Receptacle_MidMount",
    )

    usb_host = _require_part(
        "Connector_Generic",
        "Conn_01x06",
        footprint="Connector_PinHeader_2.54mm:PinHeader_1x06_P2.54mm_Vertical",
    )
    for idx, label in enumerate(["VBUS", "D+", "D-", "ID", "SHIELD", "GND"], start=1):
        _label_pin(usb_host[idx], label)

    # === Headers ===
    left_header = _require_part(
        "Connector_Generic",
        "Conn_02x20_Odd_Even",
        footprint="Connector_PinHeader_2.54mm:PinHeader_2x20_P2.54mm_Vertical",
    )
    right_header = _require_part(
        "Connector_Generic",
        "Conn_02x20_Odd_Even",
        footprint="Connector_PinHeader_2.54mm:PinHeader_2x20_P2.54mm_Vertical",
    )
    underside_header = _require_part(
        "Connector_Generic",
        "Conn_02x07_Odd_Even",
        footprint="Connector_PinHeader_2.54mm:PinHeader_2x07_P2.54mm_Vertical",
    )

    jtag = _require_part(
        "Connector_Generic",
        "Conn_01x04",
        footprint="Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical",
    )
    for idx, label in enumerate(["SWDIO", "SWCLK", "RESET_B", "GND"], start=1):
        _label_pin(jtag[idx], label)

    # === Passives ===
    cc1 = _require_part("Device", "R", value="5.1k", footprint="Resistor_SMD:R_0603_1608Metric")
    cc2 = _require_part("Device", "R", value="5.1k", footprint="Resistor_SMD:R_0603_1608Metric")
    boot0_pull = _require_part("Device", "R", value="10k", footprint="Resistor_SMD:R_0603_1608Metric")
    boot1_pull = _require_part("Device", "R", value="10k", footprint="Resistor_SMD:R_0603_1608Metric")
    program_pull = _require_part("Device", "R", value="10k", footprint="Resistor_SMD:R_0603_1608Metric")
    reset_pull = _require_part("Device", "R", value="10k", footprint="Resistor_SMD:R_0603_1608Metric")
    usb_host_id_pull = _require_part("Device", "R", value="100k", footprint="Resistor_SMD:R_0603_1608Metric")
    vusb_filter = _require_part(
        "Device", "L", value="FB", footprint="Inductor_SMD:L_0805_2012Metric"
    )
    vbat_cap = _require_part("Device", "C", value="10u", footprint="Capacitor_SMD:C_1206_3216Metric")
    v3v3_caps = [
        _require_part("Device", "C", value="100n", footprint="Capacitor_SMD:C_0603_1608Metric")
        for _ in range(3)
    ]

    # === Nets ===
    usb_dp = Net("USB_DP")
    usb_dm = Net("USB_DM")
    usb_cc1 = Net("USB_CC1")
    usb_cc2 = Net("USB_CC2")
    usb_shield = Net("USB_SHIELD")

    usb_host_dp = Net("USB_HOST_DP")
    usb_host_dm = Net("USB_HOST_DM")
    usb_host_id = Net("USB_HOST_ID")

    qspi_clk = Net("QSPI_CLK")
    qspi_cs = Net("QSPI_CS")
    qspi_d0 = Net("QSPI_D0")
    qspi_d1 = Net("QSPI_D1")
    qspi_d2 = Net("QSPI_D2")
    qspi_d3 = Net("QSPI_D3")

    reset_b = Net("RESET_B")
    program = Net("PROGRAM")
    on_off = Net("ON_OFF")
    boot0 = Net("BOOT_MODE0")
    boot1 = Net("BOOT_MODE1")

    swdclk = Net("SWDCLK")
    swdio = Net("SWDIO")

    mclk = Net("MCLK")
    bclk = Net("BCLK")
    lrclk = Net("LRCLK")
    i2s_tx = Net("I2S_TX")
    i2s_rx = Net("I2S_RX")
    i2c_scl = Net("I2C_SCL")
    i2c_sda = Net("I2C_SDA")
    codec_reset = Net("CODEC_RESET")
    audio_mute = codec_reset  # alias for anyone reading the IMX pin names

    # === Power wiring ===
    for pin_name in ["VIN"]:
        mcu[pin_name] += vusb
    for pin_name in ["3V3_CORE"]:
        mcu[pin_name] += v3v3
    for pin_name in ["GND0"]:
        mcu[pin_name] += gnd

    boot_mcu["VDD"] += v3v3
    boot_mcu["GND"] += gnd

    qspi["VCC"] += v3v3
    qspi["GND"] += gnd

    vusb_filter[1] += vusb
    vusb_filter[2] += v5

    vbat_cap[1] += vbat
    vbat_cap[2] += gnd

    for cap in v3v3_caps:
        cap[1] += v3v3
        cap[2] += gnd

    # === USB connections ===
    for pin in ["A1", "A4", "A9", "B1", "B4", "B9"]:
        usb[pin] += vusb
    for pin in ["A12", "B12"]:
        usb[pin] += gnd
    usb["SH"] += usb_shield

    usb["A6"] += usb_dp
    usb["B6"] += usb_dp
    usb["A7"] += usb_dm
    usb["B7"] += usb_dm
    usb["A5"] += usb_cc1
    usb["B5"] += usb_cc2

    cc1[1] += usb_cc1
    cc1[2] += gnd
    cc2[1] += usb_cc2
    cc2[2] += gnd

    mcu["USB_DP"] += usb_dp
    mcu["USB_DM"] += usb_dm
    mcu["USBH_DP"] += usb_host_dp
    mcu["USBH_DM"] += usb_host_dm

    usb_host[1] += v5
    usb_host[2] += usb_host_dp
    usb_host[3] += usb_host_dm
    usb_host[4] += usb_host_id
    usb_host[5] += usb_shield
    usb_host[6] += gnd
    usb_host_id_pull[1] += usb_host_id
    usb_host_id_pull[2] += gnd

    # === Boot + debug ===
    mcu["PROGRAM"] += program
    mcu["RESET_B"] += reset_b
    mcu["ON_OFF"] += on_off
    mcu["BOOT_MODE0"] += boot0
    mcu["BOOT_MODE1"] += boot1
    mcu["SWDIO"] += swdio
    mcu["SWDCLK"] += swdclk

    boot_mcu["PROG"] += program
    boot_mcu["RESET_B"] += reset_b
    boot_mcu["ON_OFF"] += on_off
    boot_mcu["SWDIO"] += swdio
    boot_mcu["SWDCLK"] += swdclk

    boot0_pull[1] += boot0
    boot0_pull[2] += gnd
    boot1_pull[1] += boot1
    boot1_pull[2] += v3v3
    program_pull[1] += program
    program_pull[2] += v3v3
    reset_pull[1] += reset_b
    reset_pull[2] += v3v3

    jtag[1] += swdio
    jtag[2] += swdclk
    jtag[3] += reset_b
    jtag[4] += gnd

    # === QSPI flash ===
    mcu["QSPI_SCK"] += qspi_clk
    mcu["QSPI_CS"] += qspi_cs
    mcu["QSPI_DATA0"] += qspi_d0
    mcu["QSPI_DATA1"] += qspi_d1
    mcu["QSPI_DATA2"] += qspi_d2
    mcu["QSPI_DATA3"] += qspi_d3

    qspi["CLK"] += qspi_clk
    qspi["/CS"] += qspi_cs
    qspi["DI"] += qspi_d0
    qspi["DO"] += qspi_d1
    qspi["/WP"] += qspi_d2
    qspi["/HOLD"] += qspi_d3

    # === Audio / control ===
    mcu["MCLK"] += mclk
    mcu["BCLK"] += bclk
    mcu["LRCLK"] += lrclk
    mcu["I2S_TX"] += i2s_tx
    mcu["I2S_RX"] += i2s_rx
    mcu["I2C_SCL"] += i2c_scl
    mcu["I2C_SDA"] += i2c_sda
    mcu["AUDIO_MUTE"] += codec_reset

    # === Breakout headers ===
    pio_nets = [Net(f"PIO_{idx:02d}") for idx in range(54)]
    for idx, pin_name in enumerate(
        [
            "PIO00",
            "PIO01",
            "PIO02",
            "PIO03",
            "PIO04",
            "PIO05",
            "PIO06",
            "PIO07",
            "PIO08",
            "PIO09",
            "PIO10",
            "PIO11",
            "PIO12",
            "PIO13",
            "PIO14",
            "PIO15",
            "PIO16",
            "PIO17",
            "PIO18",
            "PIO19",
            "PIO20",
            "PIO21",
            "PIO22",
            "PIO23",
            "PIO24",
            "PIO25",
            "PIO26",
            "PIO27",
            "PIO28",
            "PIO29",
            "PIO30",
            "PIO31",
            "PIO32",
            "PIO33",
            "PIO34",
            "PIO35",
            "PIO36",
            "PIO37",
            "PIO38",
            "PIO39",
            "PIO40",
            "PIO41",
            "PIO42",
            "PIO43",
            "PIO44",
            "PIO45",
            "PIO46",
            "PIO47",
            "PIO48",
            "PIO49",
            "PIO50",
            "PIO51",
            "PIO52",
            "PIO53",
            "PIO54",
            "PIO55",
            "PIO56",
            "PIO57",
            "PIO58",
            "PIO59",
            "PIO60",
            "PIO61",
            "PIO62",
            "PIO63",
            "PIO64",
            "PIO65",
            "PIO66",
            "PIO67",
        ]
    ):
        if idx < len(pio_nets):
            net = pio_nets[idx]
            mcu[pin_name] += net

    # map to headers: left_header pins odd/even mapping like Teensy 4.1
    for idx in range(20):
        left_header[2 * idx + 1] += pio_nets[idx]
        left_header[2 * idx + 2] += gnd if idx % 2 else v3v3

    for idx in range(20):
        right_header[2 * idx + 1] += pio_nets[idx + 20]
        right_header[2 * idx + 2] += gnd if idx % 2 else v3v3

    for idx in range(7):
        underside_header[2 * idx + 1] += pio_nets[40 + idx * 2]
        underside_header[2 * idx + 2] += pio_nets[41 + idx * 2]

    return locals()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/hw/teensy41_core.net"),
        help="Where to drop the generated netlist.",
    )
    args = parser.parse_args(argv)

    args.output.parent.mkdir(parents=True, exist_ok=True)

    build_core()
    ERC()
    generate_netlist(file_=str(args.output))
    return 0


if __name__ == "__main__":
    sys.exit(main())
