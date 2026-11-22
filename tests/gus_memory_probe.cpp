#include <cstdlib>
#include <fstream>
#include <iostream>
#include "audio/gus_memory.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int ram_kib;
  unsigned int count;
  unsigned int cases{0};
  while(input >> ram_kib >> count) {
    std::vector<uint32_t> sizes(count);
    for(auto &size : sizes) input >> size;
    if(!input) throw std::runtime_error{"truncated Gravis sample-size input"};
    auto const result{tfx::audio::plan_gus_residency(sizes, ram_kib)};
    std::vector<int64_t> values;
    for(auto const address : result.addresses) values.push_back(address ? static_cast<int64_t>(*address) : -1);
    values.push_back(result.remaining);
    // Independently exercise command 5 on the unsorted caller requests too.
    tfx::audio::gus_sample_ram ram{ram_kib};
    for(auto const size : sizes) {
      auto const address{ram.allocate(size)};
      values.push_back(address ? static_cast<int64_t>(*address) : -1);
    }
    values.push_back(ram.remaining());
    for(auto const value : values) {
      if(argc == 2) {
        int64_t expected;
        if(!(input >> expected) || value != expected) throw std::runtime_error{"Gravis RAM mismatch at case " + std::to_string(cases)};
      } else {
        std::cout << value << ' ';
      }
    }
    if(argc != 2) std::cout << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
