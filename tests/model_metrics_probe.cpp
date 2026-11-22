#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string_view>
#include "models/shape.h"
#include "render/model_metrics.h"
#include "resources/archive.h"

int main(int const argc, char const *const argv[]) try {
  bool const corpus{argc == 3 && std::string_view{argv[1]} == "--corpus"};
  std::optional<tfx::resources::archive> game;
  if(corpus) game.emplace(tfx::resources::find_game_archive(argv[2]));
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if((argc != 1 && !corpus && (argc != 2 || !fixture))) throw std::runtime_error{"expected [FILE] or --corpus DATA_DIRECTORY"};
  auto &input{fixture.is_open() ? fixture : std::cin};
  std::map<uint32_t, tfx::models::shape> models;
  unsigned int cases{0};
  uint32_t id{0};
  vec3<int32_t> point;
  while(corpus ? static_cast<bool>(input >> id >> point.x >> point.y >> point.z) : static_cast<bool>(input >> point.x >> point.y >> point.z)) {
    std::optional<tfx::models::shape> synthetic;
    if(!corpus) {
      int16_t bias;
      int16_t minimum;
      unsigned int count;
      input >> bias >> minimum >> count;
      std::vector<uint8_t> bytes(48);
      auto const store{[&](unsigned int const at, uint16_t const value){
        bytes.at(at) = static_cast<uint8_t>(value >> 8);
        bytes.at(at + 1) = static_cast<uint8_t>(value);
      }};
      auto const emit{[&](uint16_t const value){
        bytes.push_back(static_cast<uint8_t>(value >> 8));
        bytes.push_back(static_cast<uint8_t>(value));
      }};
      store(0, static_cast<uint16_t>(minimum));
      store(16, static_cast<uint16_t>(bias));
      for(unsigned int i{0}; i != count; ++i) {
        unsigned int distance;
        unsigned int kind;
        unsigned int colour;
        input >> distance >> kind >> colour;
        unsigned int const field{34 + i * 2};
        store(2 + i * 2, static_cast<uint16_t>(distance));
        store(18 + i * 2, static_cast<uint16_t>(kind));
        store(field, static_cast<uint16_t>(bytes.size() - field));
        if(kind <= 1) {
          emit(65535);
          emit(0);
          emit(65535);
        } else {
          emit(static_cast<uint16_t>(colour));
        }
      }
      synthetic.emplace(bytes);
    } else if(!models.contains(id)) models.emplace(id, game->read(id));
    auto const &shape{corpus ? models.at(id) : *synthetic};
    auto const metrics{tfx::render::measure_model(point, shape.sort_depth_bias)};
    auto const *detail{shape.detail_for_distance(metrics.distance)};
    int32_t const values[]{metrics.distance, metrics.order, metrics.colour_row, detail ? static_cast<int32_t>(detail - shape.details.data()) : -1, detail ? static_cast<int32_t>(detail->kind) : -1, detail ? static_cast<int32_t>(detail->offset) : -1, detail ? detail->colour : 0};
    if(fixture.is_open()) {
      for(auto const value : values) {
        int32_t expected;
        if(!(input >> expected) || value != expected) throw std::runtime_error{"native model metric/detail mismatch at case " + std::to_string(cases)};
      }
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
