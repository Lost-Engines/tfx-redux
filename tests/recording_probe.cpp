#include <iomanip>
#include <iostream>
#include "recording/file.h"

int main() try {
  unsigned int files{0};
  if(!(std::cin >> files)) throw std::runtime_error{"missing recording case count"};
  for(unsigned int case_index{0}; case_index != files; ++case_index) {
    tfx::recording::file input;
    unsigned int frames{0};
    unsigned int resources{0};
    std::cin >> frames >> resources;
    input.frames.resize(frames);
    input.resources.resize(resources);
    auto const byte{[]{
      unsigned int value{0};
      std::cin >> value;
      return static_cast<uint8_t>(value);
    }};
    for(auto &frame : input.frames) {
      for(auto &value : frame.reserved) value = byte();
      for(unsigned int axis{0}; axis != 3; ++axis) std::cin >> frame.camera_angles[axis];
      unsigned int count{0};
      std::cin >> count;
      frame.order.resize(count);
      frame.models.resize(count);
      for(auto &entry : frame.order) std::cin >> entry.priority >> entry.slot;
      for(auto &model : frame.models) {
        model.kind = static_cast<tfx::recording::model_kind>(byte());
        model.initial_state = byte();
        model.colour_row = byte();
        model.scale = byte();
        std::cin >> model.resource;
        for(unsigned int axis{0}; axis != 3; ++axis) std::cin >> model.angles[axis];
        for(unsigned int axis{0}; axis != 3; ++axis) std::cin >> model.camera_origin[axis];
        for(auto &value : model.parameters) value = byte();
      }
    }
    for(auto &resource : input.resources) std::cin >> resource.handle >> std::quoted(resource.name);
    if(!std::cin) throw std::runtime_error{"incomplete recording input"};
    auto const bytes{tfx::recording::encode(input)};
    if(tfx::recording::encode(tfx::recording::decode(bytes)) != bytes) throw std::runtime_error{"recording round trip differs"};
    auto const size{static_cast<uint32_t>(bytes.size())};
    for(unsigned int index{0}; index != 4; ++index) std::cout.put(static_cast<char>(size >> (index * 8)));
    std::cout.write(reinterpret_cast<char const *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
