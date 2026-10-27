#include "qsound_effect.h"

#include <algorithm>
#include <exception>
#include <utility>
#include "qsound/position_sweep.h"
#include "qsound/spatial_model.h"
#include "qsound/upgrade.h"
#include "audio/music_clock.h"

namespace tfx::audio {
namespace {

auto tables(std::span<uint8_t const> const firmware)->qsound::spatial_tables {
  auto const functions{qsound::decode_firmware_asset(firmware)};
  unsigned int constexpr original_firmware_rate{8000};
  return qsound::recover_spatial_tables(qsound::select_qsound_function(functions, original_firmware_rate));
}

} // anonymous namespace

struct qsound_effect::implementation {
  qsound::position_sweep sweep;
  music_clock clock;
  int chain_phase{};
  unsigned int const tail;
  qsound::spatial_model processor;

  implementation(qsound::spatial_tables tables, unsigned int const output_rate) :
    clock{output_rate},
    tail{static_cast<unsigned int>(std::max({tables.left_filter.taps.size(), tables.right_filter.taps.size(),
      static_cast<std::size_t>(tables.direct_delay_samples + 1)}))},
    processor{std::move(tables), {
      .rounding{qsound::rounding_model::floor},
      .accumulation{qsound::accumulation_model::saturate_each_mac},
    }, qsound::position_policy::clamp_to_endpoints} {
  }
};

qsound_effect::qsound_effect(std::span<uint8_t const> const firmware, unsigned int const output_rate) :
  state{std::make_unique<implementation>(tables(firmware), output_rate)} {
}

qsound_effect::~qsound_effect() = default;

void qsound_effect::select(sample_pan const &parameters) {
  // Publication remains timer-driven. Preserve the cache and filter histories.
  state->sweep.set({
    .position{parameters.position},
    .increment{parameters.increment},
    .moving{parameters.movement_enable != 0},
  });
}

void qsound_effect::advance_frame() {
  while(state->clock.due()) {
    state->clock.consume();
    // Original OSL IRQ chaining: 50 callbacks per 137 PIT 0x5555 interrupts.
    int constexpr chain_step{50};
    int constexpr chain_modulus{137};
    state->chain_phase -= chain_step;
    if(state->chain_phase < 0) {
      state->chain_phase += chain_modulus;
      if(auto const position{state->sweep.tick()}) {
        unsigned int constexpr unused_right_input_position{16};
        state->processor.set_positions(*position, unused_right_input_position);
      }
    }
  }
  state->clock.advance(1);
}

stereo_frame qsound_effect::process(uint8_t const sample) noexcept {
  auto const result{state->processor.process({
    .left{static_cast<int16_t>((static_cast<int>(sample) - 128) * 256)},
    .right{},
  })};
  float constexpr pcm_scale{32768.0f};
  return {
    .left{result.left / pcm_scale},
    .right{result.right / pcm_scale},
  };
}

unsigned int qsound_effect::tail_samples() const noexcept {
  return state->tail;
}

std::unique_ptr<qsound_effect> load_qsound_effect(std::span<std::filesystem::path const> const directories,
    unsigned int const output_rate) {
  std::exception_ptr invalid_firmware;
  for(auto const &directory : directories) {
    for(auto const *name : {"WFM0001A.CSP", "wfm0001a.csp", "CSP/WFM0001A.CSP", "csp/wfm0001a.csp", "qsound/MAIN.PVL"}) {
      auto const path{directory / name};
      if(std::filesystem::is_regular_file(path)) {
        try {
          return std::make_unique<qsound_effect>(resources::read_file(path), output_rate);
        } catch(std::exception const &) {
          // A stale local copy must not hide a valid fetched firmware asset.
          if(!invalid_firmware) invalid_firmware = std::current_exception();
        }
      }
    }
  }
  if(invalid_firmware) std::rethrow_exception(invalid_firmware);
  return {};
}

} // namespace tfx::audio
