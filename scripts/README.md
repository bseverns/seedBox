# Utility scripts — the pit crew

`scripts/` holds tiny helpers that keep the build smooth and the version info
honest. Nothing here should feel scary; if a script needs special setup, it must
say so loudly.

## What's in the garage

| Script | Job | Notes |
| --- | --- | --- |
| `starter_bundle.sh` | Runs the minimum newcomer workflow (`pio pkg install` + `pio test -e native`). | Use this when onboarding: it proves the laptop-native path before touching hardware. |
| `describe_seedbox_config.py` | Reads `include/SeedBoxConfig.h` and prints flag defaults. | CI uses it to prove the docs aren't lying about the toggles; run it locally when writing docs. |
| `doc_spellcheck_targets.py` | Emits the Markdown files we want spell-checkers or Vale to monitor. | Pipe its output into `codespell`, `vale`, or your favourite doc linter so new guides stay honest. |
| `gen_version.py` | Generates `include/BuildInfo.generated.h` with git hash + build time. | PlatformIO runs it before builds so the firmware can introduce itself over serial without dirtying tracked files. |
| `generate_golden_fixture_browser.py` | Emits a static HTML browser from `tests/native_golden/golden.json`. | Writes `build/fixtures/index.html` with audio players, metadata, and inline ledger previews so the checked-in fixture crate is browseable without custom tooling. |
| `generate_app_service_map.py` | Emits the Mermaid service graph from `docs/architecture/app_services_map.json`. | Writes `docs/architecture/app_services.mmd` so the extracted app/JUCE/runtime seams stay documented without hand-editing the graph. |
| `generate_native_golden_header.py` | Regenerates `tests/native_golden/fixtures_autogen.hpp` from the manifest. | Keeps the Unity harness synced with whatever `compute_golden_hashes.py` discovers so we never hand-edit fixture tables. |
| `offline_native_golden.sh` | Compiles and runs the desktop golden renderer without PlatformIO. | Accepts `--filter` plus repeatable `--input-tone [name=]freq[:amp[:seconds]]` so you can add ad-hoc tone fixtures into `build/fixtures/` and then rehash them into the manifest. |
| `run_local_input_golden.py` | Runs an external WAV through the native host stack, then routes the resulting folder through the golden hash/browser pipeline. | Builds `seedbox_native_input_probe`, writes receipt artifacts beside the source file by default, emits `golden.local.json`, and generates a local `index.html` without touching the checked-in manifest/header. |
| `serve_golden_fixture_browser.py` | Serves the fixture browser with a local-only tone-generation API. | Binds a same-origin POST endpoint for the generated HTML surface, runs `offline_native_golden.sh --input-tone ...`, and reloads the browser against the refreshed manifest/browser. |
| `native/tap_tempo.py` | Estimates BPM from tap timestamps. | Accepts CLI args or STDIN; prints mean interval, BPM, and optional PPQN correction so workshops can nerd out on timing math. |
| `native/micro_offset_probe.py` | Audits per-track micro offsets. | Feed it offsets in milliseconds and it yells if any lane drifts beyond your tolerance — perfect for regression gates around swing experiments. |
| `kicad/sgtl5000_frontend.py` | Spits out a KiCad-ready SGTL5000 codec netlist using SKiDL. | Needs KiCad libraries on disk (`KICAD_SYMBOL_DIR` etc.) and `pip install skidl`. In library-less CI or preview runs it falls back to internal stub symbols, prints a ton of warnings, and still drops `build/hw/sgtl5000_frontend.net`. |
| `kicad/teensy41_core.py` | Builds the Teensy 4.1 core (IMXRT1062, MKL02, QSPI flash, USB-C, breakouts). | Mirrors the [JensChr reference board](https://github.com/jenschr/Teensy-4.1-example) so we can fab our own MCU base. Same SKiDL + KiCad lib requirements as the SGTL script; stub symbols unblock previews. |
| `kicad/seedbox_stack.py` | One-shot netlist for the full SeedBox brain (core + SGTL5000). | Calls the two generators above, reuses their nets, and spits out `build/hw/seedbox_stack.net` so you can start layout with everything already talking. |

## When you add a script

1. Prefer standard-library dependencies. If you need extra packages, document
   the install steps in the top-level README.
2. Make reruns safe — scripts should be idempotent so they never trash previous
   artifacts.
3. Drop a usage example either in the script header or this README. Future you
   will appreciate the reminder.

If a script spits out renders or logs, aim them at `out/` for disposable jams or
`artifacts/` for golden material. Both paths are already ignored by git, so the
history stays focused on intent, not binaries.

### Flag crib sheet on demand

Need to prove the README's build flag table still matches reality? Run the
config narrator and paste its Markdown straight into your doc:

```bash
python scripts/describe_seedbox_config.py --format=markdown
```

It scrapes `include/SeedBoxConfig.h`, decodes the inline stories, and prints a
ready-to-share table. Great for PR descriptions, lab guides, or reminding CI to
show its work.

### KiCad / SKiDL setup crib notes

The SGTL5000 generator leans on SKiDL's KiCad integration. Make sure your KiCad
install exported `KICAD_SYMBOL_DIR`, `KICAD6_SYMBOL_DIR` (or whichever major
version you live on), and your `fp-lib-table` is in the usual KiCad config
directory. If SKiDL screams about missing libraries, point those environment
variables at your KiCad share tree, rerun the script, and you should be back in
business. Keep the one-liner handy:

```bash
pip install skidl
python scripts/kicad/sgtl5000_frontend.py --output build/hw/sgtl5000_frontend.net
```

No KiCad install on hand? The script now conjures stubbed symbols so you can
still preview the topology and net names. Expect SKiDL to yell about "missing
libraries" and spit out ERC warnings — that's fine for a sanity check. Once you
route this for real, rerun the script with proper KiCad libs so footprints and
pin types stay honest.

### Building a full-fat Teensy clone without dev boards

`kicad/teensy41_core.py` tracks the same vibe as Jens Chr. Brynildsen's
Teensy 4.1 example design: drop the IMXRT1062, PJRC's MKL02 boot MCU, the QSPI
flash, and USB-C straight onto your board, then fan out all IO through chunky
2x20 headers. You can still run the core and SGTL generators separately if you
want to stitch the sheets by hand, but the new `seedbox_stack.py` shortcut does
the legwork for you:

```bash
python scripts/kicad/seedbox_stack.py --output build/hw/seedbox_stack.net
```

Under the hood that script reuses the same `Net` objects for the shared rails,
I²S, I²C, and codec reset lines, so the MCU pins and the SGTL5000 land on the
exact same nets. Fire it up and you've basically recreated the Teensy 4.1 +
audio shield stack without leaving SKiDL.

Treat these helpers like the band techs: not flashy, but the show can't start
without them.
