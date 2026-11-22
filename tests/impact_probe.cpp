#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>
#include "world/impact.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int cases{0};
  char operation;
  while(input >> operation) {
    int value;
    if(operation == 'P' || operation == 'W') {
      unsigned int valid;
      unsigned int pixel;
      unsigned int water;
      int x;
      int y;
      input >> valid >> x >> y >> pixel >> water;
      unsigned int cannon{0};
      if(operation == 'W') input >> cannon;
      tfx::render::framebuffer frame;
      if(x >= 0 && x < tfx::render::screen_width && y >= 0 && y < tfx::render::screen_height) frame.pixels[static_cast<unsigned int>(y * tfx::render::screen_width + x)] = static_cast<uint8_t>(pixel);
      std::optional<vec2<int16_t>> point;
      if(valid) point.emplace(static_cast<int16_t>(x), static_cast<int16_t>(y));
      auto const surface{tfx::world::classify_impact(frame, point, water != 0)};
      if(operation == 'W') {
        auto const effect{tfx::world::select_impact_effect(surface, cannon != 0)};
        value = effect ? static_cast<int>(std::to_underlying(*effect)) : -1;
      } else {
        value = std::to_underlying(surface);
      }
    } else if(operation == 'C') {
      int collision;
      int type;
      int surface;
      unsigned int present;
      unsigned int water;
      input >> collision >> present >> type >> surface >> water;
      value = std::to_underlying(tfx::world::select_crash_appearance(static_cast<int16_t>(collision), present ? std::optional{static_cast<int16_t>(type)} : std::nullopt, static_cast<tfx::world::impact_surface>(surface), water != 0));
    } else {
      throw std::runtime_error{"unknown impact operation"};
    }
    if(argc == 2) {
      int expected;
      if(!(input >> expected) || value != expected) throw std::runtime_error{"impact mismatch at case " + std::to_string(cases)};
    } else {
      std::cout << value << '\n';
    }
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
