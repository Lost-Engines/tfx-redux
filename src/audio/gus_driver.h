#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <span>

namespace tfx::audio {

struct gus_parameters {
  static unsigned int constexpr maximum_volume{256};
  static unsigned int constexpr volume_count{maximum_volume + 1};
  uint16_t frequency_base;
  std::array<uint16_t, volume_count> volumes;
};
gus_parameters load_gus_parameters(std::span<uint8_t const> firmware);

struct gus_port_write {
  uint16_t port;
  uint8_t size;
  uint16_t value;
};
struct gus_bus {
  std::function<uint16_t(uint16_t, uint8_t)> read;
  std::function<void(gus_port_write const &)> write;
};
enum class gus_register : uint8_t {
  voice_control,
  frequency,
  loop_begin_high,
  loop_begin_low,
  loop_end_high,
  loop_end_low,
  ramp_rate,
  ramp_begin,
  ramp_end,
  volume,
  current_high,
  current_low,
  pan,
  ramp_control,
  active_voices,
  voice_status = 0x80,
  ramp_status = 0x8d,
};

// The GUS.COM resident playback/volume/stop operations, expressed as port I/O.
// Its tables are loaded from the user's driver; no native instructions execute.
class gus_driver {
private:
  gus_parameters parameters;
  gus_bus bus;
  static uint8_t constexpr ramp_pcm_bit{4};
  static uint8_t constexpr wave_irq_bit{0x20};
  static uint8_t constexpr wave_reverse_bit{0x40};
  static uint8_t constexpr wave_stopped_mask{3};
  void output(uint16_t port, uint8_t size, uint16_t value);
  void select(unsigned int voice);
  void byte(gus_register address, uint8_t value);
  void word(gus_register address, uint16_t value);
  uint8_t read(gus_register address);
  void repeated_byte(gus_register address, uint8_t value);
  void position(gus_register address, uint32_t bytes);

public:
  static unsigned int constexpr physical_voices{32};
  static uint16_t constexpr card_base{0x240};
  static uint16_t constexpr register_base{card_base + 0x100};
  static uint16_t constexpr voice_select{register_base + 2};
  static uint16_t constexpr register_select{register_base + 3};
  static uint16_t constexpr data_low{register_base + 4};
  static uint16_t constexpr data_high{register_base + 5};
  gus_driver(gus_parameters const &parameters, gus_bus bus);
  uint16_t frequency_control(uint32_t rate) const;
  void volume(unsigned int voice, unsigned int value);
  bool stopped(unsigned int voice);
  void play(unsigned int voice, uint32_t begin, uint32_t end, uint32_t rate);
  void stop(unsigned int voice);
};

} // namespace tfx::audio
