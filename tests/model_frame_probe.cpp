#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "models/shape.h"
#include "render/integer_model_polygon.h"
#include "render/model_raster.h"

namespace {
tfx::resources::bytes decode_hex(std::string_view const hex) {
  if(hex.size() % 2) throw std::runtime_error{"model hex has an incomplete byte"};
  tfx::resources::bytes result;
  result.reserve(hex.size() / 2);
  for(unsigned int at{0}; at != hex.size(); at += 2) {
    unsigned int value;
    auto const parsed{std::from_chars(hex.data() + at, hex.data() + at + 2, value, 16)};
    if(parsed.ec != std::errc{} || parsed.ptr != hex.data() + at + 2) throw std::runtime_error{"invalid model hex"};
    result.push_back(static_cast<uint8_t>(value));
  }
  return result;
}
} // anonymous namespace
int main(int const argc, char const *const argv[]) try {
  bool pixels{false};
  bool viewport{false};
  std::ifstream fixture;
  for(int i{1}; i != argc; ++i) {
    if(std::string_view{argv[i]} == "--pixels") {
      pixels = true;
    } else if(std::string_view{argv[i]} == "--viewport") {
      viewport = true;
    } else {
      fixture.open(argv[i]);
      if(!fixture) throw std::runtime_error{"cannot open model-frame fixture"};
    }
  }
  auto &input{fixture.is_open() ? fixture : std::cin};
  bool wide;
  unsigned int seed;
  unsigned int object_offset;
  unsigned int fixed_offset;
  unsigned int cases{0};
  int16_t distance;
  uint16_t status;
  std::array<uint8_t, 65536> texture;
  for(unsigned int v{0}; v != 256; ++v) {
    for(unsigned int u{0}; u != 256; ++u) texture[v * 256 + u] = static_cast<uint8_t>(u * 3 + v * 5 + (u ^ v));
  }
  while(input >> wide >> seed >> object_offset >> fixed_offset >> distance >> status) {
    tfx::models::shape_context context;
    mat3<int32_t> basis;
    vec3<int32_t> origin;
    for(auto &value : basis.data) input >> value;
    input >> origin.x >> origin.y >> origin.z;
    for(unsigned int i{0}; i != 8; ++i) {
      unsigned int value;
      input >> value;
      context.parameters[i] = static_cast<uint8_t>(value);
    }
    auto view{tfx::render::original_camera({0, 0, 0}, mat3<int32_t>{}, {160, 100}, wide ? tfx::render::aperture::missile_nose : tfx::render::aperture::normal)};
    if(viewport) input >> view.projection_origin.x >> view.projection_origin.y >> view.viewport_minimum.x >> view.viewport_minimum.y >> view.viewport_maximum.x >> view.viewport_maximum.y;
    std::string hex;
    input >> hex;
    tfx::models::shape const shape{decode_hex(hex)};
    context.camera.emplace(tfx::models::shape_camera{
      .basis{mat3<int32_t>{basis}},
      .origin{vec3<int32_t>{origin}},
      .projection_shift{tfx::render::projection_shift(view.opening)},
    });
    context.distance = distance;
    context.status = status;
    context.face_visibility = [&](std::span<tfx::models::model_vertex const> const vertices, tfx::models::facing_test const purpose){return tfx::render::model_facing(vertices, view, purpose);};
    auto const geometry{tfx::models::interpret(shape, context)};
    if(!geometry.warnings.empty()) throw std::runtime_error{"model frame emitted a runtime warning"};
    tfx::render::colour_map object;
    std::array<tfx::render::colour_map, tfx::render::colour_map_count> rows;
    for(unsigned int i{0}; i != object.size(); ++i) {
      object[i] = static_cast<uint8_t>(i * 5 + object_offset);
      for(unsigned int row{0}; row != rows.size(); ++row) rows[row][i] = static_cast<uint8_t>(i * 3 + fixed_offset + row * 17);
    }
    tfx::render::framebuffer output;
    output.pixels.fill(99);
    bool defined{true};
    auto random{static_cast<uint16_t>(seed)};
    for(auto const &face : geometry.primitives) {
      unsigned int const row{face.palette == tfx::models::palette_source::face_depth ? tfx::render::distance_colour_row(static_cast<int32_t>(face.palette_depth) * 65536) : 0u};
      auto const *const remap{face.palette == tfx::models::palette_source::direct ? nullptr : face.palette == tfx::models::palette_source::distance ? &object : &rows[row]};
      if(!tfx::render::paint_model_primitive(output, face, face.vertices, view, face.texture ? std::span<uint8_t const>{texture} : std::span<uint8_t const>{}, remap, random)) defined = false;
    }
    uint64_t hash{14695981039346656037ull};
    for(auto const value : output.pixels) hash = (hash ^ value) * 1099511628211;
    if(pixels) {
      std::cout.write(reinterpret_cast<char const *>(output.pixels.data()), output.pixels.size());
    } else if(fixture.is_open()) {
      uint64_t expected;
      unsigned int expected_seed;
      if(!defined || !(input >> expected >> expected_seed) || hash != expected || random != expected_seed) throw std::runtime_error{"native model-frame mismatch at " + std::to_string(cases)};
    } else {
      std::cout << hash << ' ' << random << ' ' << defined << '\n';
    }
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
