#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "render/integer_model_polygon.h"

std::vector<uint8_t> program(std::array<vec3<int16_t>, 3> const &vertices, uint16_t const scale, bool const shaded) {
  std::vector<uint8_t> source(50);
  auto const store{[&](unsigned int const at, uint16_t const value){
    source.at(at) = static_cast<uint8_t>(value >> 8);
    source.at(at + 1) = static_cast<uint8_t>(value);
  }};
  auto const emit{[&](std::initializer_list<uint16_t> const words){
    for(auto const value : words) {
      source.push_back(static_cast<uint8_t>(value >> 8));
      source.push_back(static_cast<uint8_t>(value));
    }
  }};
  store(2, 32767);
  store(18, 0);
  store(34, 14);
  store(48, 65535);
  emit({48, scale});
  for(unsigned int i{0}; i != vertices.size(); ++i) emit({98, static_cast<uint16_t>(i), static_cast<uint16_t>(vertices[i].x), static_cast<uint16_t>(vertices[i].y), static_cast<uint16_t>(vertices[i].z)});
  emit({97, 3, 0, 21, 0, 1, 2, 0});
  auto const field{static_cast<unsigned int>(source.size() - 2)};
  if(shaded) {
    emit({76, 0xc0cf, 0xc700, 77, 0, 1, 2, 76, 0xc0cf, 0xc700, 77, 0, 1, 2, 0});
  } else {
    emit({2, 17, 0, 1, 2, 2, 17, 0, 1, 2, 0});                                 // Two draws on the untaken branch.
  }
  store(field, static_cast<uint16_t>(source.size() - field));
  if(shaded) {
    emit({76, 0xc5d3, 0xcb00, 77, 0, 1, 2, 0, 65535});
  } else {
    emit({2, 19, 0, 1, 2, 0, 65535});                                          // One draw on the taken branch.
  }
  return source;
}

int main(int const argc, char const *const argv[]) try {
  bool pixels{false};
  bool shaded{false};
  bool native_camera{false};
  std::ifstream fixture;
  for(int i{1}; i != argc; ++i) {
    std::string_view const option{argv[i]};
    if(option == "--pixels") {
      pixels = true;
    } else if(option == "--shaded") {
      shaded = true;
    } else if(option == "--native-camera") {
      native_camera = true;
    } else {
      if(fixture.is_open()) throw std::runtime_error{"expected [FILE] [--pixels] [--shaded]"};
      fixture.open(argv[i]);
      if(!fixture) throw std::runtime_error{"cannot open model fixture"};
    }
  }
  auto &input{fixture.is_open() ? fixture : std::cin};
  bool wide;
  uint16_t scale;
  vec3<int16_t> offset;
  unsigned int cases{0};
  mat3<int32_t> const identity{32767, 0, 0, 0, 32767, 0, 0, 0, 32767};
  while(input >> wide >> scale >> offset.x >> offset.y >> offset.z) {
    std::array<vec3<int16_t>, 3> vertices;
    for(auto &point : vertices) input >> point.x >> point.y >> point.z;
    tfx::models::shape const shape{program(vertices, scale, shaded)};
    auto view{tfx::render::original_camera({0, 0, 0}, identity, {160, 100}, wide ? tfx::render::aperture::missile_nose : tfx::render::aperture::normal)};
    view.far_distance = 100000;
    auto const transform{[&](std::span<tfx::models::model_vertex const> const source){
      std::vector<tfx::models::model_vertex> result;
      for(auto const &point : source) {
        result.push_back({
          .position{native_camera ? vec3d{point.position} : view.transform(point.position + vec3d{offset})},
          .uv{0, 0},
          .shade{point.shade},
          .projection_scale{point.projection_scale},
        });
      }
      return result;
    }};
    tfx::models::shape_context context;
    if(native_camera) {
      context.camera.emplace(tfx::models::shape_camera{
        .basis{mat3<int32_t>{identity}},
        .origin{vec3<int32_t>{offset} * 32767},
        .projection_shift{tfx::render::projection_shift(view.opening)},
      });
    }
    context.face_visibility = [&](std::span<tfx::models::model_vertex const> const points, tfx::models::facing_test const purpose){return tfx::render::model_facing(transform(points), view, purpose);};
    auto const geometry{tfx::models::interpret(shape, context)};
    if(!geometry.warnings.empty()) throw std::runtime_error{"synthetic model emitted a warning"};
    tfx::render::framebuffer output;
    output.pixels.fill(99);
    for(auto const &face : geometry.primitives) {
      if(!tfx::render::integer_model_polygon(output, transform(face.vertices), view, static_cast<uint8_t>(face.colour))) throw std::runtime_error{"unhandled synthetic model face"};
    }
    uint64_t hash{14695981039346656037ull};
    for(auto const value : output.pixels) hash = (hash ^ value) * 1099511628211;
    if(pixels) {
      std::cout.write(reinterpret_cast<char const *>(output.pixels.data()), output.pixels.size());
    } else if(fixture.is_open()) {
      uint64_t draws;
      uint64_t expected;
      if(!(input >> draws >> expected) || draws != (geometry.projection_aborted ? 0u : (geometry.steps - 7) / (shaded ? 2 : 1)) || expected != hash) throw std::runtime_error{"native model program mismatch at case " + std::to_string(cases)};
    } else {
      std::cout << (geometry.projection_aborted ? 0u : (geometry.steps - 7) / (shaded ? 2 : 1)) << ' ' << hash << '\n';
    }
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
