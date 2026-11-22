#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "render/model_queue.h"

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
    std::array<unsigned int, tfx::render::model_queue_capacity> payload{};
    std::vector<unsigned int> values;
    for(unsigned int i{0}; i != count; ++i) {
      int16_t priority;
      input >> priority;
      auto const slot{queue.insert(priority)};
      values.push_back(slot);
      payload.at(slot) = i;
    }
    auto const order{queue.draw_order()};
    values.push_back(static_cast<unsigned int>(order.size()));
    for(auto const slot : order) values.push_back(payload.at(slot));
    uint64_t hash{14695981039346656037ull};
    for(auto const value : values) {
      for(unsigned int byte{0}; byte != 4; ++byte) hash = (hash ^ static_cast<uint8_t>(value >> (byte * 8))) * 1099511628211;
    }
    if(full) {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    } else if(fixture.is_open()) {
      uint64_t expected;
      if(!(input >> expected) || hash != expected) throw std::runtime_error{"native model queue mismatch at case " + std::to_string(cases)};
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
