#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "actors/contact_region.h"
#include "actors/scenery.h"

namespace {
tfx::resources::bytes unhex(std::string const &text) {
  if(text.size() % 2) throw std::runtime_error{"odd hexadecimal input"};
  tfx::resources::bytes bytes;
  for(unsigned int index{0}; index < text.size(); index += 2) bytes.push_back(static_cast<uint8_t>(std::stoul(text.substr(index, 2), nullptr, 16)));
  return bytes;
}
} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  bool const check{argc == 2};
  std::ifstream fixture;
  if(check) fixture.open(argv[1]);
  std::istream &input{check ? fixture : std::cin};
  if(!input) throw std::runtime_error{"cannot read world-contact fixture"};
  char command{0};
  while(input >> command) {
    std::vector<int64_t> values;
    if(command == 'R') {
      vec3<int32_t> previous;
      vec3<int32_t> current;
      vec3<int32_t> position;
      input >> previous.x >> previous.y >> previous.z >> current.x >> current.y >> current.z >> position.x >> position.y >> position.z;
      auto const region{tfx::actors::nearby_surface_region(previous, current)};
      values = {region.centre.x, region.centre.y, region.centre.z, region.extent.x, region.extent.y, region.extent.z, region.contains(position)};
    } else if(command == 'P') {
      std::string profile_hex;
      std::string program_hex;
      input >> profile_hex >> program_hex;
      auto const profiles{tfx::actors::profiles(unhex(profile_hex))};
      tfx::actors::program const program{unhex(program_hex)};
      for(auto const &placement : tfx::actors::initialise_scenery(program, profiles)) {
        values.insert(values.end(), {placement.record, placement.profile_id, placement.position.x, placement.position.y, placement.position.z, placement.event, static_cast<int64_t>(placement.definition.size())});
        for(unsigned char const character : placement.definition) values.push_back(character);
      }
    } else {
      throw std::runtime_error{"unknown world-contact operation"};
    }
    uint64_t digest{14695981039346656037ULL};
    for(auto const value : values) {
      for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ ((static_cast<uint32_t>(value) >> shift) & 255)) * 1099511628211ULL;
    }
    if(check) {
      uint64_t expected{0};
      if(!(input >> expected) || expected != digest) throw std::runtime_error{"native fixed placement or nearby region differs"};
    } else {
      for(auto const val : values) std::cout << val << ' ';
      std::cout << '\n';
    }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
