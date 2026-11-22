#include <iostream>
#include "recording/file.h"

namespace {
void require(bool const condition) {
  if(!condition) throw std::runtime_error{"recording assertion failed"};
}
template<typename Action> void rejects(Action const &action) {
  try {
    action();
  } catch(std::runtime_error const &) {
    return;
  }
  throw std::runtime_error{"invalid recording was accepted"};
}
}

int main() try {
  using tfx::recording::decode;
  using tfx::recording::encode;
  using tfx::recording::file;
  using tfx::recording::header_bytes;
  using tfx::recording::model_kind;
  file recording;
  tfx::resources::bytes const empty{0, 0, 20, 0, 0, 0, 10, 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 0, 0};
  require(encode(recording) == empty);
  require(decode(empty).frames.empty());
  recording.frames.resize(1);
  auto &frame{recording.frames.front()};
  // Cross the 255-byte fill/literal boundaries in all compressed arrays.
  frame.models.resize(tfx::render::model_queue_capacity);
  for(unsigned int index{0}; index != frame.models.size(); ++index) {
    frame.order.push_back({
      .priority{static_cast<int16_t>(index - 150)},
      .slot{static_cast<uint16_t>(index)},
    });
    auto &model{frame.models[index]};
    model.resource = static_cast<uint16_t>(index + 2);
    model.kind = index % 2 ? model_kind::rotated : model_kind::ground_shadow;
    model.angles = {static_cast<uint16_t>(index * 137), 0, 65535};
    model.camera_origin = {-123456789, static_cast<int32_t>(index * 65537), 2147483647};
    if(index % 7 == 0) model.parameters[index % 8] = static_cast<uint8_t>(index);
    if(index >= 255) model.scale = 255;
  }
  auto const bytes{encode(recording)};
  require(encode(decode(bytes)) == bytes);
  for(size_t length{0}; length != bytes.size(); ++length) rejects([&]{decode(std::span{bytes}.first(length));});
  auto damaged{bytes};
  damaged[header_bytes + 14] = 255;
  damaged[header_bytes + 15] = 255;
  rejects([&]{decode(damaged);});
  frame.order[0].slot = tfx::render::model_queue_capacity;
  rejects([&]{encode(recording);});
  frame.order[0].slot = 0;
  for(auto &model : frame.models) model.parameters.fill(42);
  rejects([&]{encode(recording);});                                            // Native C5958's 2 KiB temporary buffer.
  for(auto &model : frame.models) model.parameters.fill(0);
  recording.resources[1].name = "badshape.3";
  recording.resources.push_back({
    .handle{12},
    .name{std::string{"bad\0name", 8}},
  });
  rejects([&]{encode(recording);});
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
