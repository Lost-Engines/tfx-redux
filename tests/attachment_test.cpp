#include <array>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include "models/shape.h"

namespace {

void require(bool const value, char const *const message) {
  if(!value) throw std::runtime_error{message};
}

tfx::models::shape program(std::initializer_list<uint16_t> const words) {
  // An independent, synthetic disk program with one detail entry.
  std::vector<uint8_t> bytes(48);
  auto const word{[&](unsigned int const at, uint16_t const value){
    bytes.at(at) = static_cast<uint8_t>(value >> 8);
    bytes.at(at + 1) = static_cast<uint8_t>(value);
  }};
  word(2, 32767);
  word(34, 14);
  auto const emit{[&](uint16_t const value){
    bytes.push_back(static_cast<uint8_t>(value >> 8));
    bytes.push_back(static_cast<uint8_t>(value));
  }};
  emit(65535);
  for(auto const value : words) emit(value);
  emit(65535);
  return tfx::models::shape{bytes};
}

} // anonymous namespace

int main() try {
  for(unsigned int slot{0}; slot != 10; ++slot) {
    auto const parent{program({48, 80, 98, 0, 800, 1600, 2400, 97, 1, 0, 81, 0, static_cast<uint16_t>(slot * 2), 33, 17, 4, 0, 1, 99, 0, 215, 98, 1, 80, 160, 240, 97, 1, 1, 1, 100, 0, 1, 0})};
    auto const child{program({33, 249, 4, 0, 98, 215, 80, 160, 240, 97, 1, 215, 51, 77, 215, 72, 7, 0})};
    std::array<tfx::models::shape const *, 10> attachments{};
    attachments[slot] = &child;
    tfx::models::shape_context context{
      .initial_state{17},
      .facing{false},
      .attachments{attachments},
    };
    context.parameters[7] = 3;
    auto geometry{tfx::models::interpret(parent, context)};
    require(geometry.primitives.size() == 3 && geometry.warnings.empty(), "attachment condition or parent restoration differs");
    auto const &point{geometry.primitives[0]};
    require(point.vertices[0].position == vec3d{11, 22, 33} && point.attachment_slot == slot, "attachment must inherit its anchor, scale and slot");
    require(point.vertices[0].projection_scale == .025, "carried model must retain its inherited native camera units");
    require(!geometry.primitives[1].attachment_slot && geometry.primitives[1].vertices[1].position == point.vertices[0].position, "native shared vertex storage differs");
    require(geometry.primitives[2].vertices[1].position == vec3d{1, 2, 3}, "attachment translation leaked into parent");
    attachments[slot] = nullptr;
    geometry = tfx::models::interpret(parent, context);
    require(geometry.primitives.size() == 1 && !geometry.primitives[0].attachment_slot, "empty attachment must be skipped");
    auto const scaled{program({48, 4, 98, 215, 80, 160, 240, 97, 1, 215, 51, 77, 215, 0})};
    attachments[slot] = &scaled;
    geometry = tfx::models::interpret(parent, context);
    require(geometry.primitives[0].vertices[0].position == vec3d{20, 40, 60}, "absolute child scale must retain the original actor origin");
    require(geometry.primitives[0].vertices[0].projection_scale == .5, "absolute carried-model scaling must replace its projection units");
  }
  auto const recursive{program({98, 0, 0, 0, 0, 97, 1, 0, 81, 0, 0, 0})};
  std::array<tfx::models::shape const *, 1> const cycle{&recursive};
  bool rejected{false};
  try {
    tfx::models::interpret(recursive, tfx::models::shape_context{.attachments{cycle}});
  }
  catch(std::runtime_error const &) {
    rejected = true;
  }
  require(rejected, "recursive attachments must share the interpreter depth limit");
  std::cout << "Carried-model anchors, inherited scale, forced condition, shared vertices and restoration passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
