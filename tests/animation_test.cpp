#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "world/animation.h"

namespace {

void require(bool const value, char const *const message) {
  if(!value) throw std::runtime_error{message};
}

} // anonymous namespace

auto main()->int try {
  /// Golden outcomes from original 0xb6ce0 calls, including byte wrap and endpoint holds
  std::array<uint16_t, 5> const growing{0, 1, 0x0603, 65534, 5};
  tfx::world::parameter_player sequence{growing};
  uint8_t parameter{0};
  for(unsigned int i{0}; i != 40; ++i) {
    sequence.tick(parameter);
    require(parameter == std::min(i, 6u), "native growth/hold sequence differs");
  }
  std::array<uint16_t, 4> const wrapping{0xff00, 1, 2, 5};
  sequence = tfx::world::parameter_player{wrapping};
  for(unsigned int i{0}; i != 40; ++i) {
    sequence.tick(parameter);
    require(parameter == (i == 1 ? 0 : 255), "native byte wrap sequence differs");
  }
  tfx::world::animation_clock clock;
  require(!clock.advance(7), "native animation gate must use a strict greater-than comparison");
  require(clock.advance(1) && clock.remainder() == 1, "first native animation tick requires eight timer ticks");
  require(clock.advance(200) && clock.remainder() == 0, "native animation backlog reset differs");
  require(!clock.advance(65535) && clock.remainder() == -1, "native signed timer word wrap differs");
  std::array<uint16_t, 2> const event{6, 19};
  sequence = tfx::world::parameter_player{event};
  auto const request{sequence.tick(parameter)};
  require(request.action == tfx::world::stream_action::event_requested && request.event == 19, "SSD event must remain an explicit dispatcher request");
  std::array<uint16_t, 2> const loop{4, 65535};
  sequence = tfx::world::parameter_player{loop};
  bool rejected{false};
  try {
    sequence.tick(parameter);
  } catch(std::runtime_error const &) {
    rejected = true;
  }
  require(rejected, "a native non-returning loop must not masquerade as a completed update");
  std::cout << "Native animation endpoint, wrapping, clock and event-request checks passed" << std::endl;
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
