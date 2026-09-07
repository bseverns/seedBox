# SB-15 — Runtime boundaries and JUCE role

`AppState` remains the control-side orchestrator. Board input reaches it through
the input-event policy, engines receive immutable seed packets, and host audio
is rendered through the JUCE bridge rather than hardware callbacks.

JUCE is declared a **rehearsal and proof body** for now: it shares seed logic,
panel semantics, MIDI, and host rendering, but it is not evidence of hardware
waveform identity. Cross-body records must continue to distinguish behavioral
parity from audio identity. Hardware remains the first-class instrument body.
