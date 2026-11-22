#include <cstdlib>
#include <iostream>
#include "presentation/menu.h"
#include "resources/archive.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"menu_probe DATA_DIRECTORY"};
  tfx::resources::archive const game{tfx::resources::find_game_archive(argv[1])};
  std::string name;
  while(std::cin >> name) {
    auto const bytes{game.read("lev/" + name + ".opt")};
    tfx::presentation::menu_program const menu{std::string_view{reinterpret_cast<char const *>(bytes.data()), bytes.size()}};
    std::cout << menu.backdrop << '|' << menu.animation << '|' << menu.cursor_style << '|' << menu.flash_box << '\n';
    for(auto const &choice : menu.choices) {
      std::cout << choice.minimum.x << ' ' << choice.minimum.y << ' ' << choice.maximum.x << ' ' << choice.maximum.y << ' ' << choice.label << ':';
      for(auto const &assignment : choice.assignments) std::cout << assignment.variable << '=' << assignment.value << ',';
      std::cout << '\n';
    }
    unsigned int count{0};
    std::cin >> count;
    for(unsigned int index{0}; index != count; ++index) {
      int x{0};
      int y{0};
      std::cin >> x >> y;
      auto const hit{menu.hit_test({static_cast<int16_t>(x), static_cast<int16_t>(y)})};
      std::cout << (hit ? *hit + 1 : 0) << ' ';
    }
    std::cout << '\n';
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
