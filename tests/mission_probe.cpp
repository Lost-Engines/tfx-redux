#include <bit>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <utility>
#include "missions/initialisation.h"
#include "resources/archive.h"

namespace {

void text(std::string const &value) {
  std::cout << '"';
  for(unsigned char const c : value) {
    if(c < 32 || c >= 127) {
      std::cout << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(c) << std::dec;
    } else {
      if(c == '"' || c == '\\') std::cout << '\\';
      std::cout << c;
    }
  }
  std::cout << '"';
}
void text(std::optional<std::string> const &value) {
  if(value) {
    text(*value);
  } else {
    std::cout << "null";
  }
}
void state(tfx::missions::setup const &value) {
  std::cout << '[';
  text(value.world);
  std::cout << ',';
  text(value.environment);
  std::cout << ',';
  text(value.waypoints);
  auto const &position{value.start.position};
  std::cout << ",[" << position.x << ',' << position.y << ',' << position.z << ',' << value.start.landed << "]," << std::to_underlying(value.aircraft) << ",[" << value.angles.x << ',' << value.angles.y << ',' << value.angles.z << "]," << std::bit_cast<uint32_t>(value.speed_factor) << ',' << static_cast<unsigned int>(value.weapons) << ',' << value.default_weapons << ',';
  text(value.saved_start_extension);
  std::cout << ',' << value.aircraft_type_filter << ',' << value.surface_type_filter << ',' << (value.timer ? *value.timer : -1) << ',' << value.clock << ",[";
  for(unsigned int i{0}; i != value.targets.size(); ++i) {
    if(i) std::cout << ',';
    auto const &target{value.targets[i]};
    std::cout << '[' << target.position.x << ',' << target.position.y << ',' << target.position.z << ',' << target.time_limit << ',';
    text(target.text);
    std::cout << ']';
  }
  std::cout << "],";
  if(value.completion) {
    auto const &rule{*value.completion};
    std::cout << '[' << static_cast<unsigned int>(std::to_underlying(rule.kind)) << ',' << rule.value << ',';
    text(rule.text);
    std::cout << ']';
  } else {
    std::cout << "null";
  }
  std::cout << ",[";
  for(unsigned int i{0}; i != value.flak.size(); ++i) {
    if(i) std::cout << ',';
    auto const &area{value.flak[i]};
    std::cout << '[' << area.first.x << ',' << area.first.y << ',' << area.last.x << ',' << area.last.y << ']';
  }
  std::cout << "],[";
  for(unsigned int i{0}; i != value.messages.size(); ++i) {
    if(i) std::cout << ',';
    std::cout << '[' << value.messages[i].time << ',';
    text(value.messages[i].text);
    std::cout << ']';
  }
  std::cout << "],[";
  for(unsigned int i{0}; i != value.scenery_events.size(); ++i) {
    if(i) std::cout << ',';
    auto const &event{value.scenery_events[i]};
    std::cout << '[' << event.time << ',' << static_cast<unsigned int>(event.entry) << ',' << static_cast<unsigned int>(event.cell.x) << ',' << static_cast<unsigned int>(event.cell.y) << ']';
  }
  std::cout << "]]\n";
}

} // anonymous namespace

auto main(int const argc, char const *const argv[])->int try {
  if(argc != 2) throw std::runtime_error{"mission probe requires the original installation directory"};
  tfx::resources::archive const game{tfx::resources::find_game_archive(argv[1])};
  std::string name;
  tfx::missions::inherited_setup context;
  uint32_t speed{0};
  unsigned int weapons{0};
  while(std::cin >> name >> context.position.x >> context.position.y >> context.position.z >> speed >> weapons) {
    context.speed_factor = std::bit_cast<float>(speed);
    context.weapons = static_cast<uint8_t>(weapons);
    auto const source{game.read("missions/" + name + ".mis")};
    state(tfx::missions::initialise({reinterpret_cast<char const *>(source.data()), source.size()}, context));
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
