#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "render/model_queue.h"
#include "render/model_transform.h"

struct payload {
  tfx::render::model_render_path path{};
  unsigned int id{0};
  uint8_t scale{1};
  vec3<int32_t> cache;
  vec3<tfx::math::angle> angles;
  std::array<uint32_t, 2> parameters{};
};

int main(int const argc, char const *const argv[]) try {
  bool const full{argc == 2 && std::string_view{argv[1]} == "--full"};
  std::ifstream fixture;
  if(argc == 2 && !full) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !full && !fixture)) throw std::runtime_error{"expected [FILE] or --full"};
  auto &input{fixture.is_open() ? fixture : std::cin};
  unsigned int count;
  unsigned int cases{0};
  while(input >> count) {
    tfx::render::model_queue queue;
    tfx::render::model_slot_metadata metadata;
    std::array<payload, tfx::render::model_queue_capacity> records;
    std::vector<uint32_t> values;
    std::vector<uint32_t> events;
    std::array<uint32_t, 2> parameters{};
    auto const paint{[&](payload &item, tfx::render::model_render_path const path, bool const immediate){
      if(!immediate) parameters = item.parameters;
      item.cache = tfx::render::scale_model_cache(item.cache, item.scale);
      unsigned int const tag{path == tfx::render::model_render_path::ground_shadow ? 2u : path == tfx::render::model_render_path::unrotated || (immediate && !item.angles.x && !item.angles.y && !item.angles.z) ? 0u : 1u};
      events.insert(events.end(), {item.id, tag, static_cast<uint32_t>(item.cache.x), static_cast<uint32_t>(item.cache.y), static_cast<uint32_t>(item.cache.z), item.parameters[0], item.parameters[1], parameters[0], parameters[1], tag ? item.angles.x : 0u, tag ? item.angles.y : 0u, tag ? item.angles.z : 0u});
    }};
    for(unsigned int i{0}; i != count; ++i) {
      unsigned int kind;
      unsigned int scale;
      unsigned int rotated;
      int16_t priority;
      input >> kind >> priority >> scale >> rotated;
      if(kind == 4) {
        for(auto const slot : queue.draw_order()) paint(records[slot], records[slot].path, false);
        queue.clear();
        values.insert(values.end(), {65535, 0});
        continue;
      }
      auto const path{kind == 0 ? tfx::render::model_render_path::unrotated : kind == 2 ? tfx::render::model_render_path::ground_shadow : tfx::render::model_render_path::scaled};
      auto const slot{kind == 3 ? queue.scratch_slot() : queue.insert(priority)};
      auto const state{kind == 3 ? metadata.immediate(slot, static_cast<uint8_t>(scale)) : metadata.queued(slot, path, static_cast<uint8_t>(scale))};
      payload item{
        .path{state.path},
        .id{i + 1},
        .scale{state.scale},
        .cache{std::bit_cast<int32_t>(i * 65537u), std::bit_cast<int32_t>(0u - i * 63591u), std::bit_cast<int32_t>((static_cast<uint32_t>(static_cast<uint16_t>(priority)) << 16) | ((i * 313u) & 65535u))},
        .angles{rotated ? static_cast<tfx::math::angle>(i * 37 + 1) : uint16_t{0}, rotated ? static_cast<tfx::math::angle>(i * 83 + 1) : uint16_t{0}, rotated ? static_cast<tfx::math::angle>(i * 159 + 1) : uint16_t{0}},
      };
      for(unsigned int byte{0}; byte != 8; ++byte) item.parameters[byte / 4] |= ((i * 11 + byte) & 255u) << ((byte % 4) * 8);
      records[slot] = std::move(item);
      values.push_back(slot);
      values.push_back(queue.size());
      if(kind == 3) paint(records[slot], path, true);
    }
    for(auto const slot : queue.draw_order()) paint(records[slot], records[slot].path, false);
    values.push_back(static_cast<uint32_t>(events.size() / 12));
    values.insert(values.end(), events.begin(), events.end());
    uint64_t hash{14695981039346656037ull};
    for(auto const value : values) {
      for(unsigned int byte{0}; byte != 4; ++byte) hash = (hash ^ static_cast<uint8_t>(value >> (byte * 8))) * 1099511628211;
    }
    if(full) {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    } else if(fixture.is_open()) {
      uint64_t expected;
      if(!(input >> expected) || hash != expected) throw std::runtime_error{"native mixed model delivery mismatch at " + std::to_string(cases)};
    } else {
      std::cout << hash << '\n';
    }
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
