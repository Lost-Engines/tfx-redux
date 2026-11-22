#include <bit>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "missions/initialisation.h"

namespace {

void require(bool const value, char const *const message) {
  if(!value) throw std::runtime_error{message};
}
void rejected(std::string const &command) {
  bool failed{false};
  try {
    tfx::missions::initialise("INIT:\r\nworld somalia\r\ntype somal_tw\r\nway miss501\r\n" + command + "\r\nFINISH:\r\n");
  } catch(std::runtime_error const &) {
    failed = true;
  }
  require(failed, "invalid or unimplemented mission declaration was silently accepted");
}

} // anonymous namespace

auto main()->int try {
  auto const parsed{tfx::missions::initialise(
    "; INIT: inside an authoring comment is not the entry label\r\n"
    "INIT:\r\nworld somalia\r\ntype somal_tw\r\nway miss501\r\n"
    "start 100,landed,200\r\nangles 65537,-1,0\r\nspeed 400\r\n"
    "message 20\r\n    LEADING INDENT IS STRIPPED\r\nSECOND LINE\r\nEND_TEXT\r\n"
    "complete enemy == 1 text\r\nCOMPLETE\r\nEND_TEXT\r\nFINISH:\r\nunsupported_finish_command\r\n", {
      .position{3, -400, 5},
      .speed_factor{0.625f},
      .weapons{173},
    })};
  require(parsed.aircraft == tfx::world::player_aircraft::eurofighter, "omitted aircraft follows the native mission reset");
  require(parsed.start.landed && parsed.start.position.y == -400 && parsed.weapons == 173, "INIT must retain untouched altitude/loadout state");
  require(parsed.angles.x == 1 && parsed.angles.y == 65535, "mission angle stores must wrap as native words");
  require(std::bit_cast<uint32_t>(parsed.speed_factor) == 1057982344, "native initial speed conversion differs");
  require(parsed.messages.size() == 1 && parsed.messages[0].time == 1080 && parsed.messages[0].text == "LEADING INDENT IS STRIPPED\r\nSECOND LINE\r\n", "native message clock/text handling differs");
  require(parsed.completion && parsed.completion->kind == tfx::missions::completion_kind::enemy && parsed.completion->value == 1, "completion declaration differs");
  rejected("plane 4");
  rejected("targ#2 10,20");
  rejected("complete unknown == 1");
  rejected("message 1\r\nmissing text terminator");
  rejected("unknown_init_command");
  rejected("flak 1,2,3,4\r\nflak 1,2,3,4\r\nflak 1,2,3,4\r\nflak 1,2,3,4\r\nflak 1,2,3,4\r\nflak 1,2,3,4");
  std::cout << "Mission INIT state retention, wrapping, text and unsupported-input checks passed" << std::endl;
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
