#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "qsound/spatial_model.h"
#include "qsound/upgrade.h"
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

void tag(bytes &output, std::string_view const name) {
  output.insert(output.end(), name.begin(), name.end());
}

void chunk(bytes &output, std::string_view const name, std::span<uint8_t const> const contents) {
  tag(output, name);
  integer(output, static_cast<uint32_t>(contents.size()), 4);
  output.insert(output.end(), contents.begin(), contents.end());
  if(contents.size() % 2) output.push_back(0);
}

bytes program(std::span<uint32_t const> const words) {
  bytes result;
  for(auto const word : words) {
    for(unsigned int byte{}; byte != 4; ++byte) result.push_back(qsound::unscramble_byte(static_cast<uint8_t>(word >> (byte * 8))));
  }
  unsigned int checksum{};
  for(auto const value : result) checksum += value;
  integer(result, checksum);
  integer(result, 1);
  return result;
}

// Synthetic coefficient/table fixture, not original Creative firmware and not
// an executable DSP replacement. It exercises the real parser and audio path.
bytes fixture_firmware() {
  std::vector<uint32_t> setup(8);
  setup.push_back(0x04278000);
  for(unsigned int filter{}; filter != 2; ++filter) {
    setup.push_back(0xf8600019 + filter * 3);
    std::array<int16_t, 3> constexpr taps{-10000, 4000, -5000};
    for(auto const tap : taps) {
      setup.push_back(0x742b0000 | static_cast<uint16_t>(tap));
      setup.push_back(0x1400201c);
    }
  }
  setup.push_back(0xfc600000);
  setup.push_back(0xfd600083);
  for(unsigned int position{}; position != 33; ++position) {
    std::array<unsigned int, 4> const row{
      2000 + position * 400,
      30000 - position * 800,
      3000 + position * 800,
      15000 - position * 300,
    };
    for(auto const value : row) {
      setup.push_back(0x742b0000 | value);
      setup.push_back(0x04002108);
    }
  }
  setup.push_back(0xfc600084);
  std::vector<uint32_t> processing(351);
  processing[0x72] = 0xfc600084;
  processing[0x73] = 0xfd600087;
  processing[0x82] = 0xf8600019;
  processing[0x83] = 0xca602000;
  processing[0x9c] = 0xf860001c;
  bytes description;
  for(auto const value : {1u, 1u, 2u, 1u, 1u, 1u}) integer(description, value);
  bytes function;
  tag(function, "func");
  chunk(function, "desc", description);
  bytes code;
  tag(code, "code");
  chunk(code, "init", program(setup));
  chunk(code, "main", program(processing));
  bytes payload;
  tag(payload, "CSP ");
  chunk(payload, "LIST", function);
  chunk(payload, "LIST", code);
  bytes result;
  chunk(result, "RIFF", payload);
  return result;
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

void verify_timer(std::span<uint8_t const> const firmware) {
  unsigned int constexpr rate{48000};
  auto const functions{qsound::decode_firmware_asset(firmware)};
  auto const tables{qsound::recover_spatial_tables(qsound::select_qsound_function(functions, 8000))};
  qsound::spatial_model expected{tables, {
    .rounding{qsound::rounding_model::floor},
    .accumulation{qsound::accumulation_model::saturate_each_mac},
  }, qsound::position_policy::clamp_to_endpoints};
  tfx::audio::qsound_effect actual{firmware, rate};
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

void verify_playback(std::span<uint8_t const> const firmware) {
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
  auto const functions{qsound::decode_firmware_asset(firmware)};
  auto const recovered{qsound::recover_spatial_tables(qsound::select_qsound_function(functions, 8000))};
  for(unsigned int const rate : {8000u, 44100u, 48000u}) {
    // Before the first PIT interrupt, gains remain at the firmware centre.
    // Independently resample native FIR outputs with the documented 119-us
    // DAC hold. This detects filtering repeated host frames or losing skipped
    // native samples at the 8-kHz host rate.
    qsound::spatial_model native{recovered, {
      .rounding{qsound::rounding_model::floor},
      .accumulation{qsound::accumulation_model::saturate_each_mac},
    }};
    tfx::audio::sample_voice held_voice{bank, rate, std::make_unique<tfx::audio::qsound_effect>(firmware, rate)};
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
    tfx::audio::sample_voice contiguous{bank, rate, std::make_unique<tfx::audio::qsound_effect>(firmware, rate)};
    tfx::audio::sample_voice fragmented{bank, rate, std::make_unique<tfx::audio::qsound_effect>(firmware, rate)};
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
  tfx::audio::sample_voice first{bank, rate, std::make_unique<tfx::audio::qsound_effect>(firmware, rate)};
  tfx::audio::sample_voice second{bank, rate, std::make_unique<tfx::audio::qsound_effect>(firmware, rate)};
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

auto main(int const argc, char const *const argv[])->int try {
  auto const firmware{argc == 2 ? tfx::resources::read_file(argv[1]) : fixture_firmware()};
  verify_timer(firmware);
  verify_playback(firmware);
  std::cout << "QSound: original timer cadence, endpoint clamping, DAC-rate filtering, callback boundaries, rate changes, preemption, additive mixing and tail drainage passed\n";
  return 0;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return 1;
}
