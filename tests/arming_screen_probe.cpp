#include <cstdlib>
#include <iostream>
#include "presentation/arming_widgets.h"
#include "resources/archive.h"
#include "resources/media.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2 && argc != 3) throw std::runtime_error{"usage: arming_screen_probe DATA_DIRECTORY"};
  tfx::resources::archive const game{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::executable_tables const tables{tfx::resources::find_game_archive(argv[1]).parent_path() / "TFX.DAT"};
  auto const background{tfx::resources::decode_screen(game.read("iff/raw/" + tables.arming_art().backdrop))};
  tfx::render::software_renderer renderer{tables};
  if(argc == 3) {
    unsigned int kind{0}, weapon{0};int x{0}, y{0};
    while(std::cin >> kind >> weapon >> x >> y) {
      renderer.output = background.image;
      if(kind == 0) tfx::presentation::draw_arming_unavailable(renderer,tables.arming_art());
      else tfx::presentation::draw_arming_store_label(renderer,tables.weapons.at(weapon).label,{x,y});
      std::cout.write(reinterpret_cast<char const*>(renderer.output.pixels.data()),renderer.output.pixels.size());
    }
    return EXIT_SUCCESS;
  }
  unsigned int aircraft{0}, first{0};int selected{0};
  while(std::cin >> aircraft >> first >> selected) {
    tfx::weapons::armament inventory{static_cast<tfx::flight::player_aircraft>(aircraft + 1),tables.weapons,tables.arming};
    for(unsigned int slot{0}; slot != tfx::flight::store_slots; ++slot) {
      int weapon{0};std::cin >> weapon;
      inventory.assign(slot,weapon < 0 ? std::nullopt : std::optional{static_cast<tfx::weapons::weapon_id>(weapon)});
    }
    double previous_drag{0};std::cin >> previous_drag;
    tfx::presentation::arming_state state{
      .first_store{first},
    };
    if(selected >= 0) state.selected = static_cast<tfx::weapons::weapon_id>(selected);
    renderer.output = background.image;
    tfx::presentation::draw_arming_widgets(renderer,tables.arming_art(),state,inventory,tables.weapons,tables.airframes.at(aircraft),previous_drag);
    std::cout.write(reinterpret_cast<char const*>(renderer.output.pixels.data()),renderer.output.pixels.size());
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
