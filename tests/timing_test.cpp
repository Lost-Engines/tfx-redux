#include <bit>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "simulation/timing.h"

namespace {

void require(bool const value, char const *const message) {
  if(!value) throw std::runtime_error{message};
}

} // anonymous namespace

auto main()->int try {
  namespace sim = tfx::simulation;
  sim::flight_clock clock{0xfffffffe};
  clock.begin({});
  require(!clock.poll(0xffffffff), "one tick must wait");
  auto step{clock.poll(0)};
  require(step && step->world_ticks == 2, "counter wrap must retain elapsed time");
  require(step->filtered_ticks == 3.5f && step->physics_seconds == 2 * static_cast<double>(0.02f), "native time/filter storage differs");
  clock.begin({
    .model{sim::flight_model::realistic},
    .time_warp{true},
  });
  require(!clock.poll(20), "realistic mode always takes an additional timer reading");
  step = clock.poll(20);
  require(step && step->world_ticks == 90 && step->physics_seconds == 18 * static_cast<double>(0.02f), "time warp changes world ticks separately from the physics timestep");
  clock.begin({
    .model{sim::flight_model::military_spec},
    .time_warp{true},
    .time_warp_allowed{false},
  });
  step = clock.poll(120);
  require(step && step->world_ticks == 4, "military frame cap or warp inhibition differs");
  float const retained_filter{step->filtered_ticks};
  clock.begin({});
  require(!clock.poll(121), "one pending tick should be retained before a menu");
  clock.resume(100000, {
    .model{sim::flight_model::realistic},
  });
  require(!clock.poll(100002), "resuming in Realistic must retain its extra poll");
  step = clock.poll(100005);
  require(step && step->world_ticks == 5 && step->filtered_ticks == (retained_filter * 3 + 5) / 4, "resuming must discard menu time and partial polling, retaining the timing filter");
  sim::flight_clock changed{0};
  changed.begin({});
  require(!changed.poll(1), "one pending tick should remain before acceleration changes");
  auto const accelerated{changed.poll(2, {
    .time_warp{true},
  })};
  require(accelerated && accelerated->world_ticks == 10 && accelerated->physics_seconds == 2 * static_cast<double>(0.02f), "changing acceleration during a pending frame must retain its elapsed ticks");
  sim::projectile_clock chunks{25};
  require(chunks.next() == 10 && chunks.next() == 10 && chunks.next() == 5 && !chunks.next(), "projectile subdivision differs");
  sim::projectile_clock zero{0};
  require(zero.next() == 0 && !zero.next(), "zero elapsed still visits projectile motion/collision once");
  sim::projectile_clock signed_word{65535};
  require(signed_word.next() == 65535 && !signed_word.next(), "projectile comparison must use a signed native word");
  bool rejected{false};
  try {
    clock.poll(100006);
  } catch(std::logic_error const &) {
    rejected = true;
  }
  require(rejected, "timer input outside a pending frame must be rejected");
  clock.begin({});
  rejected = false;
  try {
    clock.begin({});
  } catch(std::logic_error const &) {
    rejected = true;
  }
  require(rejected, "pending timer state must not be overwritten");
  std::cout << "Native flight-clock wrapping, polling, caps, warp and projectile boundaries passed" << std::endl;
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
