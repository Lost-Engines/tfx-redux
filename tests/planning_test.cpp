#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "planning/document.h"
#include "planning/notes.h"
#include "resources/compiled_panel.h"

namespace {
void require(bool const condition, char const *const message) {
  if(!condition) throw std::runtime_error{message};
}
} // anonymous namespace
int main() try {
  std::string const source{
    "1010 1 305419896 2\r\nfirst.lbm\r\nTESTWORLD\r\nsecond.lbm\r\nworld.col\r\nthird.lbm\r\n"
    "2 = number of paths\r\nPLAYER\r\nWP1 61 23\r\n-endcmnd-\r\nWP2 94 51\r\n11 9000\r\n-endcmnd-\r\n=ENDPATH=\r\n"
    "WINGMAN\r\nWP1 -1 32768\r\n14 1 4 8\r\n-endcmnd-\r\n=ENDPATH=\r\nBriefing\033\r\n"
  };
  tfx::planning::document const plan{source};
  require(plan.format == 1010 && plan.routes.size() == 2 && plan.routes[0].points.size() == 2, "planner route/header count differs");
  require(plan.parameters.aircraft_index == 1 && plan.parameters.departure_cell == 0x5678 && plan.parameters.primary_target_cell == 2 && plan.parameters.additional_target_count == 0, "overlapping native dword header writes differ");
  require(plan.parameters.additional_targets.front() == 0xffff && plan.parameters.weather_index == -1, "omitted authoring parameters lost their native defaults");
  require(plan.routes[1].points[0].cell.x == -1 && plan.routes[1].points[0].cell.y == -32768, "native waypoint word narrowing differs");
  require(plan.routes[0].points[1].commands == std::vector<std::string>{"11 9000"}, "planner discarded an authored command");
  require(plan.notes == "Briefing\033\r\n" && plan.map_images[2] == "third.lbm" && plan.world == "TESTWORLD", "planner lost metadata or original briefing bytes");
  auto const end{source.rfind("=ENDPATH=") + std::string_view{"=ENDPATH="}.size()};
  for(unsigned int length{0}; length != end; ++length) {
    bool rejected{false};
    try {
      tfx::planning::document incomplete{std::string_view{source}.substr(0, length)};
    } catch(std::runtime_error const &) {
      rejected = true;
    }
    require(rejected, "truncated planner document was accepted");
  }
  tfx::planning::document const old{"1000 WAYPOINT MISSION FILE\na\nb\nc\nd\ne\n0 = number of paths\n"};
  require(old.parameters.aircraft_index == -1 && old.parameters.additional_target_count == 0 && old.parameters.retained.back() == 0xffff, "legacy header defaults differ");
  std::array<int32_t, 15> complete{};
  complete.back() = 0x12345678;
  tfx::planning::mission_parameters const parameters{complete};
  require(parameters.retained[2] == 0x5678 && parameters.retained[3] == 0x1234, "last native header dword lost its spill word");
  tfx::planning::notes const notes{
    "0 255 25 8 13 15\r\nIntro\033\r\n1 255 0 22 10 0\r\nTake off\033\r\n\r\n4 255 7 10 6 2\r\nFirst\033\r\n\r\n6 255 13 12 9 2\r\nSecond\033\r\n"
  };
  require(notes.all().size() == 3 && notes.find(0) && notes.find(1), "planner annotation key order differs");
  require(notes.find(1)->size.y == 1 && notes.find(1)->text == "Take off", "annotation zero size or carriage-return removal differs");
  require(!notes.find(4) && !notes.find(6) && notes.find(0, 0)->text == "Second", "original blank-line note replacement differs");
  tfx::planning::notes replaced{"\n3\033\n\n6 255 13 12 9 2\nReplacement\033\n"};
  require(replaced.find(0, 0)->text == "Replacement", "an overwritten malformed block was eagerly decoded");
  replaced.append("0 255 1 1 1 1\nTranslated\033\n");
  require(replaced.find(0)->text == "Translated" && replaced.find(0, 0)->text == "Replacement", "translated note overlay discarded other keys");
  std::string long_note{"0 255 0 0 1 1\n"};
  long_note.append(410, 'A');
  long_note += '\033';
  tfx::planning::notes const truncated{long_note};
  require(truncated.find(0)->text.size() == 400, "native annotation text buffer limit differs");
  tfx::resources::screen_overlay overlay;
  overlay.coverage.pixels[0] = 255;
  overlay.image.pixels[0] = 0;
  overlay.image.pixels[1] = 77;
  tfx::render::framebuffer target;
  target.pixels.fill(12);
  overlay.paint(target);
  require(target.pixels[0] == 0 && target.pixels[1] == 12, "compiled-panel composition lost written black or overwrote an uncovered map pixel");
  std::cout << "Planner metadata, editable routes/commands, original briefing bytes, overlapping scanf writes and truncation bounds passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
