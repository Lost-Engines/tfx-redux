#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "render/precipitation.h"

int main(int const argc, char const *const argv[]) try {
  if(argc >= 2 && std::string_view{argv[1]} == "--gate") {
    std::ifstream fixture;if(argc == 3) fixture.open(argv[2]);
    if(argc > 3 || (argc == 3 && !fixture)) throw std::runtime_error{"expected --gate [FILE]"};
    auto &input{argc == 3 ? fixture : std::cin};
    int enabled,storm,layer,view;uint32_t height;int32_t speed;unsigned int cases{0};
    while(input >> enabled >> storm >> layer >> height >> view >> speed) {
      auto const count{tfx::render::precipitation_count({
        .enabled{enabled != 0},
        .storm{storm != 0},
        .layer_y{static_cast<int16_t>(layer)},
        .player_y{std::bit_cast<float>(height)},
        .view{static_cast<tfx::cockpit::view_mode>(view)},
        .speed{speed},
      })};
      int32_t const value{count ? *count : 65536};
      if(argc == 3) { int32_t expected;if(!(input >> expected) || expected != value) throw std::runtime_error{"native rain gate mismatch at case " + std::to_string(cases)}; } else std::cout << value << '\n';
      ++cases;
    }
    return cases ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  bool const pixels{argc == 2 && std::string_view{argv[1]} == "--pixels"};
  std::ifstream fixture;if(argc == 2 && !pixels) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !pixels && !fixture)) throw std::runtime_error{"expected [FILE | --pixels]"};
  auto &input{fixture.is_open() ? fixture : std::cin};
  unsigned int cases{0};
  unsigned int low,high;
  while(input >> low >> high) {
    vec3<int16_t> drift;int16_t count;uint32_t wx,wz;
    input >> drift.x >> drift.y >> drift.z >> count >> wx >> wz;
    mat3<int32_t> matrix;
    for(auto &value : matrix.data) input >> value;
    tfx::render::precipitation_view view{
      .matrix{matrix},
    };
    input >> view.camera.x >> view.camera.y >> view.camera.z >> view.origin.x >> view.origin.y >> view.minimum.x >> view.minimum.y >> view.maximum.x >> view.maximum.y;
    tfx::render::precipitation rain{drift};
    tfx::math::random_state random{
      .low{static_cast<uint16_t>(low)},
      .high{static_cast<uint16_t>(high)},
    };
    rain.initialise(random);
    uint64_t field_hash{14695981039346656037ull};
    for(auto const &point : rain.field()) for(unsigned int axis{0}; axis != 3; ++axis) for(unsigned int shift{0}; shift != 16; shift += 8) field_hash = (field_hash ^ static_cast<uint8_t>(static_cast<uint16_t>(point[axis]) >> shift)) * 1099511628211;
    tfx::render::framebuffer output;output.pixels.fill(77);
    rain.paint(output,view,count,{std::bit_cast<float>(wx),std::bit_cast<float>(wz)});
    uint64_t pixel_hash{14695981039346656037ull};
    for(auto const value : output.pixels) pixel_hash = (pixel_hash ^ value) * 1099511628211;
    if(pixels) std::cout.write(reinterpret_cast<char const*>(output.pixels.data()),output.pixels.size());
    else {
      uint64_t const values[]{random.low,random.high,field_hash,static_cast<uint16_t>(rain.drift().x),static_cast<uint16_t>(rain.drift().y),static_cast<uint16_t>(rain.drift().z),pixel_hash};
      if(fixture.is_open()) {
        for(auto const value : values) { uint64_t expected;if(!(input >> expected) || expected != value) throw std::runtime_error{"native precipitation mismatch at case " + std::to_string(cases)}; }
      } else { for(auto const value : values) std::cout << value << ' ';std::cout << '\n'; }
    }
    if(!input) throw std::runtime_error{"incomplete precipitation input"};
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
