#include <cstdlib>
#include <iostream>
#include "resources/archive.h"
#include "resources/media.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"media_probe DATA_DIRECTORY"};
  tfx::resources::archive const game{tfx::resources::find_game_archive(argv[1])};
  char mode{0};uint32_t id{0};
  auto const word{[](uint32_t const value){ for(unsigned int shift{0}; shift != 32; shift += 8) std::cout.put(static_cast<char>(value >> shift)); }};
  auto const bytes{[](auto const &values){ std::cout.write(reinterpret_cast<char const*>(values.data()),static_cast<std::streamsize>(values.size())); }};
  auto const palette{[&](tfx::render::palette const &colours){ for(auto const &colour : colours) for(unsigned int channel{0}; channel != 3; ++channel) std::cout.put(static_cast<char>(colour[channel])); }};
  while(std::cin >> mode >> id) {
    auto const source{game.read(id)};
    if(mode == 'A') {
      tfx::resources::animation animation{source};
      word(animation.game_frames);word(animation.initial_full);word(animation.block_count());palette(animation.colours);
      while(animation.advance()) { bytes(animation.image().pixels);bytes(animation.mask().pixels); }
    } else if(mode == 'B') {
      auto const image{tfx::resources::decode_screen(source)};
      palette(image.colours);bytes(image.image.pixels);
    } else if(mode == 'S') {
      auto const sprites{tfx::resources::decode_sprites(source)};
      word(static_cast<uint32_t>(sprites.size()));
      for(auto const &sprite : sprites) { word(sprite.size.x);word(sprite.size.y);bytes(sprite.pixels); }
    } else throw std::runtime_error{"unknown media mode"};
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
