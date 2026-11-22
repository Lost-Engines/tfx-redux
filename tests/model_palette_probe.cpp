#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "models/shape.h"
#include "render/integer_model_polygon.h"

std::vector<uint8_t> program(unsigned int const transition, bool const nested, std::array<uint16_t,3> const &colours, std::array<uint16_t,6> const &depths) {
  std::vector<uint8_t> source(50);
  auto const store{[&](unsigned int const at,uint16_t const value){ source.at(at) = static_cast<uint8_t>(value >> 8);source.at(at + 1) = static_cast<uint8_t>(value); }};
  auto const emit{[&](std::initializer_list<uint16_t> const words){ for(auto const value : words) { source.push_back(static_cast<uint8_t>(value >> 8));source.push_back(static_cast<uint8_t>(value)); } }};
  store(2,32767);store(34,14);store(48,65535);emit({48,1});
  for(unsigned int face{0}; face != 6; ++face) {
    int const x{static_cast<int>(face % 3) * 400 - 500},y{static_cast<int>(face / 3) * 400 - 300};
    for(unsigned int vertex{0}; vertex != 3; ++vertex) emit({98,static_cast<uint16_t>(face * 3 + vertex),static_cast<uint16_t>(x + (vertex == 1 ? 280 : 0)),static_cast<uint16_t>(y + (vertex == 2 ? 280 : 0)),depths[face]});
  }
  emit({97,18,0});
  auto const face{[&](unsigned int const index){
    if(index == transition) emit({84});
    uint16_t const first{static_cast<uint16_t>(index * 3)};
    if(index == 1 || index == 3) emit({113,first,static_cast<uint16_t>(first + 1),static_cast<uint16_t>(first + 2)});
    else if(index == 4) emit({76,0xc0cf,0xc700,77,first,static_cast<uint16_t>(first + 1),static_cast<uint16_t>(first + 2)});
    else emit({2,colours[index == 0 ? 0 : index == 2 ? 1 : 2],first,static_cast<uint16_t>(first + 1),static_cast<uint16_t>(first + 2)});
  }};
  face(0);face(1);
  if(nested) {
    emit({8,0});auto const target{static_cast<unsigned int>(source.size() - 2)};
    face(5);emit({0});store(target,static_cast<uint16_t>(source.size() - target));
    face(2);face(3);face(4);emit({0,65535});
  } else { for(unsigned int i{2}; i != 6; ++i) face(i);emit({0,65535}); }
  return source;
}

int main(int const argc, char const *const argv[]) try {
  bool pixels{false};std::ifstream fixture;
  for(int i{1}; i != argc; ++i) if(std::string_view{argv[i]} == "--pixels") pixels = true;else { fixture.open(argv[i]);if(!fixture) throw std::runtime_error{"cannot open model palette fixture"}; }
  auto &input{fixture.is_open() ? fixture : std::cin};unsigned int transition,object_offset,fixed_offset,cases{0};bool nested,wide;
  while(input >> transition >> nested >> wide >> object_offset >> fixed_offset) {
    std::array<uint16_t,3> colours;std::array<uint16_t,6> depths;
    for(auto &value : colours) input >> value;
    for(auto &value : depths) input >> value;
    tfx::models::shape const shape{program(transition,nested,colours,depths)};
    auto view{tfx::render::original_camera({0,0,0},mat3<int32_t>{},{160,100},wide ? tfx::render::aperture::missile_nose : tfx::render::aperture::normal)};view.far_distance = 1e10;
    tfx::models::shape_context context;
    context.facing = false;
    context.camera.emplace(tfx::models::shape_camera{
      .basis{32767,0,0,0,32767,0,0,0,32767},
      .origin{0,0,0},
      .projection_shift{tfx::render::projection_shift(view.opening)},
    });
    auto const geometry{tfx::models::interpret(shape,context)};
    if(!geometry.warnings.empty() || geometry.primitives.size() != 6) throw std::runtime_error{"palette model emitted incomplete geometry"};
    tfx::render::colour_map object,fixed;
    for(unsigned int i{0}; i != object.size(); ++i) { object[i] = static_cast<uint8_t>(i * 5 + object_offset);fixed[i] = static_cast<uint8_t>(255 - i + fixed_offset); }
    tfx::render::framebuffer output;output.pixels.fill(99);bool defined{true};
    for(auto const &face : geometry.primitives) {
      auto const &remap{face.palette == tfx::models::palette_source::fixed ? fixed : object};
      if(!tfx::render::integer_model_polygon(output,face.vertices,view,remap.at(face.colour),&remap)) defined = false;
    }
    uint64_t hash{14695981039346656037ull};for(auto const value : output.pixels) hash = (hash ^ value) * 1099511628211;
    if(pixels) std::cout.write(reinterpret_cast<char const*>(output.pixels.data()),output.pixels.size());
    else if(fixture.is_open()) { uint64_t expected;if(!defined || !(input >> expected) || hash != expected) throw std::runtime_error{"native model palette mismatch at " + std::to_string(cases)}; } else std::cout << hash << ' ' << defined << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
