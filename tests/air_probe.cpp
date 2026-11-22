#include <charconv>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include "actors/air.h"
#include "air_snapshot.h"
#include "resources/archive.h"
#include "resources/executable_tables.h"

int main(int const argc, char const *const argv[]) try {
  if(argc < 2 || argc > 7) throw std::runtime_error{"air_probe DATA_DIRECTORY [--verbose]"};
  bool verbose{false};
  bool combat{false};
  bool collision{false};
  unsigned int damage_tick{0};
  for(int i{2}; i < argc; ++i) {
    if(std::string_view{argv[i]} == "--verbose") {
      verbose = true;
    } else if(std::string_view{argv[i]} == "--combat") {
      combat = true;
    } else if(std::string_view{argv[i]} == "--collision") {
      collision = true;
    } else if(std::string_view{argv[i]} == "--damage" && i + 1 < argc) {
      damage_tick = static_cast<unsigned int>(std::stoul(argv[++i]));
    } else {
      throw std::runtime_error{"unrecognised air probe option"};
    }
  }
  auto const path{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::archive const game{path};
  tfx::resources::executable_tables const tables{path.parent_path() / "TFX.DAT"};
  auto const profiles{tfx::actors::profiles(game.read("waybin/craftype.cfg"))};
  auto const manoeuvres{combat ? tfx::actors::load_manoeuvres(game) : std::vector<tfx::actors::manoeuvre>{}};
  std::string name;
  unsigned int ticks{0};
  unsigned int intact{0};
  while(std::cin >> name >> ticks >> intact) {
    uint32_t identifier{0};
    auto const [end, error]{std::from_chars(name.data(), name.data() + name.size(), identifier, 16)};
    auto const bytes{error == std::errc{} && end == name.data() + name.size() ? game.read(identifier) : game.read(name)};
    tfx::actors::program const program{bytes};
    auto const initial{tfx::actors::initialise(program, profiles, tables.formations)};
    tfx::actors::air_runtime state{program, initial, profiles, tables.formations, tables.aircraft_envelopes, {}, manoeuvres};
    tfx::actors::air_context context{
      .intact_ground_target{[intact](vec2<int16_t> const &){return intact != 0;}},
    };
    std::array<std::optional<tfx::actors::air_threat>, tfx::actors::player_air_target> threats;
    if(combat) {
      tfx::actors::air_contact player;
      unsigned int aircraft{0};
      unsigned int flags{0};
      unsigned int weapons{0};
      unsigned int incoming{0};
      if(!(std::cin >> player.position.x >> player.position.y >> player.position.z >> player.angles.x >> player.angles.y >> player.angles.z >> player.speed >> aircraft >> flags >> weapons >> incoming)) throw std::runtime_error{"incomplete combat context"};
      context.combat = true;
      context.player = std::move(player);
      context.player_aircraft = static_cast<tfx::flight::player_aircraft>(aircraft + 1);
      context.combat_flags = static_cast<uint16_t>(flags);
      context.weapons_enabled = weapons != 0;
      if(incoming) {
        for(unsigned int index{0}; index != state.aircraft().size(); ++index) {
          if(state.aircraft()[index]) {
            auto position{vec3<int32_t>{state.aircraft()[index]->flight.pose.position}};
            position.x += incoming % 2 ? 300000 : -300000;
            position.z += 200000;
            threats[index] = tfx::actors::air_threat{
              .position{vec3<int32_t>{position}},
              .weapon{static_cast<uint8_t>(incoming % 3)},
            };
          }
        }
      }
      context.threats = threats;
    }
    for(unsigned int tick{0}; tick != ticks; ++tick) {
      std::vector<tfx::actors::air_request> requests;
      if(damage_tick && tick == damage_tick) {
        for(unsigned int index{0}; index != state.aircraft().size(); ++index) {
          if(state.aircraft()[index]) {
            auto death{state.fatal_damage(index, collision ? tfx::actors::air_loss_cause::collision : tfx::actors::air_loss_cause::combat)};
            requests.insert(requests.end(), std::make_move_iterator(death.begin()), std::make_move_iterator(death.end()));
          }
        }
      }
      auto updates{state.advance(context)};
      requests.insert(requests.end(), std::make_move_iterator(updates.begin()), std::make_move_iterator(updates.end()));
      auto const digest{tfx::test::air_snapshot(program, state, requests, verbose ? &std::cout : nullptr, combat)};
      if(!verbose) std::cout << digest;
      std::cout << '\n';
    }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
