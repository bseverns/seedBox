#pragma once

//
// MIDI concierge with pluggable backends.
// --------------------------------------
// The router acts as a teaching aid and a guard rail.  It hides the raw wire
// protocols (USB, TRS Type-A, and the CLI simulator) behind a façade that spells
// out which ports can do what, how we route clock/transport, and where future
// CC→parameter maps plug in.  Every field is narrated so lab exercises can treat
// this header as both spec and quick reference.
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>
#include "util/Annotations.h"

class MidiRouter {
public:
  // Each physical (or simulated) transport that can exchange MIDI with the
  // firmware.  USB and TRS exist on hardware; the CLI backend mirrors both when
  // we compile for the native test harness.
  enum class Port : std::uint8_t {
    kUsb = 0,
    kTrsA = 1,
    kCount
  };

  static constexpr std::size_t kPortCount = static_cast<std::size_t>(Port::kCount);

  // A compact truth table describing what a backend is capable of.  The goal is
  // to demystify port wiring: when students flip open the router they instantly
  // see whether a port speaks clock, transport, or CC in either direction.
  struct PortInfo {
    const char* label = "";
    bool available = false;
    bool clockIn = false;
    bool clockOut = false;
    bool transportIn = false;
    bool transportOut = false;
    bool controlChangeIn = false;
    bool controlChangeOut = false;
  };

  // RouteConfig lets each UI page opt-in to consuming or mirroring specific
  // message types.  Think of it as the “patch bay” for the MIDI layer.
  struct RouteConfig {
    bool acceptClock = false;
    bool acceptTransport = false;
    bool acceptControlChange = true;
    bool mirrorClock = false;
    bool mirrorTransport = false;
  };

  // Channel maps keep routing explicit.  The default is identity (channel 0
  // stays 0, etc) but tests and future controller integrations can drop in a
  // remap without touching backend internals.
  struct ChannelMap {
    std::array<std::uint8_t, 16> inbound;
    std::array<std::uint8_t, 16> outbound;
    ChannelMap();
  };

  // Pages mirror the firmware UI.  PERF mode is the live rig, EDIT/HACK slots
  // are placeholders so we can evolve routing strategies without refactoring
  // every call site.
  enum class Page : std::uint8_t {
    kPerf = 0,
    kEdit,
    kHack,
    kCount
  };

  using ClockHandler = std::function<void()>;
  using TransportHandler = std::function<void()>;
  using ControlChangeHandler = std::function<void(std::uint8_t, std::uint8_t, std::uint8_t)>;
  using SysExHandler = std::function<void(const std::uint8_t*, std::size_t)>;

  MidiRouter();
  ~MidiRouter();

  // Spin up the available backends and reset all state.  Hardware builds wake
  // USB + TRS; native builds swap in CLI simulators so tests can push the same
  // flows without plugging in a Teensy.
  void begin();

  // Pump once per loop() so each backend can flush pending bytes.  The router
  // also takes the opportunity to maintain the MN42 keep-alive cadence.
  void poll();

  // Per-port metadata and routing levers.  Exposed publicly so docs/tests can
  // assert the ground truth about capabilities without reverse engineering the
  // implementation.
  const PortInfo& portInfo(Port port) const;
  void configurePortRouting(Port port, const RouteConfig& config);

  void configurePageRouting(Page page,
                            const std::array<RouteConfig, kPortCount>& matrix);
  void activatePage(Page page);

  void setChannelMap(Port port, const ChannelMap& map);
  const ChannelMap& channelMap(Port port) const;

  void setClockHandler(ClockHandler cb) { clockHandler_ = std::move(cb); }
  void setStartHandler(TransportHandler cb) { startHandler_ = std::move(cb); }
  void setStopHandler(TransportHandler cb) { stopHandler_ = std::move(cb); }
  void setControlChangeHandler(ControlChangeHandler cb) {
    controlChangeHandler_ = std::move(cb);
  }
  void setSysExHandler(SysExHandler cb) { sysexHandler_ = std::move(cb); }

  // MIDI generation surface.  Note-on/off keep a guard table so PANIC can flush
  // only the notes we actually lit up.
  void sendNoteOn(Port port, std::uint8_t channel, std::uint8_t note, std::uint8_t velocity);
  void sendNoteOff(Port port, std::uint8_t channel, std::uint8_t note, std::uint8_t velocity);
  void sendControlChange(Port port, std::uint8_t channel, std::uint8_t cc, std::uint8_t value);
  void sendStart(Port port);
  void sendStop(Port port);
  void sendClock(Port port);

  void panic();

  // Stub for mapping CCs to parameters.  The signature already exposes channel,
  // controller number, and value so higher layers can build a learn table.
  void onControlChange(std::uint8_t ch, std::uint8_t cc, std::uint8_t val);

  // Mark that the application stack finished booting. Once this flag flips we
  // reply to any MN42 hello traffic and keep the controller convinced we're
  // alive by streaming periodic handshake pulses.
  SEEDBOX_MAYBE_UNUSED void markAppReady();

  // Test harness hook: expose the CLI backend so unit tests can queue events and
  // inspect what would have gone out on the wire.
#ifndef SEEDBOX_HW
  class CliBackendAdapter;
  class CliBackend {
  public:
    struct SentMessage {
      enum class Type : std::uint8_t {
        kClock,
        kStart,
        kStop,
        kControlChange,
        kNoteOn,
        kNoteOff,
        kAllNotesOff
      };

      Type type;
      std::uint8_t channel = 0;
      std::uint8_t data1 = 0;
      std::uint8_t data2 = 0;
    };

    // Queue inbound events so the router sees the same flow a hardware backend
    // would deliver.
    void pushClock();
    void pushStart();
    void pushStop();
    void pushControlChange(std::uint8_t channel, std::uint8_t controller, std::uint8_t value);
    void pushSysEx(const std::vector<std::uint8_t>& payload);

    // Peek at what the router would have transmitted.  Tests can assert against
    // this log instead of scraping usbMIDI state.
    const std::vector<SentMessage>& sentMessages() const { return sent_; }
    void clearSent();

  private:
    friend class MidiRouter;
    friend class CliBackendAdapter;
    explicit CliBackend(MidiRouter& router, Port port);

    void poll();
    void begin();
    void sendClock();
    void sendStart();
    void sendStop();
    void sendControlChange(std::uint8_t channel, std::uint8_t controller, std::uint8_t value);
    void sendNoteOn(std::uint8_t channel, std::uint8_t note, std::uint8_t velocity);
    void sendNoteOff(std::uint8_t channel, std::uint8_t note, std::uint8_t velocity);
    void sendAllNotesOff(std::uint8_t channel);

    struct Event {
      enum class Type : std::uint8_t { kClock, kStart, kStop, kControlChange, kSysEx };
      Type type;
      std::uint8_t data1 = 0;
      std::uint8_t data2 = 0;
      std::uint8_t data3 = 0;
      std::vector<std::uint8_t> sysex;
    };

    MidiRouter& router_;
    Port port_;
    std::vector<Event> queue_{};
    std::vector<SentMessage> sent_{};
  };

  CliBackend* cliBackend(Port port);
#endif

private:
#ifdef SEEDBOX_HW
  class UsbMidiBackend;
  class TrsAMidiBackend;
#endif
  class Backend;

  struct PortState {
    PortInfo info;
    RouteConfig route;
    ChannelMap channels;
    std::array<std::bitset<128>, 16> activeNotes;
    bool noteGuardEnabled = true;
  };

  static constexpr std::size_t portIndex(Port port) {
    return static_cast<std::size_t>(port);
  }

  void clearNoteState();

  void handleClockFrom(Port port);
  void handleStartFrom(Port port);
  void handleStopFrom(Port port);
  void handleControlChangeFrom(Port port, std::uint8_t ch, std::uint8_t cc, std::uint8_t val);
  void handleSysExFrom(Port port, const std::uint8_t* data, std::size_t len);

  void handleMn42ControlChange(std::uint8_t ch, std::uint8_t cc, std::uint8_t val);
  void handleMn42SysEx(const std::uint8_t* data, std::size_t len);
  void sendMn42Handshake(std::uint8_t value);
  void maybeSendMn42KeepAlive();
  std::uint32_t nowMs() const;

  std::array<PortState, kPortCount> ports_{};
  std::array<std::unique_ptr<Backend>, kPortCount> backends_{};
#ifndef SEEDBOX_HW
  std::array<std::unique_ptr<CliBackend>, kPortCount> cliBackends_{};
#endif
  std::array<std::array<RouteConfig, kPortCount>, static_cast<std::size_t>(Page::kCount)>
      pageRouting_{};
  Page activePage_{Page::kPerf};

  ClockHandler clockHandler_{};
  TransportHandler startHandler_{};
  TransportHandler stopHandler_{};
  ControlChangeHandler controlChangeHandler_{};
  SysExHandler sysexHandler_{};

  bool mn42HelloSeen_{false};
  bool mn42AppReady_{false};
  bool mn42AckSent_{false};
  std::uint32_t mn42LastKeepAliveMs_{0};
};
