#include "io/MidiRouter.h"

#include "SeedBoxConfig.h"
#include "interop/mn42_map.h"

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

#if SEEDBOX_HW
  #include <MIDI.h>
  #include <usb_midi.h>
  #include <Arduino.h>
  #include "hal/hal_midi_serial7.h"
#endif

namespace {
constexpr std::uint32_t kMn42KeepAliveIntervalMs = 3000;

std::uint8_t sanitizeChannel(std::uint8_t channel) {
  return static_cast<std::uint8_t>(channel % 16);
}

}  // namespace

MidiRouter::ChannelMap::ChannelMap() {
  for (std::size_t i = 0; i < inbound.size(); ++i) {
    inbound[i] = static_cast<std::uint8_t>(i);
    outbound[i] = static_cast<std::uint8_t>(i);
  }
}

class MidiRouter::Backend {
public:
  Backend(MidiRouter& router, Port port) : router_(router), port_(port) {}
  virtual ~Backend() = default;

  Port port() const { return port_; }

  virtual PortInfo describe() const = 0;
  virtual void begin() = 0;
  virtual void poll() = 0;
  virtual void sendClock() = 0;
  virtual void sendStart() = 0;
  virtual void sendStop() = 0;
  virtual void sendControlChange(std::uint8_t channel, std::uint8_t controller,
                                 std::uint8_t value) = 0;
  virtual void sendNoteOn(std::uint8_t channel, std::uint8_t note, std::uint8_t velocity) = 0;
  virtual void sendNoteOff(std::uint8_t channel, std::uint8_t note, std::uint8_t velocity) = 0;
  virtual void sendAllNotesOff(std::uint8_t channel) = 0;

protected:
  MidiRouter& router_;
  Port port_;
};

#if SEEDBOX_HW
class MidiRouter::UsbMidiBackend : public MidiRouter::Backend {
public:
  explicit UsbMidiBackend(MidiRouter& router)
      : MidiRouter::Backend(router, MidiRouter::Port::kUsb) {}

  PortInfo describe() const override {
    PortInfo info{};
    info.label = "USB";
    info.available = true;
    info.clockIn = true;
    info.clockOut = true;
    info.transportIn = true;
    info.transportOut = true;
    info.controlChangeIn = true;
    info.controlChangeOut = true;
    return info;
  }

  void begin() override {}

  void poll() override {
    if constexpr (SeedBoxConfig::kQuietMode) {
      while (usbMIDI.read()) {
        // Drain packets to keep the USB stack happy but avoid dispatching to the
        // app.  Quiet builds are typically unit tests poking the CLI backend.
      }
      return;
    }
    while (usbMIDI.read()) {
      const auto type = usbMIDI.getType();
      switch (type) {
        case midi::Clock:
          router_.handleClockFrom(port_);
          break;
        case midi::Start:
          router_.handleStartFrom(port_);
          break;
        case midi::Stop:
          router_.handleStopFrom(port_);
          break;
        case midi::ControlChange: {
          const std::uint8_t raw_channel = usbMIDI.getChannel();
          const std::uint8_t channel =
              seedbox::interop::mn42::NormalizeUsbChannel(raw_channel);
          const std::uint8_t controller = usbMIDI.getData1();
          const std::uint8_t value = usbMIDI.getData2();
          router_.handleControlChangeFrom(port_, channel, controller, value);
          break;
        }
        case midi::SystemExclusive: {
          const std::uint8_t* data = usbMIDI.getSysExArray();
          const std::uint32_t len = usbMIDI.getSysExArrayLength();
          router_.handleSysExFrom(port_, data, static_cast<std::size_t>(len));
          break;
        }
        default:
          break;
      }
    }
  }

  void sendClock() override {
    if constexpr (SeedBoxConfig::kQuietMode) {
      return;
    }
    usbMIDI.sendRealTime(midi::Clock);
  }

  void sendStart() override {
    if constexpr (SeedBoxConfig::kQuietMode) {
      return;
    }
    usbMIDI.sendRealTime(midi::Start);
  }

  void sendStop() override {
    if constexpr (SeedBoxConfig::kQuietMode) {
      return;
    }
    usbMIDI.sendRealTime(midi::Stop);
  }

  void sendControlChange(std::uint8_t channel, std::uint8_t controller,
                         std::uint8_t value) override {
    if constexpr (SeedBoxConfig::kQuietMode) {
      return;
    }
    usbMIDI.sendControlChange(controller, value, static_cast<std::uint8_t>(channel + 1));
  }

  void sendNoteOn(std::uint8_t channel, std::uint8_t note, std::uint8_t velocity) override {
    if constexpr (SeedBoxConfig::kQuietMode) {
      return;
    }
    usbMIDI.sendNoteOn(note, velocity, static_cast<std::uint8_t>(channel + 1));
  }

  void sendNoteOff(std::uint8_t channel, std::uint8_t note, std::uint8_t velocity) override {
    if constexpr (SeedBoxConfig::kQuietMode) {
      return;
    }
    usbMIDI.sendNoteOff(note, velocity, static_cast<std::uint8_t>(channel + 1));
  }

  void sendAllNotesOff(std::uint8_t channel) override {
    if constexpr (SeedBoxConfig::kQuietMode) {
      return;
    }
    usbMIDI.sendControlChange(123, 0, static_cast<std::uint8_t>(channel + 1));
  }
};

class MidiRouter::TrsAMidiBackend : public MidiRouter::Backend {
public:
  explicit TrsAMidiBackend(MidiRouter& router)
      : MidiRouter::Backend(router, MidiRouter::Port::kTrsA) {}

  PortInfo describe() const override {
    PortInfo info{};
    info.label = "TRS-A";
    info.available = true;
    info.clockIn = true;
    info.clockOut = true;
    info.transportIn = true;
    info.transportOut = true;
    info.controlChangeIn = true;
    info.controlChangeOut = true;
    return info;
  }

  void begin() override {
    if constexpr (SeedBoxConfig::kQuietMode) {
      hal::midi::serial7::Handlers handlers{};
      hal::midi::serial7::begin(handlers, nullptr);
      return;
    }

    hal::midi::serial7::Handlers handlers{};
    handlers.clock = [](void* ctx) {
      static_cast<MidiRouter::TrsAMidiBackend*>(ctx)->router_.handleClockFrom(Port::kTrsA);
    };
    handlers.start = [](void* ctx) {
      static_cast<MidiRouter::TrsAMidiBackend*>(ctx)->router_.handleStartFrom(Port::kTrsA);
    };
    handlers.stop = [](void* ctx) {
      static_cast<MidiRouter::TrsAMidiBackend*>(ctx)->router_.handleStopFrom(Port::kTrsA);
    };
    handlers.control_change = [](std::uint8_t channel, std::uint8_t controller,
                                 std::uint8_t value, void* ctx) {
      static_cast<MidiRouter::TrsAMidiBackend*>(ctx)->router_.handleControlChangeFrom(
          Port::kTrsA, channel, controller, value);
    };
    hal::midi::serial7::begin(handlers, this);
  }

  void poll() override {
    if constexpr (SeedBoxConfig::kQuietMode) {
      return;
    }
    hal::midi::serial7::poll();
  }

  void sendClock() override {
    if constexpr (SeedBoxConfig::kQuietMode) {
      return;
    }
    hal::midi::serial7::sendClock();
  }

  void sendStart() override {
    if constexpr (SeedBoxConfig::kQuietMode) {
      return;
    }
    hal::midi::serial7::sendStart();
  }

  void sendStop() override {
    if constexpr (SeedBoxConfig::kQuietMode) {
      return;
    }
    hal::midi::serial7::sendStop();
  }

  void sendControlChange(std::uint8_t channel, std::uint8_t controller,
                         std::uint8_t value) override {
    if constexpr (SeedBoxConfig::kQuietMode) {
      return;
    }
    hal::midi::serial7::sendControlChange(channel, controller, value);
  }

  void sendNoteOn(std::uint8_t channel, std::uint8_t note, std::uint8_t velocity) override {
    if constexpr (SeedBoxConfig::kQuietMode) {
      return;
    }
    hal::midi::serial7::sendNoteOn(channel, note, velocity);
  }

  void sendNoteOff(std::uint8_t channel, std::uint8_t note, std::uint8_t velocity) override {
    if constexpr (SeedBoxConfig::kQuietMode) {
      return;
    }
    hal::midi::serial7::sendNoteOff(channel, note, velocity);
  }

  void sendAllNotesOff(std::uint8_t channel) override {
    if constexpr (SeedBoxConfig::kQuietMode) {
      return;
    }
    hal::midi::serial7::sendAllNotesOff(channel);
  }
};
#else  // !SEEDBOX_HW

MidiRouter::CliBackend::CliBackend(MidiRouter& router, Port port)
    : router_(router), port_(port) {}

void MidiRouter::CliBackend::pushClock() {
  queue_.push_back({Event::Type::kClock});
}

void MidiRouter::CliBackend::pushStart() {
  queue_.push_back({Event::Type::kStart});
}

void MidiRouter::CliBackend::pushStop() {
  queue_.push_back({Event::Type::kStop});
}

void MidiRouter::CliBackend::pushControlChange(std::uint8_t channel, std::uint8_t controller,
                                               std::uint8_t value) {
  Event ev{};
  ev.type = Event::Type::kControlChange;
  ev.data1 = channel;
  ev.data2 = controller;
  ev.data3 = value;
  queue_.push_back(ev);
}

void MidiRouter::CliBackend::pushSysEx(const std::vector<std::uint8_t>& payload) {
  Event ev{};
  ev.type = Event::Type::kSysEx;
  ev.sysex = payload;
  queue_.push_back(ev);
}

void MidiRouter::CliBackend::clearSent() { sent_.clear(); }

void MidiRouter::CliBackend::begin() {
  clearSent();
  queue_.clear();
}

void MidiRouter::CliBackend::poll() {
  while (!queue_.empty()) {
    Event ev = queue_.front();
    queue_.erase(queue_.begin());
    switch (ev.type) {
      case Event::Type::kClock:
        router_.handleClockFrom(port_);
        break;
      case Event::Type::kStart:
        router_.handleStartFrom(port_);
        break;
      case Event::Type::kStop:
        router_.handleStopFrom(port_);
        break;
      case Event::Type::kControlChange:
        router_.handleControlChangeFrom(port_, ev.data1, ev.data2, ev.data3);
        break;
      case Event::Type::kSysEx:
        router_.handleSysExFrom(port_, ev.sysex.data(), ev.sysex.size());
        break;
    }
  }
}

void MidiRouter::CliBackend::sendClock() { sent_.push_back({SentMessage::Type::kClock}); }

void MidiRouter::CliBackend::sendStart() { sent_.push_back({SentMessage::Type::kStart}); }

void MidiRouter::CliBackend::sendStop() { sent_.push_back({SentMessage::Type::kStop}); }

void MidiRouter::CliBackend::sendControlChange(std::uint8_t channel, std::uint8_t controller,
                                               std::uint8_t value) {
  sent_.push_back({SentMessage::Type::kControlChange, channel, controller, value});
}

void MidiRouter::CliBackend::sendNoteOn(std::uint8_t channel, std::uint8_t note,
                                        std::uint8_t velocity) {
  sent_.push_back({SentMessage::Type::kNoteOn, channel, note, velocity});
}

void MidiRouter::CliBackend::sendNoteOff(std::uint8_t channel, std::uint8_t note,
                                         std::uint8_t velocity) {
  sent_.push_back({SentMessage::Type::kNoteOff, channel, note, velocity});
}

void MidiRouter::CliBackend::sendAllNotesOff(std::uint8_t channel) {
  sent_.push_back({SentMessage::Type::kAllNotesOff, channel});
}

class MidiRouter::CliBackendAdapter : public MidiRouter::Backend {
public:
  explicit CliBackendAdapter(MidiRouter::CliBackend& backend)
      : MidiRouter::Backend(backend.router_, backend.port_), backend_(backend) {}

  MidiRouter::PortInfo describe() const override {
    MidiRouter::PortInfo info{};
    info.label = backend_.port_ == MidiRouter::Port::kUsb ? "USB" : "TRS-A";
    info.available = true;
    info.clockIn = true;
    info.clockOut = true;
    info.transportIn = true;
    info.transportOut = true;
    info.controlChangeIn = true;
    info.controlChangeOut = true;
    return info;
  }

  void begin() override { backend_.begin(); }
  void poll() override { backend_.poll(); }
  void sendClock() override { backend_.sendClock(); }
  void sendStart() override { backend_.sendStart(); }
  void sendStop() override { backend_.sendStop(); }
  void sendControlChange(std::uint8_t ch, std::uint8_t cc, std::uint8_t val) override {
    backend_.sendControlChange(ch, cc, val);
  }
  void sendNoteOn(std::uint8_t ch, std::uint8_t note, std::uint8_t velocity) override {
    backend_.sendNoteOn(ch, note, velocity);
  }
  void sendNoteOff(std::uint8_t ch, std::uint8_t note, std::uint8_t velocity) override {
    backend_.sendNoteOff(ch, note, velocity);
  }
  void sendAllNotesOff(std::uint8_t ch) override { backend_.sendAllNotesOff(ch); }

private:
  MidiRouter::CliBackend& backend_;
};

#endif  // SEEDBOX_HW

MidiRouter::MidiRouter() {
  for (auto& routeMatrix : pageRouting_) {
    for (auto& route : routeMatrix) {
      route = RouteConfig{};
      route.acceptControlChange = true;
    }
  }

  clearNoteState();

  ports_[portIndex(Port::kUsb)].info.label = "USB";
  ports_[portIndex(Port::kTrsA)].info.label = "TRS-A";

#if SEEDBOX_HW
  ports_[portIndex(Port::kUsb)].info.available = true;
  ports_[portIndex(Port::kUsb)].info.clockIn = true;
  ports_[portIndex(Port::kUsb)].info.clockOut = true;
  ports_[portIndex(Port::kUsb)].info.transportIn = true;
  ports_[portIndex(Port::kUsb)].info.transportOut = true;
  ports_[portIndex(Port::kUsb)].info.controlChangeIn = true;
  ports_[portIndex(Port::kUsb)].info.controlChangeOut = true;

  ports_[portIndex(Port::kTrsA)].info.available = true;
  ports_[portIndex(Port::kTrsA)].info.clockIn = true;
  ports_[portIndex(Port::kTrsA)].info.clockOut = true;
  ports_[portIndex(Port::kTrsA)].info.transportIn = true;
  ports_[portIndex(Port::kTrsA)].info.transportOut = true;
  ports_[portIndex(Port::kTrsA)].info.controlChangeIn = true;
  ports_[portIndex(Port::kTrsA)].info.controlChangeOut = true;
#else
  ports_[portIndex(Port::kUsb)].info.available = true;
  ports_[portIndex(Port::kUsb)].info.clockIn = true;
  ports_[portIndex(Port::kUsb)].info.clockOut = true;
  ports_[portIndex(Port::kUsb)].info.transportIn = true;
  ports_[portIndex(Port::kUsb)].info.transportOut = true;
  ports_[portIndex(Port::kUsb)].info.controlChangeIn = true;
  ports_[portIndex(Port::kUsb)].info.controlChangeOut = true;

  ports_[portIndex(Port::kTrsA)].info.available = true;
  ports_[portIndex(Port::kTrsA)].info.clockIn = true;
  ports_[portIndex(Port::kTrsA)].info.clockOut = true;
  ports_[portIndex(Port::kTrsA)].info.transportIn = true;
  ports_[portIndex(Port::kTrsA)].info.transportOut = true;
  ports_[portIndex(Port::kTrsA)].info.controlChangeIn = true;
  ports_[portIndex(Port::kTrsA)].info.controlChangeOut = true;
#endif
}

MidiRouter::~MidiRouter() = default;

void MidiRouter::clearNoteState() {
  for (auto& port : ports_) {
    for (auto& channel : port.activeNotes) {
      channel.reset();
    }
  }
}

void MidiRouter::begin() {
#if SEEDBOX_HW
  if (!backends_[portIndex(Port::kUsb)]) {
    backends_[portIndex(Port::kUsb)] = std::make_unique<UsbMidiBackend>(*this);
  }
  if (!backends_[portIndex(Port::kTrsA)]) {
    backends_[portIndex(Port::kTrsA)] = std::make_unique<TrsAMidiBackend>(*this);
  }
#else
  if (!backends_[portIndex(Port::kUsb)]) {
    backends_[portIndex(Port::kUsb)] =
        std::make_unique<CliBackendAdapter>(*cliBackend(Port::kUsb));
  }
  if (!backends_[portIndex(Port::kTrsA)]) {
    backends_[portIndex(Port::kTrsA)] =
        std::make_unique<CliBackendAdapter>(*cliBackend(Port::kTrsA));
  }
#endif

  for (std::size_t i = 0; i < kPortCount; ++i) {
    if (backends_[i]) {
      ports_[i].info = backends_[i]->describe();
      backends_[i]->begin();
    } else {
      ports_[i].info.available = false;
    }
  }

  // Activate whatever routing the current page requested.
  activatePage(activePage_);

  clearNoteState();

  mn42HelloSeen_ = false;
  mn42AppReady_ = false;
  mn42AckSent_ = false;
  mn42LastKeepAliveMs_ = nowMs();
}

void MidiRouter::poll() {
  maybeSendMn42KeepAlive();
  for (auto& backend : backends_) {
    if (backend) {
      backend->poll();
    }
  }
}

const MidiRouter::PortInfo& MidiRouter::portInfo(Port port) const {
  return ports_[portIndex(port)].info;
}

void MidiRouter::configurePortRouting(Port port, const RouteConfig& config) {
  ports_[portIndex(port)].route = config;
}

void MidiRouter::configurePageRouting(
    Page page, const std::array<RouteConfig, kPortCount>& matrix) {
  pageRouting_[static_cast<std::size_t>(page)] = matrix;
  if (page == activePage_) {
    activatePage(page);
  }
}

void MidiRouter::activatePage(Page page) {
  activePage_ = page;
  const auto& matrix = pageRouting_[static_cast<std::size_t>(page)];
  for (std::size_t i = 0; i < kPortCount; ++i) {
    ports_[i].route = matrix[i];
  }
}

void MidiRouter::setChannelMap(Port port, const ChannelMap& map) {
  ports_[portIndex(port)].channels = map;
}

const MidiRouter::ChannelMap& MidiRouter::channelMap(Port port) const {
  return ports_[portIndex(port)].channels;
}

void MidiRouter::sendNoteOn(Port port, std::uint8_t channel, std::uint8_t note,
                            std::uint8_t velocity) {
  const std::size_t idx = portIndex(port);
  if (!ports_[idx].info.available) {
    return;
  }
  const std::uint8_t mappedChannel = ports_[idx].channels.outbound[sanitizeChannel(channel)];
  ports_[idx].activeNotes[mappedChannel].set(note);
  if (auto* backend = backends_[idx].get()) {
    backend->sendNoteOn(mappedChannel, note, velocity);
  }
}

void MidiRouter::sendNoteOff(Port port, std::uint8_t channel, std::uint8_t note,
                             std::uint8_t velocity) {
  const std::size_t idx = portIndex(port);
  if (!ports_[idx].info.available) {
    return;
  }
  const std::uint8_t mappedChannel = ports_[idx].channels.outbound[sanitizeChannel(channel)];
  auto& channelState = ports_[idx].activeNotes[mappedChannel];
  if (ports_[idx].noteGuardEnabled && !channelState.test(note)) {
    return;
  }
  channelState.reset(note);
  if (auto* backend = backends_[idx].get()) {
    backend->sendNoteOff(mappedChannel, note, velocity);
  }
}

void MidiRouter::sendControlChange(Port port, std::uint8_t channel, std::uint8_t cc,
                                   std::uint8_t value) {
  const std::size_t idx = portIndex(port);
  if (!ports_[idx].info.available) {
    return;
  }
  const std::uint8_t mappedChannel = ports_[idx].channels.outbound[sanitizeChannel(channel)];
  if (auto* backend = backends_[idx].get()) {
    backend->sendControlChange(mappedChannel, cc, value);
  }
}

void MidiRouter::sendStart(Port port) {
  const std::size_t idx = portIndex(port);
  if (auto* backend = backends_[idx].get()) {
    backend->sendStart();
  }
}

void MidiRouter::sendStop(Port port) {
  const std::size_t idx = portIndex(port);
  if (auto* backend = backends_[idx].get()) {
    backend->sendStop();
  }
}

void MidiRouter::sendClock(Port port) {
  const std::size_t idx = portIndex(port);
  if (auto* backend = backends_[idx].get()) {
    backend->sendClock();
  }
}

void MidiRouter::panic() {
  for (std::size_t i = 0; i < kPortCount; ++i) {
    if (!ports_[i].info.available) {
      continue;
    }
    if (auto* backend = backends_[i].get()) {
      for (std::uint8_t channel = 0; channel < 16; ++channel) {
        if (ports_[i].activeNotes[channel].any()) {
          backend->sendAllNotesOff(channel);
        }
      }
    }
  }
  clearNoteState();
}

void MidiRouter::onControlChange(std::uint8_t ch, std::uint8_t cc, std::uint8_t val) {
  if (controlChangeHandler_) {
    controlChangeHandler_(ch, cc, val);
  }
}

SEEDBOX_MAYBE_UNUSED void MidiRouter::markAppReady() {
  mn42AppReady_ = true;
  mn42LastKeepAliveMs_ = nowMs();
  if (mn42HelloSeen_ && !mn42AckSent_) {
    sendMn42Handshake(seedbox::interop::mn42::handshake::kAck);
  }
}

#if !SEEDBOX_HW
MidiRouter::CliBackend* MidiRouter::cliBackend(Port port) {
  const std::size_t idx = portIndex(port);
  if (!cliBackends_[idx]) {
    cliBackends_[idx] = std::unique_ptr<CliBackend>(new CliBackend(*this, port));
  }
  return cliBackends_[idx].get();
}
#endif

void MidiRouter::handleClockFrom(Port port) {
  const std::size_t idx = portIndex(port);
  if (!ports_[idx].route.acceptClock) {
    return;
  }
  if (clockHandler_) {
    clockHandler_();
  }
  for (std::size_t i = 0; i < kPortCount; ++i) {
    if (i == idx) {
      continue;
    }
    if (ports_[i].route.mirrorClock && backends_[i]) {
      backends_[i]->sendClock();
    }
  }
}

void MidiRouter::handleStartFrom(Port port) {
  const std::size_t idx = portIndex(port);
  if (!ports_[idx].route.acceptTransport) {
    return;
  }
  if (startHandler_) {
    startHandler_();
  }
  for (std::size_t i = 0; i < kPortCount; ++i) {
    if (i == idx) {
      continue;
    }
    if (ports_[i].route.mirrorTransport && backends_[i]) {
      backends_[i]->sendStart();
    }
  }
}

void MidiRouter::handleStopFrom(Port port) {
  const std::size_t idx = portIndex(port);
  if (!ports_[idx].route.acceptTransport) {
    return;
  }
  if (stopHandler_) {
    stopHandler_();
  }
  for (std::size_t i = 0; i < kPortCount; ++i) {
    if (i == idx) {
      continue;
    }
    if (ports_[i].route.mirrorTransport && backends_[i]) {
      backends_[i]->sendStop();
    }
  }
}

void MidiRouter::handleControlChangeFrom(Port port, std::uint8_t ch, std::uint8_t cc,
                                         std::uint8_t val) {
  const std::size_t idx = portIndex(port);
  if (!ports_[idx].route.acceptControlChange) {
    return;
  }
  const std::uint8_t mappedChannel = ports_[idx].channels.inbound[sanitizeChannel(ch)];
  handleMn42ControlChange(mappedChannel, cc, val);
  onControlChange(mappedChannel, cc, val);
}

void MidiRouter::handleSysExFrom(Port port, const std::uint8_t* data, std::size_t len) {
  (void)port;
  handleMn42SysEx(data, len);
  if (sysexHandler_) {
    sysexHandler_(data, len);
  }
}

void MidiRouter::handleMn42ControlChange(std::uint8_t ch, std::uint8_t cc, std::uint8_t val) {
  const bool isMn42Handshake =
      (ch == seedbox::interop::mn42::kDefaultChannel &&
       cc == seedbox::interop::mn42::cc::kHandshake);

  if (isMn42Handshake && val == seedbox::interop::mn42::handshake::kHello) {
    mn42AckSent_ = false;
  }

  if (!isMn42Handshake) {
    return;
  }

  using namespace seedbox::interop::mn42;
  if (val == handshake::kHello) {
    mn42HelloSeen_ = true;
    mn42LastKeepAliveMs_ = nowMs();
    if (mn42AppReady_ && !mn42AckSent_) {
      sendMn42Handshake(handshake::kAck);
    }
  } else if (val == handshake::kKeepAlive) {
    mn42LastKeepAliveMs_ = nowMs();
  }
}

void MidiRouter::handleMn42SysEx(const std::uint8_t* data, std::size_t len) {
  if (!data || len < 7) {
    return;
  }
  if (data[0] != 0xF0 || data[len - 1] != 0xF7) {
    return;
  }
  if (data[1] != seedbox::interop::mn42::handshake::kManufacturerId) {
    return;
  }
  if (len < 6) {
    return;
  }
  if (data[2] != seedbox::interop::mn42::handshake::kProductMajor ||
      data[3] != seedbox::interop::mn42::handshake::kProductMinor ||
      data[4] != seedbox::interop::mn42::handshake::kProductRevision) {
    return;
  }
  mn42HelloSeen_ = true;
  mn42LastKeepAliveMs_ = nowMs();
  if (mn42AppReady_ && !mn42AckSent_) {
    sendMn42Handshake(seedbox::interop::mn42::handshake::kAck);
  }
}

void MidiRouter::sendMn42Handshake(std::uint8_t value) {
  sendControlChange(Port::kUsb, seedbox::interop::mn42::kDefaultChannel,
                    seedbox::interop::mn42::cc::kHandshake, value);
  if (value == seedbox::interop::mn42::handshake::kAck ||
      value == seedbox::interop::mn42::handshake::kKeepAlive) {
    mn42AckSent_ = true;
    mn42LastKeepAliveMs_ = nowMs();
  }
}

void MidiRouter::maybeSendMn42KeepAlive() {
  if (!mn42AppReady_ || !mn42AckSent_ || !mn42HelloSeen_) {
    return;
  }
  const std::uint32_t now = nowMs();
  if (now - mn42LastKeepAliveMs_ >= kMn42KeepAliveIntervalMs) {
    sendMn42Handshake(seedbox::interop::mn42::handshake::kKeepAlive);
  }
}

std::uint32_t MidiRouter::nowMs() const {
#if SEEDBOX_HW
  return millis();
#else
  return 0;
#endif
}
