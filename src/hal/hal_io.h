#pragma once

namespace seedbox {
class AppState;
}

namespace seedbox::hal {

// Configure IO subsystems before handing control to the application. For now
// this is a placeholder; hardware builds still prepare MIDI inside AppState.
void bootIo(AppState& app);

// Service non-audio IO during the main loop. On hardware this drains USB MIDI
// events when quiet mode allows it.
void pollIo(AppState& app);

}  // namespace seedbox::hal
