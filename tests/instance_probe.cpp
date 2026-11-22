#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <utility>
#include "resources/archive.h"
#include "world/instance.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"instance_probe DATA_DIRECTORY"};
  tfx::resources::archive const game{tfx::resources::find_game_archive(argv[1])};
  auto const names{tfx::world::model_names(game.read("ss/ssinfo.fn"))};
  uint32_t id{0};
  unsigned int commands{0};
  while(std::cin >> id >> commands) {
    tfx::world::definition const definition{game.read(id),names};
    tfx::world::instance state{definition};
    if(!commands) {
      auto const centres{state.damage_centres()};
      std::cout << centres.size();
      for(auto const &centre : centres) std::cout << ' ' << centre.x << ' ' << centre.y << ' ' << centre.z;
      std::cout << '\n';
    }
    for(unsigned int command{0}; command != commands; ++command) {
      unsigned int entry{0}, animate{0}, player{0}, enabled{0}, damage{0}, status{0};
      int distance{0};
      std::cin >> entry >> animate >> player >> enabled >> damage >> status >> distance;
      tfx::world::instance_context context{
        .player_query{player != 0},
        .surface_enabled{enabled != 0},
        .impact_damage{static_cast<uint16_t>(damage)},
        .status{static_cast<uint16_t>(status)},
        .distance{static_cast<int16_t>(distance)},
        .anchor_distance{[distance](vec3d const&){ return static_cast<int16_t>(distance); }},
      };
      for(auto *point : {&context.movement.previous,&context.movement.current}) {
        int x{0},y{0},z{0};
        std::cin >> x >> y >> z;
        *point = vec3<int16_t>{static_cast<int16_t>(x),static_cast<int16_t>(y),static_cast<int16_t>(z)};
      }
      auto const result{animate ? state.animate(context) : state.execute(entry,context)};
      auto const word{[](auto const value){ std::cout << static_cast<int>(value) << ' '; }};
      word(state.destroyed); word(state.modified); word(state.category); word(state.impact_damage); word(state.health);
      word(state.custom.size());
      for(auto const value : state.custom) word(value);
      word(state.parts.size());
      for(unsigned int index{0}; index != state.parts.size(); ++index) {
        auto const &part{state.parts[index]};
        word(part.model_index); word(part.rotation_mode);
        word(part.rotation.x); word(part.rotation.y); word(part.rotation.z);
        for(auto const value : part.parameters) word(value);
        for(unsigned int parameter{0}; parameter != tfx::world::part_parameters; ++parameter) word(state.stream_cursor(index,parameter).value_or(0));
      }
      word(result.collisions); word(result.volume_hit); word(result.ejection_requested); word(result.surface_present); word(result.surface_height.value_or(0)); word(result.surface_height.has_value()); word(result.zone); word(result.player_zone); word(result.message);
      word(result.draws.size());
      for(auto const &draw : result.draws) { word(draw.slot); word(draw.anchor.x); word(draw.anchor.y); word(draw.anchor.z); word(std::to_underlying(draw.kind)); word(std::to_underlying(draw.placement));word(std::to_underlying(draw.orientation));word(std::to_underlying(draw.delivery));
        auto const &part{draw.part};word(part.model_index);word(part.rotation_mode);word(part.rotation.x);word(part.rotation.y);word(part.rotation.z);for(auto const value : part.parameters) word(value);
      }
      word(result.requests.size());
      for(auto const &request : result.requests) { word(request.opcode); word(request.arguments.size()); for(auto const value : request.arguments) word(value); }
      word(result.damage_reports.size());
      for(auto const value : result.damage_reports) word(value);
      std::cout << '\n';
    }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
