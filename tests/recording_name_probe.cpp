#include <iomanip>
#include <iostream>
#include "presentation/configuration_menu.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"recording_name_probe TFX.DAT"};
  auto const executable{tfx::resources::read_file(argv[1])};
  tfx::presentation::configuration_menu_assets const assets{executable};
  tfx::presentation::widget_font const font{executable};
  unsigned int count{0};
  while(std::cin >> count) {
    tfx::configuration::settings preferences;
    preferences.control = tfx::flight::control_source::mouse;
    preferences.detail = 2;
    preferences.effects = {
      .g_force{tfx::configuration::g_effects::none},
      .precipitation{0},
      .crashing{1},
      .gear_damage{1},
      .wind{0},
      .temperature{0},
      .recovery{0},
    };
    auto name{assets.recording_name};
    tfx::presentation::configuration_menu menu{preferences, assets, font, name};
    menu.click({160, 140});
    for(unsigned int index{0}; index != count; ++index) {
      unsigned int scan{0};
      unsigned int shift{0};
      if(!(std::cin >> scan >> shift)) throw std::runtime_error{"truncated recording-name input"};
      menu.press(static_cast<uint8_t>(scan), shift != 0);
      std::cout << std::quoted(name) << '\n';
      tfx::render::framebuffer image;
      menu.draw(image);
      std::cout.write(reinterpret_cast<char const *>(image.pixels.data()), image.pixels.size());
    }
  }
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
