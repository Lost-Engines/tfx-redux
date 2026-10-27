#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "qsound/spatial_model.h"
#include "audio/qsound_effect.h"
#include "audio/sample_voice.h"
#include "simulation/timing.h"

namespace {

using bytes = tfx::resources::bytes;

void require(bool const condition, char const *message) {
  if(!condition) throw std::runtime_error{message};
}

void integer(bytes &output, uint32_t const value, unsigned int const count = 2) {
  for(unsigned int byte{}; byte != count; ++byte) output.push_back(static_cast<uint8_t>(value >> (byte * 8)));
}

tfx::resources::archive fixture_samples() {
  bytes data(4);
  std::array<unsigned int, 4> constexpr sizes{2000, 2500, 1300, 1500};
  std::array<unsigned int, 4> offsets{};
  for(unsigned int index{}; index != sizes.size(); ++index) {
    offsets[index] = static_cast<unsigned int>(data.size());
    for(unsigned int sample{}; sample != sizes[index]; ++sample) data.push_back(static_cast<uint8_t>(sample * 37 + index * 73));
  }
  auto const table{static_cast<uint32_t>(data.size())};
  for(unsigned int byte{}; byte != 4; ++byte) data[byte] = static_cast<uint8_t>(table >> (byte * 8));
  integer(data, static_cast<uint32_t>(sizes.size()));
  for(unsigned int count{}; count != 3; ++count) integer(data, 0);
  for(unsigned int index{}; index != sizes.size(); ++index) {
    integer(data, tfx::resources::hash_short_name("samples\\" + std::to_string(index) + "sam"), 4);
    integer(data, offsets[index], 4);
    integer(data, sizes[index], 4);
  }
  return {std::move(data)};
}

void verify_timer() {
  unsigned int constexpr rate{48000};
  auto const tables{qsound::make_spatial_tables(8000)};
  qsound::spatial_model expected{tables, {
    .rounding{qsound::rounding_model::floor},
    .accumulation{qsound::accumulation_model::saturate_each_mac},
  }, qsound::position_policy::clamp_to_endpoints};
  tfx::audio::qsound_effect actual{rate};
  tfx::audio::sample_pan parameters{
    .position{14 * 256},
    .increment{1024},
    .movement_enable{1},
  };
  actual.select(parameters);
  uint64_t previous_callbacks{};
  uint16_t phase{parameters.position};
  uint8_t cached{255};
  for(unsigned int frame{}; frame != rate; ++frame) {
    if(frame == 16000) {
      parameters = {
        .position{33 * 256},
        .increment{},
        .movement_enable{},
      };
      phase = parameters.position;
      actual.select(parameters);
    }
    if(frame == 18000) {
      parameters = {
        .position{1 * 256},
        .increment{-113},
        .movement_enable{48},
      };
      phase = parameters.position;
      actual.select(parameters);
    }
    uint64_t const irqs{static_cast<uint64_t>(frame) * tfx::simulation::pit_input_hz / (rate * static_cast<uint64_t>(tfx::simulation::pit_divisor))};
    auto const callbacks{(irqs * 50 + 136) / 137};
    while(previous_callbacks != callbacks) {
      ++previous_callbacks;
      auto const position{static_cast<uint8_t>(phase >> 8)};
      if(position != cached) {
        expected.set_positions(position, 16);
        cached = position;
      }
      if(parameters.movement_enable) phase = static_cast<uint16_t>(phase + parameters.increment);
    }
    actual.advance_frame();
    if(frame % 11 == 0) {
      auto const sample{static_cast<uint8_t>(frame * 37)};
      auto const result{actual.process(sample)};
      auto const reference{expected.process({
        .left{static_cast<int16_t>((static_cast<int>(sample) - 128) * 256)},
        .right{},
      })};
      require(result.left == reference.left / 32768.0f && result.right == reference.right / 32768.0f,
        "QSound publication cadence, persistent history or byte conversion changed");
    }
  }
}

void verify_playback() {
  auto const archive{fixture_samples()};
  std::array<tfx::audio::sample_definition, 4> const definitions{{
    {
      .name{"0"},
      .rate{8363},
      .retrigger_delay{},
      .qsound_parameters{
        .position{},
        .increment{},
        .movement_enable{},
      },
    },
    {
      .name{"1"},
      .rate{7500},
      .retrigger_delay{},
      .qsound_parameters{
        .position{33 * 256},
        .increment{},
        .movement_enable{},
      },
    },
    {
      .name{"2"},
      .rate{8363},
      .retrigger_delay{},
      .qsound_parameters{
        .position{14 * 256},
        .increment{1024},
        .movement_enable{1},
      },
    },
    {
      .name{"3"},
      .rate{6500},
      .retrigger_delay{},
      .qsound_parameters{
        .position{32 * 256},
        .increment{-113},
        .movement_enable{48},
      },
    },
  }};
  tfx::audio::sample_bank const bank{definitions, archive};
  auto const recovered{qsound::make_spatial_tables(8000)};
  for(unsigned int const rate : {8000u, 44100u, 48000u}) {
    // Before the first PIT interrupt, gains remain at the default centre.
    // Independently resample native FIR outputs with the documented 119-us
    // DAC hold. This detects filtering repeated host frames or losing skipped
    // native samples at the 8-kHz host rate.
    qsound::spatial_model native{recovered, {
      .rounding{qsound::rounding_model::floor},
      .accumulation{qsound::accumulation_model::saturate_each_mac},
    }};
    tfx::audio::sample_voice held_voice{bank, rate, std::make_unique<tfx::audio::qsound_effect>(rate)};
    std::array<unsigned int, 1> constexpr first_sample{0};
    held_voice.submit(tfx::audio::sample_priority::normal, first_sample);
    unsigned int const frames{rate / 100};
    std::vector<float> held(frames * 2);
    held_voice.render(held);
    unsigned int next{};
    qsound::stereo_sample expected{};
    unsigned int constexpr sample_period_us{119};
    for(unsigned int frame{}; frame != frames; ++frame) {
      auto const index{static_cast<uint64_t>(frame) * 1'000'000 / (sample_period_us * static_cast<uint64_t>(rate))};
      while(next <= index) {
        expected = native.process({
          .left{static_cast<int16_t>((static_cast<int>(bank.pcm(0)[next++]) - 128) * 256)},
          .right{},
        });
      }
      require(held[frame * 2] == expected.left / 32768.0f && held[frame * 2 + 1] == expected.right / 32768.0f,
        "QSound filter clock or DAC sample holds changed");
    }
    tfx::audio::sample_voice contiguous{bank, rate, std::make_unique<tfx::audio::qsound_effect>(rate)};
    tfx::audio::sample_voice fragmented{bank, rate, std::make_unique<tfx::audio::qsound_effect>(rate)};
    std::array<unsigned int, 4> constexpr sequence{0, 1, 2, 3};
    contiguous.submit(tfx::audio::sample_priority::normal, sequence);
    fragmented.submit(tfx::audio::sample_priority::normal, sequence);
    std::vector<float> whole(rate * 2), split(rate * 2);
    contiguous.render(whole);
    for(unsigned int frame{}; frame != rate;) {
      auto const count{std::min(rate - frame, (frame * 7 + 13) % 337 + 1)};
      fragmented.render(std::span{split}.subspan(frame * 2, count * 2));
      frame += count;
    }
    require(whole == split, "QSound playback depends on callback segmentation");
    require(!contiguous.state().active && !fragmented.state().active, "QSound stream did not complete");
    require(std::ranges::all_of(std::span{whole}.last(48), [](float value) { return value == 0; }), "QSound tail did not drain");
    require(std::ranges::any_of(whole, [](float value) { return value != 0; }), "QSound stream was silent");
    for(auto const value : whole) require(std::isfinite(value), "QSound produced a non-finite sample");
  }
  unsigned int constexpr rate{48000};
  tfx::audio::sample_voice first{bank, rate, std::make_unique<tfx::audio::qsound_effect>(rate)};
  tfx::audio::sample_voice second{bank, rate, std::make_unique<tfx::audio::qsound_effect>(rate)};
  std::array<unsigned int, 1> constexpr start{0}, interrupt{2}, rejected{1};
  first.submit(tfx::audio::sample_priority::normal, start);
  second.submit(tfx::audio::sample_priority::normal, start);
  std::vector<float> a(8000), b(8000);
  first.render(a);
  second.render(b);
  first.submit(tfx::audio::sample_priority::urgent, interrupt);
  second.submit(tfx::audio::sample_priority::urgent, interrupt);
  first.submit(tfx::audio::sample_priority::normal, rejected);
  std::ranges::fill(a, 0);
  for(unsigned int index{}; index != b.size(); ++index) b[index] = index % 2 ? -0.25f : 0.125f;
  first.render(a);
  second.render(b);
  for(unsigned int index{}; index != a.size(); ++index) {
    require(b[index] == a[index] + (index % 2 ? -0.25f : 0.125f), "Rejected request changed panning or QSound processed other audio");
  }
}

} // anonymous namespace

auto main()->int try {
  verify_timer();
  verify_playback();
  std::cout << "QSound: original timer cadence, endpoint clamping, DAC-rate filtering, callback boundaries, rate changes, preemption, additive mixing and tail drainage passed\n";
  return 0;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return 1;
}
