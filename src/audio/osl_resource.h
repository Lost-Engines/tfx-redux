#pragma once

#include <array>
#include "resources/binary.h"

namespace tfx::audio {

unsigned int constexpr osl_channels{16};
enum class osl_device : uint8_t {
  fm = 2,
  lapc = 4,
  sound_canvas = 8
};
struct osl_cell {
  std::array<uint8_t, 4> notes{};
  uint8_t instrument{0};
  uint8_t effect{0};
  uint8_t argument{0};
  uint8_t auxiliary{0};
};
using osl_row = std::array<osl_cell, osl_channels>;
struct osl_pattern {
  std::span<uint8_t const> bytes;
  uint16_t flags;
  uint16_t rows;
  uint16_t stored_channels;
};
class osl_pattern_reader {
private:
  osl_pattern pattern;
  unsigned int position{0};
  unsigned int row{0};

public:
  explicit osl_pattern_reader(osl_pattern pattern);
  osl_row next(unsigned int channels);
  unsigned int offset() const;
  unsigned int rows_read() const;
};
struct osl_song {
  std::span<uint8_t const> resource;
  unsigned int offset;
  unsigned int channels() const;
  unsigned int order_count() const;
  unsigned int loop_order() const;
  unsigned int pattern_count() const;
  uint16_t clock_rate() const;
  uint8_t speed() const;
  uint8_t transpose() const;
  uint8_t channel_volume(unsigned int channel) const;
  uint8_t order(unsigned int index) const;
  osl_pattern pattern(unsigned int index) const;
};
class osl_resource {
private:
  resources::bytes bytes;

public:
  explicit osl_resource(resources::bytes source);
  osl_device device() const;
  unsigned int song_count() const;
  osl_song song(unsigned int index) const;
  unsigned int instrument_count() const;
  std::span<uint8_t const> instrument(unsigned int index) const;
  std::span<uint8_t const> instrument_parameters(unsigned int index) const;
};

} // namespace tfx::audio
