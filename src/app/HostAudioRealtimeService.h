#pragma once

class AppState;

// HostAudioRealtimeService owns the callback-safe heartbeat used by the JUCE
// lane. It intentionally avoids UI/storage work and focuses on the timing
// edges that must advance once per host audio block.
class HostAudioRealtimeService {
 public:
  void tick(AppState& app) const;

 private:
  static void syncTransportState(AppState& app);
  static void advanceClockEdgeIfNeeded(AppState& app);
  static void publishFrameState(AppState& app);
};
