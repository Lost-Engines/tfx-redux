#pragma once

#include <array>
#include <string>
#include <vector>
#include "math/attitude.h"
#include "render/model_queue.h"
#include "resources/binary.h"

namespace tfx::recording {

unsigned int constexpr header_bytes{10}, native_resource_limit{1000};
unsigned int constexpr compression_buffer_bytes{2048};
// Six compressed fields, each with a word length, plus the fixed/raw arrays.
unsigned int constexpr maximum_frame_bytes{16 + render::model_queue_capacity * 20 + 6 * (2 + compression_buffer_bytes)};
// Original queued constructors CC0BF/CC164/CC238 store these dispatch offsets.
// Shadow path 56 includes parameter and scale setup; 48 skips both steps.
enum class model_kind : uint8_t { unrotated = 40, rotated = 44, ground_shadow = 56 };

using order_entry = render::model_queue_entry;
struct model {
  model_kind kind{model_kind::unrotated};
  uint8_t initial_state{0};
  uint8_t colour_row{0};
  uint8_t scale{1};
  uint16_t resource{0};
  vec3<math::angle> angles{0,0,0};
  vec3<int32_t> camera_origin{0,0,0};
  std::array<uint8_t,8> parameters{};
};
struct frame {
  // C5BEC/C5FF0 copy these eight retained bytes verbatim. No producer has
  // been found in retail 2.05; retain the field without inventing semantics.
  std::array<uint8_t,8> reserved{};
  vec3<math::angle> camera_angles{0,0,0};
  std::vector<order_entry> order;
  std::vector<model> models;
};
struct resource {
  uint16_t handle{0};
  std::string name;
};
struct scene_frame {
  frame data;
  std::vector<std::string> model_names;
};
struct file_header {
  uint16_t frames{0};
  uint32_t size{0};
  uint32_t dictionary{0};
};
struct decoded_frame {
  frame value;
  unsigned int bytes{0};
};
struct file {
  std::vector<frame> frames;
  // Indices 0 and 1 have no stored names. Native playback supplies
  // badshape.3 for entry 1 and searches this table backwards by handle.
  std::vector<resource> resources{
    {
      .handle{0},
      .name{},
    },
    {
      .handle{1},
      .name{"badshape.3"},
    },
  };
};

resources::bytes encode_frame(frame const &source);
file_header decode_header(std::span<uint8_t const> source);
decoded_frame decode_frame(std::span<uint8_t const> source);
std::vector<resource> decode_resources(std::span<uint8_t const> source);
resources::bytes encode(file const &source);
file decode(std::span<uint8_t const> source);

} // namespace tfx::recording
