#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "render/point_projection.h"
#include "world/instance.h"

std::vector<uint8_t> definition(bool const all_parameters) {
  std::vector<uint8_t> source(14);
  source[1] = 1;
  source[5] = all_parameters ? 1 : 0;
  source[8] = 2;
  auto const word{[&](uint16_t const value){
    source.push_back(static_cast<uint8_t>(value));
    source.push_back(static_cast<uint8_t>(value >> 8));
  }};
  source[10] = 2;
  for(unsigned int parameter{0}; parameter != 8; ++parameter) {
    for(auto const value : {20u + parameter, 0u, 4u, 1u, 4u, 65534u}) word(static_cast<uint16_t>(value));
  }
  word(0);
  auto const entry{static_cast<uint16_t>((source.size() - 12) / 2)};
  source[12] = static_cast<uint8_t>(entry);
  source[13] = static_cast<uint8_t>(entry >> 8);
  word(0);
  return source;
}

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{fixture.is_open() ? fixture : std::cin};
  bool all;
  unsigned int count;
  unsigned int cases{0};
  while(input >> all >> count) {
    tfx::world::definition const source{definition(all), std::span<std::string const>{}};
    tfx::world::instance state{source};
    state.parts.front().parameters.fill(250);
    uint64_t hash{14695981039346656037ull};
    for(unsigned int i{0}; i != count; ++i) {
      unsigned int policy;
      bool enabled;
      bool wide;
      tfx::render::point_projection view;
      vec3<int32_t> origin;
      int16_t radius;
      input >> policy >> enabled >> wide;
      for(auto &value : view.matrix.data) input >> value;
      input >> view.position_q16.x >> view.position_q16.y >> view.position_q16.z >> view.origin.x >> view.origin.y >> view.maximum.x >> view.maximum.y >> origin.x >> origin.y >> origin.z >> radius;
      view.opening = wide ? tfx::render::aperture::missile_nose : tfx::render::aperture::normal;
      bool const visible{view.camera_sphere_visible(view.camera_fixed(origin), radius)};
      if(tfx::world::animation_eligible(static_cast<tfx::world::animation_policy>(policy), enabled, visible)) state.animate();
      for(auto const value : state.parts.front().parameters) hash = (hash ^ value) * 1099511628211;
      for(unsigned int parameter{0}; parameter != 8; ++parameter) {
        uint16_t const value{static_cast<uint16_t>(state.stream_cursor(0, parameter).value_or(0))};
        hash = (hash ^ static_cast<uint8_t>(value)) * 1099511628211;
        hash = (hash ^ static_cast<uint8_t>(value >> 8)) * 1099511628211;
      }
    }
    if(fixture.is_open()) {
      uint64_t expected;
      if(!(input >> expected) || expected != hash) throw std::runtime_error{"native animation eligibility mismatch at " + std::to_string(cases)};
    } else {
      std::cout << hash << '\n';
    }
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
