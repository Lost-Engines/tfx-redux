#include <cstdlib>
#include <iostream>
#include "presentation/pilot_panel.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 3) throw std::runtime_error{"pilot_panel_probe DATA_DIRECTORY SAVE_DIRECTORY"};
  tfx::resources::archive const game{tfx::resources::find_game_archive(argv[1])};
  auto const executable{tfx::resources::read_file(std::filesystem::path{argv[1]} / "TFX.DAT")};
  tfx::presentation::fonts const fonts{executable};
  tfx::presentation::pilot_assets const assets{executable};
  tfx::presentation::program const schema{game.read("tfx.par")};
  tfx::presentation::pilot_store const store{argv[2]};
  auto const background{tfx::resources::decode_screen(game.read("iff/raw/com_enrl.lbm"))};
  for(auto const &colour : background.colours) {
    std::cout.put(static_cast<char>(colour.x));
    std::cout.put(static_cast<char>(colour.y));
    std::cout.put(static_cast<char>(colour.z));
  }
  std::cout.write(reinterpret_cast<char const *>(background.image.pixels.data()), background.image.pixels.size());
  bool load{false};
  bool confirm{false};
  unsigned int selected{0};
  while(std::cin >> load >> selected >> confirm) {
    if(selected >= tfx::presentation::pilot_slots) throw std::runtime_error{"pilot panel selection exceeds its slot count"};
    tfx::presentation::variables state;
    tfx::presentation::reset_pilot(schema, state);
    tfx::presentation::pilot_panel panel{game, fonts, assets, schema, state, store};
    for(unsigned int frame{0}; frame != 40; ++frame) panel.advance();
    if(load) panel.press(0x50);
    panel.press(0x1c);
    if(!load) {
      for(unsigned int field{0}; field != 3; ++field) panel.press(0x1c);
    }
    for(unsigned int row{0}; row != selected; ++row) panel.press(0x50);
    if(confirm) panel.press(0x1c);
    panel.advance();
    auto const &pixels{panel.image().pixels};
    std::cout.write(reinterpret_cast<char const *>(pixels.data()), pixels.size());
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
