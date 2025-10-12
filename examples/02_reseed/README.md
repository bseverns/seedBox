# 02 · reseed — shuffle the ghost garden

We're still in quiet-land, but this time we yank on the RNG to show how repeatable the sandbox can be. The sketch spins a faux stem garden, reseeds it mid-flight, and keeps every move in the log so you can plan future resampling rituals.

## What it does

* Compiles under the native PlatformIO toolchain and runs as a desktop binary.
* Juggles a handful of imaginary stems, shuffling their order with a deterministic RNG.
* Reseeds the sequence and proves the groove stays silent—no audio buses, no crashes, just data scribbles.

## Wiring

Still zero. This is a notebook jam session with the speakers pulled from the rack.

## TODO for future WAV renders

* Mirror each seed into `/out/reseed-A.wav` and `/out/reseed-B.wav` when the render hooks exist.
* Compare the offline stems against hardware bounces to verify the RNG parity.
* Pipe the event log into a JSON sidecar once we start automating take sheets.

Nothing screams louder than silence with receipts.
