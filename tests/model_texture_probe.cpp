#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "models/shape.h"
#include "render/integer_model_texture.h"

int main(int const argc, char const *const argv[]) try {
  bool pixels{false};
  std::ifstream fixture;
  for(int i{1}; i != argc; ++i) {
    if(std::string_view{argv[i]} == "--pixels") {
      pixels = true;
    } else {
      fixture.open(argv[i]);
      if(!fixture) throw std::runtime_error{"cannot open model texture fixture"};
    }
  }
  auto &input{fixture.is_open() ? fixture : std::cin};
  bool wide;
  unsigned int count;
  unsigned int offset;
  unsigned int cases{0};
  std::array<uint8_t, 65536> texture;
  for(unsigned int v{0}; v != 256; ++v) {
    for(unsigned int u{0}; u != 256; ++u) texture[v * 256 + u] = static_cast<uint8_t>(u * 3 + v * 5 + (u ^ v));
  }
  while(input >> wide >> count >> offset) {
    std::vector<vec3<int16_t>> points(count);
    std::vector<vec2<uint16_t>> uv(count);
    for(unsigned int i{0}; i != count; ++i) input >> points[i].x >> points[i].y >> points[i].z >> uv[i].x >> uv[i].y;
    std::vector<uint8_t> source(50);
    auto const store{[&](unsigned int const at, uint16_t const value){
      source.at(at) = static_cast<uint8_t>(value >> 8);
      source.at(at + 1) = static_cast<uint8_t>(value);
    }};
    auto const word{[&](uint16_t const value){
      source.push_back(static_cast<uint8_t>(value >> 8));
      source.push_back(static_cast<uint8_t>(value));
    }};
    auto const emit{[&](std::initializer_list<uint16_t> const words){
      for(auto const value : words) word(value);
    }};
    store(2, 32767);
    store(34, 14);
    store(48, 65535);
    emit({48, 1});
    for(unsigned int i{0}; i != count; ++i) emit({98, static_cast<uint16_t>(i), static_cast<uint16_t>(points[i].x), static_cast<uint16_t>(points[i].y), static_cast<uint16_t>(points[i].z)});
    emit({97, static_cast<uint16_t>(count), 0, 71, 0, static_cast<uint16_t>(count)});
    for(auto const &v : uv) emit({v.x, v.y});
    emit({47, static_cast<uint16_t>(count)});
    for(unsigned int i{0}; i != count; ++i) word(static_cast<uint16_t>(i));
    emit({0, 65535});
    tfx::models::shape const shape{source};
    tfx::models::shape_context context;
    context.facing = false;
    context.camera.emplace(tfx::models::shape_camera{
      .basis{32767, 0, 0, 0, 32767, 0, 0, 0, 32767},
      .origin{0, 0, 0},
      .projection_shift{tfx::render::projection_shift(wide ? tfx::render::aperture::missile_nose : tfx::render::aperture::normal)},
    });
    auto const geometry{tfx::models::interpret(shape, context)};
    if(!geometry.warnings.empty() || geometry.primitives.size() != 1) throw std::runtime_error{"texture model emitted incomplete geometry"};
    auto view{tfx::render::original_camera({0, 0, 0}, mat3<int32_t>{}, {160, 100}, wide ? tfx::render::aperture::missile_nose : tfx::render::aperture::normal)};
    view.far_distance = 1e10;
    tfx::render::colour_map remap;
    for(unsigned int i{0}; i != remap.size(); ++i) remap[i] = static_cast<uint8_t>(i * 5 + offset);
    tfx::render::framebuffer output;
    output.pixels.fill(99);
    bool const defined{tfx::render::integer_model_texture(output, geometry.primitives.front().vertices, view, texture, remap)};
    uint64_t hash{14695981039346656037ull};
    for(auto const value : output.pixels) hash = (hash ^ value) * 1099511628211;
    if(pixels) {
      std::cout.write(reinterpret_cast<char const *>(output.pixels.data()), output.pixels.size());
    } else if(fixture.is_open()) {
      uint64_t expected;
      if(!defined || !(input >> expected) || hash != expected) throw std::runtime_error{"native textured model mismatch at " + std::to_string(cases)};
    } else {
      std::cout << hash << ' ' << defined << '\n';
    }
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
