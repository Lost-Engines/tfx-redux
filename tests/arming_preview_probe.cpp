#include <cstdlib>
#include <iostream>
#include "presentation/arming_screen.h"

int main(int const argc, char const *const argv[]) try {
  if(argc < 3) throw std::runtime_error{"usage: arming_preview_probe DATA_DIRECTORY sequence|incomplete|picks|frames|flow|fades|animations"};
  tfx::resources::archive const game{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::executable_tables const tables{tfx::resources::find_game_archive(argv[1]).parent_path() / "TFX.DAT"};
  if(std::string_view{argv[2]} == "sequence" || std::string_view{argv[2]} == "incomplete") {
    bool const incomplete{std::string_view{argv[2]} == "incomplete"};
    unsigned int selected{0};
    std::cin >> selected;
    auto const aircraft{static_cast<tfx::flight::player_aircraft>(selected + 1)};
    tfx::weapons::armament inventory{aircraft, tables.weapons, tables.arming};
    tfx::presentation::arming_screen screen{game, tables, inventory, aircraft, 0, incomplete ? tfx::presentation::arming_entry::mission_incomplete : tfx::presentation::arming_entry::normal};
    auto const output{[&]{
      auto const &pixels{screen.image().pixels};
      std::cout.write(reinterpret_cast<char const *>(pixels.data()), pixels.size());
    }};
    output();
    for(unsigned int frame{0}; frame != 40; ++frame) screen.advance({100, 75}, {0, 0}, 0);
    int x{0};
    int y{0};
    int dx{0};
    int dy{0};
    unsigned int buttons{0};
    while(std::cin >> x >> y >> dx >> dy >> buttons) {
      if(screen.advance({static_cast<int16_t>(x), static_cast<int16_t>(y)}, {static_cast<int16_t>(dx), static_cast<int16_t>(dy)}, static_cast<uint8_t>(buttons))) throw std::runtime_error{"notice dismissal activated DONE"};
      output();
    }
  } else if(std::string_view{argv[2]} == "fades") {
    tfx::weapons::armament inventory{tfx::flight::player_aircraft::eurofighter, tables.weapons, tables.arming};
    tfx::presentation::arming_screen screen{game, tables, inventory, tfx::flight::player_aircraft::eurofighter, 0};
    auto const output{[&]{
      for(auto const &colour : screen.colours()) std::cout.write(reinterpret_cast<char const *>(colour.data()), 3);
    }};
    output();
    for(unsigned int step{1}; step != 40; ++step) {
      screen.advance({210, 190}, {0, 0}, 0);
      output();
    }
    screen.advance({210, 190}, {0, 0}, 0);
    screen.press(1);
    screen.advance({210, 190}, {0, 0}, 0);
    output();
    for(unsigned int step{1}; step != 40; ++step) {
      screen.advance({210, 190}, {0, 0}, 0);
      output();
    }
    screen.advance({210, 190}, {0, 0}, 0);
    output();
  } else if(std::string_view{argv[2]} == "picks") {
    std::array<std::unique_ptr<tfx::presentation::arming_preview>, 3> previews;
    for(unsigned int plane{0}; plane != previews.size(); ++plane) previews[plane] = std::make_unique<tfx::presentation::arming_preview>(game, tables, static_cast<tfx::flight::player_aircraft>(plane + 1));
    unsigned int plane{0};
    int x{0};
    int y{0};
    int z{0};
    while(std::cin >> plane >> x >> y >> z) {
      vec3<int16_t> const previous{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(z)};
      std::cin >> x >> y >> z;
      auto const picked{previews.at(plane)->pick(previous, {static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(z)})};
      std::cout << (picked ? static_cast<int>(*picked) : -1) << '\n';
    }
  } else if(std::string_view{argv[2]} == "animations") {
    for(unsigned int plane{0}; plane != 3; ++plane) {
      tfx::presentation::arming_preview preview{game, tables, static_cast<tfx::flight::player_aircraft>(plane + 1)};
      preview.start();
      for(unsigned int frame{0}; frame != 20; ++frame) {
        preview.advance();
        std::cout << static_cast<unsigned int>(preview.bay_position()) << ' ';
      }
      std::cout << '\n';
    }
  } else {
    for(unsigned int plane{0}; plane != 3; ++plane) {
      auto const aircraft{static_cast<tfx::flight::player_aircraft>(plane + 1)};
      tfx::weapons::armament inventory{aircraft, tables.weapons, tables.arming};
      tfx::presentation::arming_screen screen{game, tables, inventory, aircraft, 0};
      for(unsigned int frame{0}; frame != 120; ++frame) screen.advance({210, 190}, {0, 0}, 0);
      if(std::string_view{argv[2]} == "flow") {
        auto const move{[&](vec2<int16_t> const &point, uint8_t const buttons = 0){screen.advance(point, {0, 0}, buttons);}};
        if(plane == 1) {
          move({308, 86}, 1);
          move({308, 86}, 1);
          move({308, 86});
        }
        move({250, 30}, 1);
        move({250, 30});
        vec2<int16_t> const mount{plane == 0 ? int16_t{214} : plane == 1 ? int16_t{171} : int16_t{205}, plane == 0 ? int16_t{166} : plane == 1 ? int16_t{130} : int16_t{109}};
        move(mount);
        move(mount);
        move(mount, 1);
        move(mount);
        move(mount, 2);
        move(mount);
        screen.press(1);
        bool finished{false};
        for(unsigned int frame{0}; frame != 45 && !finished; ++frame) finished = screen.advance(mount, {0, 0}, 0);
        if(!finished) throw std::runtime_error{"arming did not finish after Escape and fade"};
        unsigned int const weapon{plane == 1 ? 8u : 1u};
        unsigned int const slot{plane == 1 ? 3u : 1u};
        for(unsigned int index{0}; index != tfx::flight::store_slots; ++index) {
          bool const occupied{index == slot || index == tables.arming_art().paired[slot]};
          auto const &store{inventory.pylons[index]};
          if(store.visible != occupied || store.count != occupied || store.weapon != (occupied ? weapon : tfx::weapons::empty_pylon)) throw std::runtime_error{"arming pointer flow did not preserve the selected paired stores"};
        }
        std::cout << "Aircraft " << plane << ": pointer selection, placement, cancellation and selected loadout exit passed\n";
      }
      screen.image().screenshot("/tmp/tfx-arming-" + std::to_string(plane) + ".ppm", screen.colours());
    }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
