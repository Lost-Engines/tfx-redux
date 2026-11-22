#include <cstdlib>
#include <iostream>
#include <map>
#include "render/software_renderer.h"
#include "render/sprites.h"
#include "resources/archive.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"usage: sprite_probe DATA_DIRECTORY"};
  auto const archive{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::archive const game{archive};
  auto const executable{tfx::resources::read_file(archive.parent_path() / "TFX.DAT")};
  tfx::resources::binary_view const source{executable};
  tfx::resources::executable_tables const tables{archive.parent_path() / "TFX.DAT"};
  tfx::render::software_renderer renderer{tables};
  std::map<std::string, std::vector<tfx::resources::indexed_image>> banks;
  char kind{0};
  while(std::cin >> kind) {
    tfx::render::framebuffer image;
    tfx::render::framebuffer coverage;
    image.pixels.fill(99);
    auto const bounds{[&]{
      vec2<int> minimum;
      vec2<int> maximum;
      std::cin >> minimum.x >> minimum.y >> maximum.x >> maximum.y;
      return tfx::render::raster_clip::viewport(minimum, maximum);
    }};
    if(kind == 'S' || kind == 'C') {
      std::string name;
      unsigned int frame{0};
      vec2<int> centre;
      std::cin >> name >> frame >> centre.x >> centre.y;
      if(!banks.contains(name)) banks.emplace(name, tfx::resources::decode_sprites(game.read(name)));
      auto const clip{kind == 'C' ? bounds() : tfx::render::raster_clip{}};
      tfx::render::draw_sprite(image, banks.at(name).at(frame), centre, &coverage, clip);
    } else if(kind == 'B' || kind == 'D') {
      unsigned int field{0};
      unsigned int colour{0};
      vec2<int> position;
      std::cin >> field >> position.x >> position.y >> colour;
      unsigned int const width{source.byte(field)};
      unsigned int const height{source.byte(field + 1)};
      auto const bitmap{tfx::resources::decode_monochrome(source.slice(field, 2 + ((width + 7) / 8) * height))};
      auto const clip{kind == 'D' ? bounds() : tfx::render::raster_clip{}};
      tfx::render::draw_monochrome(image, bitmap, position, static_cast<uint8_t>(colour), &coverage, clip);
    } else if(kind == 'T') {
      vec2<int> position;
      unsigned int colour{0};
      std::string hex;
      std::cin >> position.x >> position.y >> colour >> hex;
      renderer.clipping = bounds();
      std::string text;
      for(unsigned int at{0}; at < hex.size(); at += 2) text += static_cast<char>(std::stoul(hex.substr(at, 2), nullptr, 16));
      renderer.output.pixels.fill(99);
      renderer.text(position.x, position.y, text, static_cast<uint8_t>(colour), &coverage);
      image = renderer.output;
    } else if(kind == 'P') {
      tfx::resources::indexed_image pattern;
      vec2<int> centre;
      vec2<int> minimum;
      vec2<int> maximum;
      bool raw{false};
      std::cin >> pattern.size.x >> pattern.size.y >> centre.x >> centre.y >> minimum.x >> minimum.y >> maximum.x >> maximum.y >> raw;
      pattern.pixels.resize(pattern.size.x * pattern.size.y);
      for(unsigned int y{0}; y != pattern.size.y; ++y) {
        for(unsigned int x{0}; x != pattern.size.x; ++x) pattern.pixels[y * pattern.size.x + x] = static_cast<uint8_t>(x % 254 + 1);
      }
      auto clip{tfx::render::raster_clip::viewport(minimum, maximum)};
      if(raw) {
        clip.sprite_minimum = minimum;
        clip.sprite_limit = maximum;
      }
      tfx::render::draw_sprite(image, pattern, centre, &coverage, clip);
    } else {
      throw std::runtime_error{"unknown image probe command"};
    }
    std::cout.write(reinterpret_cast<char const *>(image.pixels.data()), image.pixels.size());
    std::cout.write(reinterpret_cast<char const *>(coverage.pixels.data()), coverage.pixels.size());
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
