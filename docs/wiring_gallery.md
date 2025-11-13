# Wiring gallery & routing riffs

This gallery is the fast handoff between schematics and the soldering iron. It
captures the canonical wiring moves, highlights edge cases, and links back to
the code that assumes you nailed each connection. Treat it like a studio wall of
polaroids: annotate it, remix it, but keep the intent front and centre.

## Quick inventory check

- [ ] Lay out the core stack from the BOM: Teensy 4.0, SGTL5000 audio shield,
      Qwiic OLED, encoders, and TRS MIDI jacks. Cross-check part numbers against
      [`docs/hardware_bill_of_materials.md`](hardware_bill_of_materials.md).
- [ ] Print or bookmark the pin table in
      [`builder_bootstrap.md`](builder_bootstrap.md#hardware-wiring--test-points)
      so the bench crew can sanity-check every lead.
- [ ] Mark any substitutions (alternate OLED, different encoder footprint) and
      note them inline so firmware folks know when to expect surprises.

## Core stack layout

- [ ] Stack the SGTL5000 and Teensy using headers that keep the I²S bundle from
      [`include/HardwareConfig.h`](../include/HardwareConfig.h) untouched. If you
      reroute MCLK/LRCLK, annotate the diff.
- [ ] Run the Qwiic OLED over SDA/SCL (pins 18/19) and confirm the default
      `0x3C` address in `HardwareConfig` still lines up with the display driver in
      [`src/ui/OledStatus.cpp`](../src/ui/OledStatus.cpp).
- [ ] Wire encoders so clockwise turns increment values — the UI state machine in
      [`src/app/AppState.cpp`](../src/app/AppState.cpp) assumes that polarity when
      dispatching mode changes.
- [ ] Keep analog and power leads twisted or ribboned; drop scope photos here to
      document noise fixes for the next build.

## TRS + clock playground

- [ ] Solder Type-A TRS jacks exactly like the bench flow in
      [`hardware/trs_clock_sync/README.md`](hardware/trs_clock_sync/README.md) and
      snapshot the wiring before you close the enclosure.
- [ ] Route the TRS IN jack through the opto coupler and into Serial7 RX (pin 28)
      so the external clock tests in
      [`test/test_app/test_external_midi_priority.cpp`](../test/test_app/test_external_midi_priority.cpp)
      keep matching hardware reality.
- [ ] Add labels for MIDI IN/OUT polarity on the PCB or wiring harness — future
      testers will thank you when reproducing `clock dominance` demos.

## Capture + share

- [ ] Photograph every unique wiring angle (top, side, underside). Drop them in
      `docs/images/` or link to the studio photo archive.
- [ ] When you edit this gallery, cite which test or firmware module motivated
      the change so code readers can trace the wiring assumption instantly.
- [ ] If a harness fix resolves a bug, mirror the story in
      [`docs/troubleshooting_log.md`](troubleshooting_log.md) with a timestamp and
      the before/after photos.
