#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "models/shape.h"
#include "render/colour_remap.h"
#include "render/integer_model_polygon.h"

namespace {
unsigned int constexpr face_count{7}, vertices_per_face{4};
std::vector<uint8_t> program(bool const nested, bool const fixed, uint16_t const threshold, std::array<uint16_t,3> const &colours, std::array<uint16_t,face_count * vertices_per_face> const &depths) {
  std::vector<uint8_t> source(50);
  auto const store{[&](unsigned int const at,uint16_t const value){ source.at(at) = static_cast<uint8_t>(value >> 8);source.at(at + 1) = static_cast<uint8_t>(value); }};
  auto const emit{[&](std::initializer_list<uint16_t> const words){ for(auto const value : words) { source.push_back(static_cast<uint8_t>(value >> 8));source.push_back(static_cast<uint8_t>(value)); } }};
  auto const link{[&](unsigned int const field){ store(field,static_cast<uint16_t>(source.size() - field)); }};
  store(2,32767);store(34,14);store(48,65535);emit({48,1});
  for(unsigned int face{0}; face != face_count; ++face) {
    int const x{static_cast<int>(face % 3) * 3000 - 4000},y{static_cast<int>(face / 3) * 2400 - 3000};
    for(unsigned int vertex{0}; vertex != vertices_per_face; ++vertex) emit({98,static_cast<uint16_t>(face * vertices_per_face + vertex),static_cast<uint16_t>(x + (vertex == 1 || vertex == 2 ? 2200 : 0)),static_cast<uint16_t>(y + (vertex >= 2 ? 1800 : 0)),depths[face * vertices_per_face + vertex]});
  }
  emit({97,face_count * vertices_per_face,0,74,0,38,threshold,0});
  unsigned int const branch{static_cast<unsigned int>(source.size() - 2)};
  emit({2,colours[0],0,1,2,7,0});unsigned int const join{static_cast<unsigned int>(source.size() - 2)};
  link(branch);emit({2,colours[2],0,1,2});link(join);
  emit({118,0});unsigned int const helper{static_cast<unsigned int>(source.size() - 2)};
  emit({113,20,21,22,2,colours[0],24,25,26,0});link(helper);
  emit({2,colours[1],4,5,6,113,8,9,10});
  if(nested) { emit({8,0});unsigned int const child{static_cast<unsigned int>(source.size() - 2)};emit({114,16,17,18,19,0});link(child); }
  if(fixed) emit({84});
  emit({3,colours[2],12,13,14,15});
  if(!nested) emit({114,16,17,18,19});
  emit({0,65535});
  return source;
}
} // anonymous namespace
int main(int const argc, char const *const argv[]) try {
  bool pixels{false};std::ifstream fixture;
  for(int i{1}; i != argc; ++i) if(std::string_view{argv[i]} == "--pixels") pixels = true;else { fixture.open(argv[i]);if(!fixture) throw std::runtime_error{"cannot open face palette fixture"}; }
  auto &input{fixture.is_open() ? fixture : std::cin};unsigned int object_offset,fixed_offset,cases{0};bool wide,nested,fixed;uint16_t threshold;
  while(input >> wide >> nested >> fixed >> threshold >> object_offset >> fixed_offset) {
    std::array<uint16_t,3> colours;std::array<uint16_t,face_count * vertices_per_face> depths;
    for(auto &value : colours) input >> value;
    for(auto &value : depths) input >> value;
    tfx::models::shape const shape{program(nested,fixed,threshold,colours,depths)};
    tfx::models::shape_context context;
    context.facing = false;
    context.distance = 123;
    context.camera.emplace(tfx::models::shape_camera{
      .basis{32767,0,0,0,32767,0,0,0,32767},
      .origin{0,0,0},
      .projection_shift{tfx::render::projection_shift(wide ? tfx::render::aperture::missile_nose : tfx::render::aperture::normal)},
    });
    auto const geometry{tfx::models::interpret(shape,context)};
    if(!geometry.warnings.empty() || geometry.primitives.size() != face_count) throw std::runtime_error{"face palette model emitted incomplete geometry"};
    auto view{tfx::render::original_camera({0,0,0},mat3<int32_t>{},{160,100},wide ? tfx::render::aperture::missile_nose : tfx::render::aperture::normal)};view.far_distance = 1e10;
    tfx::render::colour_map object;std::array<tfx::render::colour_map,tfx::render::colour_map_count> rows;
    for(unsigned int i{0}; i != object.size(); ++i) {
      object[i] = static_cast<uint8_t>(i * 5 + object_offset);
      for(unsigned int row{0}; row != rows.size(); ++row) rows[row][i] = static_cast<uint8_t>(i * 3 + fixed_offset + row * 17);
    }
    tfx::render::framebuffer output;output.pixels.fill(99);bool defined{true};
    for(auto const &face : geometry.primitives) {
      unsigned int const row{face.palette == tfx::models::palette_source::face_depth ? tfx::render::distance_colour_row(static_cast<int32_t>(face.palette_depth) * 65536) : 0u};
      auto const &remap{face.palette == tfx::models::palette_source::distance ? object : rows[row]};
      if(!tfx::render::integer_model_polygon(output,face.vertices,view,remap.at(face.colour),&remap)) defined = false;
    }
    uint64_t hash{14695981039346656037ull};for(auto const value : output.pixels) hash = (hash ^ value) * 1099511628211;
    if(pixels) std::cout.write(reinterpret_cast<char const*>(output.pixels.data()),output.pixels.size());
    else if(fixture.is_open()) { uint64_t expected;if(!defined || !(input >> expected) || hash != expected) throw std::runtime_error{"native depth palette mismatch at " + std::to_string(cases)}; } else std::cout << hash << ' ' << defined << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
