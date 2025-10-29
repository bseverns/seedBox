#!/usr/bin/env python3
"""Generate a KiCad-ready netlist for the SGTL5000 audio front end.

This SKiDL script sketches a faithful clone of the PJRC audio shield's codec
zone so we can graft it straight onto a custom control-surface PCB without
buying the off-the-shelf breakout. It wires the SGTL5000 codec to Teensy I²S
headers, drops the recommended decoupling network, and exposes stereo line
output on a jack-friendly connector.

Bring your own KiCad libraries: SKiDL needs the stock symbol + footprint libs
reachable via ``KICAD_SYMBOL_DIR``/``KICAD6_SYMBOL_DIR`` etc. If the script
squawks about missing symbols, point those environment variables at your KiCad
installation before rerunning.

Usage:
    python scripts/kicad/sgtl5000_frontend.py \
        --output build/hw/sgtl5000_frontend.net

Key nets stay obvious:

* ``MCLK``, ``BCLK``, ``LRCLK``, ``I2S_TX``, ``I2S_RX`` on an 8-pin header so the
  firmware's hard-coded pin map keeps working.
* ``CODEC_RESET`` + I²C lines travel alongside the I²S bundle for easy harnessing.
* Stereo headphone outs feed a TRS jack through the usual 100 Ω / 100 nF filter
  combo.

You can also punt the CLI and import :func:`build_frontend` from another SKiDL
script if you want to extend/merge the design with more circuitry. The generated
netlist targets KiCad 6+. If you are on KiCad 5 you can still use the netlist;
just remap footprints manually.
"""

from __future__ import annotations

import argparse
import sys
from collections.abc import Mapping
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

# Point SKiDL at KiCad so library names resolve the way we expect.
set_default_tool(KICAD)


def _power_net(name: str) -> Net:
    """Create a named power net that behaves like a supply rail."""

    net = Net(name)
    net.drive = POWER
    return net


def _label_pin(pin, label: str) -> None:
    """Give a connector pin a friendly name without assuming KiCad libs are present."""

    if hasattr(pin, "alias"):
        pin.alias(label)
    else:  # SKiDL-native pins expose ``name`` instead of an alias helper.
        pin.name = label


_STUB_PARTS: dict[tuple[str, str], dict[str, object]] = {
    ("power", "+3V3"): {
        "pins": [Pin(num="1", name="+3V3")],
        "ref_prefix": "PWR",
    },
    ("power", "GND"): {
        "pins": [Pin(num="1", name="GND")],
        "ref_prefix": "PWR",
    },
    ("power", "PWR_FLAG"): {
        "pins": [Pin(num="1", name="PWR")],
        "ref_prefix": "PWR",
    },
    ("Connector_Generic", "Conn_01x08"): {
        "pins": [
            Pin(num=str(idx), name=f"Pin_{idx}")
            for idx in range(1, 9)
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
    ("Connector_Audio", "AudioJack3"): {
        "pins": [
            Pin(num="1", name="TIP"),
            Pin(num="2", name="RING"),
            Pin(num="3", name="SLEEVE"),
        ],
        "ref_prefix": "J",
    },
    ("Device", "R"): {
        "pins": [
            Pin(num="1", name="1"),
            Pin(num="2", name="2"),
        ],
        "ref_prefix": "R",
    },
    ("Device", "C"): {
        "pins": [
            Pin(num="1", name="1"),
            Pin(num="2", name="2"),
        ],
        "ref_prefix": "C",
    },
    ("Device", "CP"): {
        "pins": [
            Pin(num="1", name="1"),
            Pin(num="2", name="2"),
        ],
        "ref_prefix": "C",
    },
    ("Device", "L"): {
        "pins": [
            Pin(num="1", name="1"),
            Pin(num="2", name="2"),
        ],
        "ref_prefix": "L",
    },
    ("Audio", "SGTL5000"): {
        "pins": [
            Pin(num="1", name="VDDA"),
            Pin(num="2", name="VDDD"),
            Pin(num="3", name="VDDIO"),
            Pin(num="4", name="VAG"),
            Pin(num="5", name="GND"),
            Pin(num="6", name="MCLK"),
            Pin(num="7", name="BCLK"),
            Pin(num="8", name="LRCLK"),
            Pin(num="9", name="DIN"),
            Pin(num="10", name="DOUT"),
            Pin(num="11", name="RESET"),
            Pin(num="12", name="SDA"),
            Pin(num="13", name="SCL"),
            Pin(num="14", name="HP_OUT_L"),
            Pin(num="15", name="HP_OUT_R"),
            Pin(num="16", name="LINEIN_L"),
            Pin(num="17", name="LINEIN_R"),
            Pin(num="18", name="MIC"),
            Pin(num="19", name="HP_GND"),
            Pin(num="20", name="VREF"),
        ],
        "ref_prefix": "U",
        "name": "SGTL5000_STUB",
    },
}


def _make_stub_part(lib: str, name: str, **kwargs) -> Part:
    """Build a minimal SKiDL-native part so the script can run without KiCad libs."""

    spec = _STUB_PARTS.get((lib, name))
    if not spec:
        raise RuntimeError(
            f"Unable to load symbol {lib}:{name}. Make sure your KiCad libraries are "
            "installed and KICAD* environment variables point at them."
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
    """Wrap :func:`~skidl.Part` with graceful fallback to stub symbols."""

    try:
        return Part(*args, **kwargs)
    except Exception:  # pragma: no cover - exercised when KiCad libs missing.
        lib = args[0] if args else "<unknown>"
        name = args[1] if len(args) > 1 else kwargs.get("name", "<unknown>")
        return _make_stub_part(lib, name, **kwargs)


def build_frontend(net_overrides: Mapping[str, Net] | None = None) -> dict[str, object]:
    """Instantiate the SGTL5000 audio codec frontend.

    Parameters
    ----------
    net_overrides:
        Optional mapping that lets callers reuse already-defined nets when this
        generator gets composed into a larger design (for example, when the
        Teensy core script calls into us). Keys line up with the local variable
        names in this function: pass ``{"mclk": existing_net}`` to bolt the
        codec's MCLK straight onto the caller's net object.
    """

    net_overrides = dict(net_overrides or {})

    def _pick_net(key: str, default_name: str, *, power: bool = False) -> Net:
        net = net_overrides.get(key)
        if net is None:
            net = Net(default_name)
            if power:
                net.drive = POWER
            net_overrides[key] = net
        elif power:
            net.drive = POWER
        return net

    # === Power rails ===
    avdd = net_overrides.get("avdd")
    if avdd is None:
        avdd = _power_net("AVDD_3V3")
        net_overrides["avdd"] = avdd

    dvdd = net_overrides.get("dvdd")
    if dvdd is None:
        dvdd = _power_net("DVDD_3V3")
        net_overrides["dvdd"] = dvdd

    vdda_filter = Net("AVDD_FILTERED")
    gnd = net_overrides.get("gnd")
    if gnd is None:
        gnd = Net("GND")
        gnd.drive = POWER
        net_overrides["gnd"] = gnd
    else:
        gnd.drive = POWER

    avdd_flag = _require_part("power", "+3V3")
    dvdd_flag = _require_part("power", "+3V3")
    gnd_flag = _require_part("power", "GND")
    pwr_flag_avdd = _require_part("power", "PWR_FLAG")
    pwr_flag_dvdd = _require_part("power", "PWR_FLAG")

    avdd_flag[1] += avdd
    dvdd_flag[1] += dvdd
    gnd_flag[1] += gnd
    pwr_flag_avdd[1] += avdd
    pwr_flag_dvdd[1] += dvdd

    # === Codec ===
    codec = _require_part(
        "Audio",
        "SGTL5000",
        footprint="Package_QFN:QFN-32-1EP_5x5mm_P0.5mm_EP3.45x3.45mm",
        dest=TEMPLATE,
    )

    # === Teensy / IMXRT I²S header ===
    i2s_header = _require_part(
        "Connector_Generic",
        "Conn_01x08",
        footprint="Connector_PinHeader_2.54mm:PinHeader_1x08_P2.54mm_Vertical",
    )
    _label_pin(i2s_header[1], "MCLK")
    _label_pin(i2s_header[2], "BCLK")
    _label_pin(i2s_header[3], "LRCLK")
    _label_pin(i2s_header[4], "I2S_TX")
    _label_pin(i2s_header[5], "I2S_RX")
    _label_pin(i2s_header[6], "CODEC_RESET")
    _label_pin(i2s_header[7], "I2C_SDA")
    _label_pin(i2s_header[8], "I2C_SCL")

    # === Control header (optional SPI for SD card) ===
    spi_header = _require_part(
        "Connector_Generic",
        "Conn_01x06",
        footprint="Connector_PinHeader_2.54mm:PinHeader_1x06_P2.54mm_Vertical",
    )
    _label_pin(spi_header[1], "3V3")
    _label_pin(spi_header[2], "SD_CS")
    _label_pin(spi_header[3], "MOSI")
    _label_pin(spi_header[4], "MISO")
    _label_pin(spi_header[5], "SCK")
    _label_pin(spi_header[6], "GND")

    # === Analog line output ===
    jack = _require_part(
        "Connector_Audio",
        "AudioJack3",
        footprint="Connector_Audio:Jack_3.5mm_PJ320D_Horizontal",
    )

    lout_res = _require_part(
        "Device", "R", value="100", footprint="Resistor_SMD:R_0603_1608Metric"
    )
    rout_res = _require_part(
        "Device", "R", value="100", footprint="Resistor_SMD:R_0603_1608Metric"
    )

    lout_cap = _require_part(
        "Device", "C", value="100n", footprint="Capacitor_SMD:C_0603_1608Metric"
    )
    rout_cap = _require_part(
        "Device", "C", value="100n", footprint="Capacitor_SMD:C_0603_1608Metric"
    )

    # === Local decoupling ===
    bulk_cap = _require_part(
        "Device", "CP", value="10u", footprint="Capacitor_SMD:C_1206_3216Metric"
    )
    avdd_bypass = _require_part(
        "Device", "C", value="100n", footprint="Capacitor_SMD:C_0603_1608Metric"
    )
    dvdd_bypass = _require_part(
        "Device", "C", value="100n", footprint="Capacitor_SMD:C_0603_1608Metric"
    )
    ref_bypass = _require_part(
        "Device", "C", value="1u", footprint="Capacitor_SMD:C_0603_1608Metric"
    )

    ferrite = _require_part(
        "Device", "L", value="600R@100MHz", footprint="Inductor_SMD:L_0805_2012Metric"
    )

    # === Nets ===
    mclk = _pick_net("mclk", "MCLK")
    bclk = _pick_net("bclk", "BCLK")
    lrclk = _pick_net("lrclk", "LRCLK")
    i2s_tx = _pick_net("i2s_tx", "I2S_TX")
    i2s_rx = _pick_net("i2s_rx", "I2S_RX")
    codec_reset = _pick_net("codec_reset", "CODEC_RESET")
    i2c_sda = _pick_net("i2c_sda", "I2C_SDA")
    i2c_scl = _pick_net("i2c_scl", "I2C_SCL")

    line_out_l = Net("LINE_OUT_L")
    line_out_r = Net("LINE_OUT_R")
    sd_cs = Net("SD_CS")
    mosi = Net("MOSI")
    miso = Net("MISO")
    sck = Net("SCK")

    # === Wire up codec power ===
    codec["VDDA"] += vdda_filter
    codec["VDDD"] += dvdd
    codec["VDDIO"] += dvdd
    codec["VAG"] += vdda_filter
    codec["GND"] += gnd

    ferrite[1] += avdd
    ferrite[2] += vdda_filter

    bulk_cap[1] += avdd
    bulk_cap[2] += gnd

    avdd_bypass[1] += vdda_filter
    avdd_bypass[2] += gnd

    dvdd_bypass[1] += dvdd
    dvdd_bypass[2] += gnd

    ref_bypass[1] += codec["VREF"]
    ref_bypass[2] += gnd

    # === Hook up I²S + control ===
    codec["MCLK"] += mclk
    codec["BCLK"] += bclk
    codec["LRCLK"] += lrclk
    codec["DIN"] += i2s_tx
    codec["DOUT"] += i2s_rx
    codec["RESET"] += codec_reset
    codec["SDA"] += i2c_sda
    codec["SCL"] += i2c_scl

    i2s_header[1] += mclk
    i2s_header[2] += bclk
    i2s_header[3] += lrclk
    i2s_header[4] += i2s_tx
    i2s_header[5] += i2s_rx
    i2s_header[6] += codec_reset
    i2s_header[7] += i2c_sda
    i2s_header[8] += i2c_scl

    spi_header[1] += dvdd
    spi_header[2] += sd_cs
    spi_header[3] += mosi
    spi_header[4] += miso
    spi_header[5] += sck
    spi_header[6] += gnd

    # === Line outputs ===
    codec["HP_OUT_L"] += lout_res[1]
    codec["HP_OUT_R"] += rout_res[1]

    lout_res[2] += line_out_l
    rout_res[2] += line_out_r

    lout_cap[1] += line_out_l
    lout_cap[2] += gnd

    rout_cap[1] += line_out_r
    rout_cap[2] += gnd

    jack["TIP"] += line_out_l
    jack["RING"] += line_out_r
    jack["SLEEVE"] += gnd

    # === Misc recommended pins ===
    codec["LINEIN_L"] += gnd
    codec["LINEIN_R"] += gnd
    codec["MIC"] += gnd
    codec["HP_GND"] += gnd

    return locals()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/hw/sgtl5000_frontend.net"),
        help="Where to drop the generated netlist.",
    )
    args = parser.parse_args(argv)

    args.output.parent.mkdir(parents=True, exist_ok=True)

    build_frontend()
    ERC()
    generate_netlist(file_=str(args.output))
    return 0


if __name__ == "__main__":
    sys.exit(main())
