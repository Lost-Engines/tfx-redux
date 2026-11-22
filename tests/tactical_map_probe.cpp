#include <array>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "cockpit/tactical_map.h"

auto main(int const argc, char const *const argv[])->int try {
  if(argc != 3) throw std::runtime_error{"usage: tactical_map_probe DATA_DIRECTORY SCHEMATIC"};
  auto const archive{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::archive const game{archive};
  tfx::resources::executable_tables const tables{archive.parent_path() / "TFX.DAT"};
  tfx::cockpit::tactical_map_animation animation;
  tfx::cockpit::tactical_map map{game,tables,argv[2],animation};
  unsigned int colour;
  while(std::cin >> colour) {
    animation.target_colour = static_cast<uint8_t>(colour);
    tfx::cockpit::tactical_map_snapshot state;
    bool landing;
    vec2<int16_t> cell;
    std::cin >> state.player.x >> state.player.y >> state.player.z >> state.heading >> landing >> cell.x >> cell.y;
    if(landing) state.landing_cell.emplace(cell);
    unsigned int count;
    std::cin >> count;
    std::vector<vec2<uint8_t>> route;
    for(unsigned int i{0}; i != count; ++i) {
      unsigned int x,z;
      std::cin >> x >> z;
      route.emplace_back(static_cast<uint8_t>(x),static_cast<uint8_t>(z));
    }
    state.route = route;
    auto const positions{[]{
      unsigned int count;
      std::cin >> count;
      std::vector<vec3<int32_t>> points(count);
      for(auto &point : points) std::cin >> point.x >> point.y >> point.z;
      return points;
    }};
    auto targets{positions()};
    state.targets = targets;
    std::array<std::vector<tfx::cockpit::tactical_map_aircraft>,2> aircraft;
    std::array<std::vector<std::string>,2> names;
    for(unsigned int pool{0}; pool != aircraft.size(); ++pool) {
      std::cin >> count;
      aircraft[pool].resize(count);
      names[pool].resize(count);
      for(unsigned int i{0}; i != count; ++i) {
        auto &contact{aircraft[pool][i]};
        auto &reading{contact.reading};
        std::cin >> reading.position_q8.x >> reading.position_q8.y >> reading.position_q8.z >> reading.heading >> reading.speed_q16 >> contact.selected >> std::quoted(names[pool][i]);
        reading.name = names[pool][i];
      }
    }
    state.hostile_aircraft = aircraft[0];
    state.allied_aircraft = aircraft[1];
    auto hostile{positions()}, allied{positions()};
    state.hostile_surface = hostile;
    state.allied_surface = allied;
    std::cin >> count;
    std::vector<tfx::simulation::timer_ticks> ticks(count);
    for(auto &tick : ticks) std::cin >> tick;
    if(!std::cin) throw std::runtime_error{"incomplete tactical-map case"};
    map.open(state);
    for(auto const tick : ticks) {
      map.advance(tick);
      for(unsigned int byte{0}; byte != 4; ++byte) std::cout.put(static_cast<char>(animation.ticks >> (byte * 8)));
      std::cout.put(static_cast<char>(animation.target_colour));
      auto const &pixels{map.image().pixels};
      std::cout.write(reinterpret_cast<char const*>(pixels.data()),pixels.size());
    }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
