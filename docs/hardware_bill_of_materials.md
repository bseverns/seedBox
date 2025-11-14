# SeedBox hardware bill of materials

Welcome to the gear dump. This is the parts bin manifest for anyone who wants to
spin up a physical SeedBox instead of just vibing with the native build. Read
it like a tour guide and a shopping list rolled into one: grab the essentials,
flag the optional bling, and learn *why* each piece exists so you can sub in
alternatives without frying the groove. Once parts land, flip to the
[wiring gallery](wiring_gallery.md) for routing photos and the
[calibration guide](calibration_guide.md) before you ever power the stack.

## Core brain stack (must-haves)

| Item | Suggested part numbers & sources | Why it matters | Notes |
| --- | --- | --- | --- |
| Teensy 4.0 microcontroller | PJRC [DEV-15583](https://www.pjrc.com/store/teensy40.html); SparkFun, Adafruit resellers | Runs the audio engine at silly-fast speeds with floating-point DSP. | Order at least one spare if you plan to field-debug. |
| PJRC Audio Shield (Rev D or later, SGTL5000) | PJRC [DEV-16829](https://www.pjrc.com/store/teensy3_audio.html) | Breaks out I²S, codec, and headphone amp so we can hear anything. | Works on Teensy 4.x with minor solder jumpers pre-made. |
| 0.1" stacking headers for Teensy + audio shield | 14-pin & 24-pin stackable header kits | Physically mates the Teensy and audio shield without mangling test access. | Go for tall stackers if you plan to wire extra boards underneath. |
| SparkFun Qwiic OLED 1.3" 128×64 (I²C, SH1107) | [LCD-17153](https://www.sparkfun.com/products/17153) | Debug + UI status display with enough pixels for verbose seed gossip. | Qwiic connector + castellated pads, 3.3 V only, on-board pull-ups already present. |
| Rotary encoders with integrated push buttons (x2) | PEC11R, Bourns, or Alps clones | Primary human input for parameter twiddling. | Choose detent style you enjoy; 24 PPR feels right. |
| Momentary push buttons (x2) | Panel-mount SPST normally-open | Transport control / macros. | Grab low-profile caps if you’re building a handheld case. |
| 10 kΩ resistors (through-hole or SMD, x4) | E12 series kit | Pull-ups for buttons and encoders. | Metal film is fine; 1/8 W or 1/4 W. |
| Hook-up wire (26–28 AWG, solid) | Wire kit or ribbon cable | Routes signals from Teensy to panel controls. | Solid wire makes breadboarding easier; switch to stranded for flex joints. |
| USB-C to micro-B cable | Any data-capable cable | Powers and programs the Teensy. | Keep a short, shielded cable for reduced noise. |

## Signal hygiene & audio path helpers

| Item | Suggested part numbers & sources | Why it matters | Notes |
| --- | --- | --- | --- |
| 4.7 kΩ resistors (x2) | For I²C pull-ups | Stabilizes the OLED bus when multiple peripherals hang out. | Skip them when the SparkFun Qwiic OLED is alone on the bus—the breakout already has them. |
| 100 nF ceramic capacitors (assorted) | Bypass kit | Decouple encoder and button lines against switch bounce noise. | Solder between signal and ground right at the panel. |
| Stereo 3.5 mm TRS jack | PJ-320D, Switchcraft 35RAPC2AV | Audio output to headphones/mixer. | Mount close to the audio shield to avoid hum loops. |
| Panel-mount USB extension (optional) | USB micro-B panel adapter | Keeps the main USB port accessible in an enclosure. | Ensure it supports data, not just charging. |

## MIDI & sync accessories

| Item | Suggested part numbers & sources | Why it matters | Notes |
| --- | --- | --- | --- |
| 3.5 mm TRS jacks (Type-A, x2) | PJRC T35-TRS, Adafruit 4399, DIYCables panel jacks | Modern MIDI I/O without adapters. | Follow the Type-A pinout: tip = MIDI+, ring = MIDI−, sleeve = ground. Route the IN jack to Serial7 RX (pin 28) and the OUT jack to Serial7 TX (pin 29) through the proper resistors. |
| 6N138 or PC900 opto-isolator | MIDI IN isolation | Complies with the MIDI electrical spec so your rig survives bad cables. | Remember the 220 Ω + 1 N4148 LED protection network. |
| 220 Ω resistors (x2) | MIDI input current limiting | Works with the opto-isolator LED. | 1/4 W carbon film is fine. |
| 33 Ω resistors (x2) | MIDI Type-A output impedance | Protects the Teensy pins and meets the spec when driving TRS. | Mount them inline on the TX leg before the jack. |
| 1 N4148 diodes (x2) | MIDI input protection | Shields the opto LED from reverse voltage. | Any fast small-signal diode works. |
| MIDI breakout PCB or perfboard | DIY layout platform | Keeps the opto, resistors, and jack tidy. | Perfboard is fine for prototypes. |

## Power management odds and ends

| Item | Suggested part numbers & sources | Why it matters | Notes |
| --- | --- | --- | --- |
| Breadboard-friendly DC barrel jack (optional) | 2.1 mm center-positive | Lets you inject external 9–12 V if you ditch USB power. | If you power externally, cut the Teensy VUSB/VIN trace and add a regulator. |
| 5 V to 3.3 V regulator (optional) | Pololu S7V8F3 or LD1117-3.3 linear reg | Powers peripherals when USB cannot. | Only needed for standalone rigs with beefy peripherals. |
| Power switch (latching) | Panel toggle or slide | Gives you a true off switch when running from external power. | Place on the high side before regulators. |

## Fasteners, mechanical bits, and creature comforts

| Item | Suggested part numbers & sources | Why it matters | Notes |
| --- | --- | --- | --- |
| M2.5 or M3 standoffs (nylon or metal) | 10 mm + 12 mm lengths | Secures Teensy + audio shield sandwich to your panel or baseplate. | Mix nylon + metal to avoid accidental shorts. |
| Laser-cut or 3D-printed enclosure panels | DIY | Keeps fingers off the high-speed digital pins. | Share your CAD files in `/docs/` when you iterate. |
| Knobs for encoders (x2) | Rogan soft-touch, Davies 1900H | Makes the interface feel like an instrument. | 6 mm D-shaft for most encoders. |
| Rubber feet or adhesive bumpers | Hardware store | Stops the box from skating off the table during live tweaks. | Cheap insurance. |
| Label tape / paint pen | Sharpie, Brother | Mark I/O and debug headers for future-you. | Hand-written labels > no labels. |

## Tools you actually need on the bench

| Tool | Why | Notes |
| --- | --- | --- |
| Temperature-controlled soldering iron + fine tip | Clean joints on Teensy headers and panel wiring. | Hakko FX-888D class or better keeps frustration low. |
| Leaded solder (0.5–0.7 mm) | Easier to work with than lead-free for prototypes. | Ventilate the room, please. |
| Flush cutters & needle-nose pliers | Trim leads, bend wires, tame headers. | The $10 Amazon kit is fine, upgrade as they dull. |
| Multimeter | Sanity-check power rails and continuity. | Autoranging handheld units are plenty accurate here. |
| Logic analyzer or USB oscilloscope (optional) | Debug MIDI/clock edges, verify I²S lines. | Saleae clones work in a pinch; document captures in `/docs/`. |
| Helping hands or PCB vise | Hold the stack-up while you solder. | Your wrists will thank you. |
| Breadboard or perfboard | Prototype input circuits before committing. | Keep one dedicated to MIDI testing. |

## Shopping notes & sourcing hacks

- **Buy extras** of cheap passives and encoders. Hardware gremlins love to eat the
  last resistor at 1 a.m.
- **Group orders** with friends to dodge shipping gouges on PJRC parts. Teensy
  boards ship fast from the mothership, but mixing in Digikey/Mouser orders keeps
  everything on one invoice.
- **Remember the Qwiic pigtails**. The SparkFun OLED speaks JST-SH and 3.3 V only;
  if you're not chaining other Qwiic widgets, a short cable or hand-soldered
  ribbon straight to SDA/SCL keeps the wiring tight and quiet.
- **Document substitutions**. If you drop in a different OLED module or encode a
  bespoke panel PCB, leave a note (and ideally a photo) in this doc so the next
  builder inherits the lore.
- **Track firmware expectations**: whenever the pinout changes, update
  [`docs/builder_bootstrap.md`](builder_bootstrap.md) so the wiring map matches
  the real world.

## Open invites for contributors

This manifest is intentionally scrappy. When you validate a new encoder family,
find a boutique knob that sparks joy, or design a case that survives gig
backpacks, add it here with prices, photos, and caveats. The more we annotate
*why* a part made the list, the easier it is for students and collaborators to
mod the rig without fear.
