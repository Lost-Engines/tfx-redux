#include <algorithm>
#include <array>
#include <iostream>
#include <optional>
#include <stdexcept>
#include "render/palette_lighting.h"
#include "resources/executable_tables.h"
#include "world/environment.h"

int main(int const argc, char const *const argv[]) {
  if(argc != 2) throw std::runtime_error{"palette lighting probe requires original data directory"};
  tfx::resources::archive const game{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::executable_tables const tables{tfx::resources::find_game_archive(argv[1]).parent_path() / "TFX.DAT"};
  std::optional<tfx::render::palette_lighting> lighting;
  std::string name;
  unsigned int reset;
  unsigned int dirty;
  unsigned int pitch;
  unsigned int roll;
  unsigned int view;
  unsigned int effects;
  unsigned int mode;
  unsigned int green;
  int32_t y;
  int black;
  int red;
  int flash;
  while(std::cin >> name >> reset >> dirty >> y >> pitch >> roll >> view >> effects >> mode >> black >> red >> green >> flash) {
    if(reset) {
      tfx::world::lighting_configuration const configuration{game, name};
      std::array<tfx::render::dac_palette, tfx::render::altitude_palette_count> banks;
      for(unsigned int bank{0}; bank != banks.size(); ++bank) {
        auto const bytes{game.read("colours/" + (bank ? configuration.upper_palette_names[bank - 1] : configuration.palette_name) + ".col")};
        if(bytes.size() != banks[bank].size()) throw std::runtime_error{"invalid altitude palette"};
        std::ranges::copy(bytes, banks[bank].begin());
      }
      lighting.emplace(banks, configuration.palette_altitudes, tables.fixed_flight_palette);
    }
    if(!lighting) throw std::runtime_error{"palette sequence requires reset"};
    if(flash >= 0) lighting->flash(static_cast<uint8_t>(flash));
    auto const result{lighting->advance({
      .camera_y{y},
      .pitch{static_cast<tfx::math::angle>(pitch)},
      .roll{static_cast<tfx::math::angle>(roll)},
      .view{static_cast<tfx::cockpit::view_mode>(view)},
      .hud_green{static_cast<uint8_t>(green)},
      .normal_changed{dirty != 0},
      .effects{effects != 0},
      .blackout{static_cast<tfx::render::blackout_mode>(mode)},
      .blackout_level{static_cast<int16_t>(black)},
      .redout_level{static_cast<int16_t>(red)},
    })};
    auto const &state{lighting->value()};
    std::array<int32_t, 5> const values{state.altitude_step, state.view_phase, state.flash, result.changed, result.sound ? static_cast<int>(*result.sound) : -1};
    std::cout.write(reinterpret_cast<char const *>(values.data()), sizeof(values));
    std::cout.write(reinterpret_cast<char const *>(lighting->colours().data()), lighting->colours().size());
  }
}
