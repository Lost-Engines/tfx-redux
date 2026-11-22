#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <utility>
#include "cockpit/frame.h"
#include "cockpit/initialisation.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 3) throw std::runtime_error{"usage: cockpit_frame_probe DATA_DIRECTORY frame|layouts|setup|position"};
  auto const archive{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::archive const game{archive};
  tfx::resources::executable_tables const tables{archive.parent_path() / "TFX.DAT"};
  auto const &assets{tables.instruments()};
  std::string_view const mode{argv[2]};
  if(mode == "layouts") {
    for(auto const &view : assets.viewports) std::cout << view.projection_centre_y << ' ' << view.bottom << ' ' << view.callback_file << ' ' << std::quoted(view.panel) << ' ' << std::quoted(view.overlay) << '\n';
  } else if(mode == "setup") {
    unsigned int aircraft{0};
    bool landed{false};
    while(std::cin >> aircraft >> landed) {
      auto const state{tfx::cockpit::initialise_instruments(static_cast<tfx::flight::player_aircraft>(aircraft + 1), landed)};
      auto const &selection{state.displays};
      std::cout << selection.left << ' ' << std::to_underlying(selection.centre) << ' ' << selection.right << ' ' << selection.horizontal_range << ' ' << selection.bvr_range << ' ' << state.offset.x << ' ' << state.offset.y << ' ' << state.fuel.reading << ' ' << state.fuel.smoothed_burn << ' ' << static_cast<unsigned int>(state.chaff) << ' ' << static_cast<unsigned int>(state.flares) << ' ' << state.stealth;
      for(auto const condition : state.systems) std::cout << ' ' << static_cast<unsigned int>(std::to_underlying(condition));
      std::cout << '\n';
    }
  } else if(mode == "position" || mode == "position-shake") {
    tfx::cockpit::cockpit_position position{assets, {0, 28}};
    unsigned int action{0};
    unsigned int view{0};
    while(std::cin >> action >> view) {
      if(action == 1) position.raise();
      if(action == 2) position.lower();
      if(mode == "position-shake") {
        vec2<int16_t> requested;
        if(!(std::cin >> requested.x >> requested.y)) throw std::runtime_error{"incomplete cockpit-shake input"};
        if(action == 3) position.shake();
        if(action == 4) position.move_to(requested);
        if(action == 5) position.bump();
      }
      position.advance(assets.viewports.at(view).bottom, view == 0);
      std::cout << position.value().x << ' ' << position.value().y << ' ' << position.step_index() << ' ' << std::to_underlying(position.movement()) << '\n';
    }
  } else if(mode == "frame" || mode == "frame-retained") {
    tfx::cockpit::frame const frame{assets, game};
    unsigned int view{0};
    vec2<int16_t> requested;
    while(std::cin >> view >> requested.x >> requested.y) {
      bool copy{true};
      if(mode == "frame-retained" && !(std::cin >> copy)) throw std::runtime_error{"missing footer-copy decision"};
      auto const offset{frame.clamp(view, requested)};
      auto const origin{frame.projection_origin(view, offset)};
      tfx::render::framebuffer pixels;
      tfx::render::framebuffer coverage;
      pixels.pixels.fill(99);
      frame.paint(pixels, view, offset, &coverage, copy ? tfx::cockpit::panel_copy::refresh : tfx::cockpit::panel_copy::preserve);
      for(int const value : {static_cast<int>(offset.x), static_cast<int>(offset.y), static_cast<int>(origin.x), static_cast<int>(origin.y)}) {
        for(unsigned int byte{0}; byte != 4; ++byte) std::cout.put(static_cast<char>(static_cast<uint32_t>(value) >> (byte * 8)));
      }
      std::cout.write(reinterpret_cast<char const *>(pixels.pixels.data()), pixels.pixels.size());
      std::cout.write(reinterpret_cast<char const *>(coverage.pixels.data()), coverage.pixels.size());
    }
  } else {
    throw std::runtime_error{"unknown cockpit-frame probe mode"};
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
