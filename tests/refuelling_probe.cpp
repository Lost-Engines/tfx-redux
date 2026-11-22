#include <bit>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <utility>
#include "flight/refuelling.h"
#include "resources/archive.h"

int main(int const argc, char const *const argv[]) {
  if(argc != 2) throw std::runtime_error{"refuelling probe requires original data directory"};
  auto const executable{tfx::resources::read_file(tfx::resources::find_game_archive(argv[1]).parent_path() / "TFX.DAT")};
  tfx::flight::refuel_assets const assets{executable};
  std::optional<tfx::flight::refuelling> controller;
  tfx::flight::refuel_registry allies;
  tfx::flight::refuel_player player;
  unsigned int reset;
  unsigned int enabled;
  unsigned int saved;
  unsigned int phase;
  unsigned int craft;
  unsigned int elapsed;
  unsigned int toggle;
  unsigned int inhibited;
  unsigned int crashed;
  unsigned int ejected;
  unsigned int mode;
  unsigned int burner;
  unsigned int patches;
  int delay;
  int dx;
  int dy;
  int dz;
  int selected;
  int horizontal;
  int ex;
  int ey;
  int ez;
  double fuel;
  double capacity;
  while(std::cin >> reset >> enabled >> saved >> phase >> delay >> dx >> dy >> dz >> selected >> horizontal >> ex >> ey >> ez >> craft >> player.position.x >> player.position.y >> player.position.z >> player.position_q8.x >> player.position_q8.y >> player.position_q8.z >> player.angles.x >> player.angles.y >> player.angles.z >> player.speed >> elapsed >> toggle >> inhibited >> crashed >> ejected >> mode >> fuel >> capacity >> burner >> patches) {
    player.aircraft = static_cast<tfx::flight::player_aircraft>(craft + 1);
    if(reset) {
      allies = {};
      controller.emplace(assets, tfx::flight::refuel_state{
        .enabled{enabled != 0},
        .saved_weapon_mode{static_cast<uint8_t>(saved)},
        .selected{selected < 0 ? std::nullopt : std::optional<unsigned int>{static_cast<unsigned int>(selected)}},
        .divisors{static_cast<int16_t>(dx), static_cast<int16_t>(dy), static_cast<int16_t>(dz)},
        .delay{static_cast<int16_t>(delay)},
        .phase{static_cast<uint8_t>(phase)},
        .horizontal_distance{horizontal},
        .angle_errors{ex, ey, ez},
      });
    }
    if(!controller) throw std::runtime_error{"refuelling sequence requires reset"};
    for(unsigned int patch{0}; patch != patches; ++patch) {
      unsigned int slot;
      unsigned int present;
      unsigned int flags;
      int type;
      tfx::flight::refuel_aircraft actor;
      std::cin >> slot >> present >> type >> flags >> actor.position_q8.x >> actor.position_q8.y >> actor.position_q8.z >> actor.angles.x >> actor.angles.y >> actor.angles.z;
      actor.flags = static_cast<uint16_t>(flags);
      actor.shape_type = static_cast<int16_t>(type);
      if(present) {
        allies.at(slot).emplace(std::move(actor));
      } else {
        allies.at(slot).reset();
      }
    }
    std::vector<tfx::flight::refuel_event> events;
    uint8_t next_mode{static_cast<uint8_t>(mode)};
    if(toggle) {
      auto update{controller->toggle(next_mode, inhibited != 0, crashed != 0, ejected != 0)};
      next_mode = update.weapon_mode;
      events = std::move(update.events);
    }
    // The player update transfers fuel before the camera's coupling controller.
    auto const transfer{controller->transfer(fuel, capacity, burner != 0)};
    auto const update{controller->advance(player, allies, static_cast<tfx::simulation::timer_ticks>(elapsed))};
    for(auto const &event : update.events) events.push_back(event);
    auto const &state{controller->value()};
    std::cout << state.enabled << ' ' << static_cast<unsigned int>(state.saved_weapon_mode) << ' ' << static_cast<unsigned int>(state.phase) << ' ' << state.delay << ' ' << state.divisors.x << ' ' << state.divisors.y << ' ' << state.divisors.z << ' ' << (state.selected ? static_cast<int>(*state.selected) : -1) << ' ' << state.horizontal_distance << ' ' << state.angle_errors.x << ' ' << state.angle_errors.y << ' ' << state.angle_errors.z << ' ' << static_cast<unsigned int>(next_mode) << ' ' << (update.target_override ? static_cast<int>(*update.target_override) : -1) << ' ' << (update.moved_tanker ? static_cast<int>(*update.moved_tanker) : -1) << ' ' << player.position.x << ' ' << player.position.y << ' ' << player.position.z << ' ' << std::bit_cast<uint64_t>(transfer.fuel) << ' ' << transfer.afterburner_warning << ' ' << events.size();
    for(auto const &event : events) std::cout << ' ' << std::to_underlying(event.kind) << ' ' << event.argument << ' ' << (event.tanker ? static_cast<int>(*event.tanker) : -1);
    for(auto const &actor : allies) {
      std::cout << ' ' << actor.has_value();
      if(actor) std::cout << ' ' << actor->position_q8.x << ' ' << actor->position_q8.y << ' ' << actor->position_q8.z << ' ' << actor->angles.x << ' ' << actor->angles.y << ' ' << actor->angles.z << ' ' << actor->flags << ' ' << actor->shape_type;
    }
    std::cout << '\n';
  }
}
