#include "music_device.h"
#include "audio/fm_synth.h"
#include "audio/midi_stream.h"
#include "audio/osl_la_driver.h"
#include "audio/osl_scc_driver.h"

namespace tfx::audio {
namespace {

class fm_music_device final : public music_device {
private:
  fm_synth synth;
  osl_fm_driver callbacks;

public:
  fm_music_device(std::span<uint8_t const> const launcher, unsigned int const rate)
    :
    synth{rate},
      callbacks{launcher, [this](opl_write const command){synth.write(command);}} {}
  osl_device_driver &driver() override {
    return callbacks;
  }
  void render(std::span<float> const stereo) override {
    synth.render(stereo);
  }
};

template<class Driver>
class midi_music_device final : public music_device {
private:
  std::unique_ptr<midi_receiver> receiver;
  midi_stream stream;
  Driver callbacks;

public:
  midi_music_device(std::span<uint8_t const> const launcher, std::unique_ptr<midi_receiver> receiver)
    :
    receiver{std::move(receiver)},
      stream{*this->receiver},
    callbacks{launcher, [this](uint8_t const byte){stream.byte(byte);}} {}
  osl_device_driver &driver() override {
    return callbacks;
  }
  void render(std::span<float> const stereo) override {
    receiver->render(stereo);
  }
  unsigned int reset_settling_frames(unsigned int const rate) const override {
    // The Sound Canvas firmware consumes serial MIDI. Run its queued GS setup
    // to completion before the first song, outside the output callback.
    return callbacks.device_type() == osl_device::sound_canvas ? rate : rate / 10;
  }
};

} // anonymous namespace

std::unique_ptr<music_device> make_fm_music_device(std::span<uint8_t const> const launcher, unsigned int const sample_rate) {
  return std::make_unique<fm_music_device>(launcher, sample_rate);
}
std::unique_ptr<music_device> make_scc_music_device(std::span<uint8_t const> const launcher,
    std::unique_ptr<midi_receiver> receiver) {
  if(!receiver) throw std::invalid_argument{"music device requires a MIDI receiver"};
  return std::make_unique<midi_music_device<osl_scc_driver>>(launcher, std::move(receiver));
}
std::unique_ptr<music_device> make_la_music_device(std::span<uint8_t const> const launcher,
    std::unique_ptr<midi_receiver> receiver) {
  if(!receiver) throw std::invalid_argument{"music device requires a MIDI receiver"};
  return std::make_unique<midi_music_device<osl_la_driver>>(launcher, std::move(receiver));
}

} // namespace tfx::audio
