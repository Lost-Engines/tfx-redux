#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "resources/binary.h"
#include "world/instance.h"

namespace {

tfx::resources::bytes program(uint16_t const health, uint16_t const impact, bool const victim, uint16_t const opcode, uint16_t const lives) {
  tfx::resources::bytes result(16);
  result[0] = victim ? 2 : 0;
  result[8] = 3;
  auto const word{[&](unsigned int const at, uint16_t const value){
    result.at(at) = static_cast<uint8_t>(value);
    result.at(at + 1) = static_cast<uint8_t>(value >> 8);
  }};
  auto const entry{[&](unsigned int const index){
    unsigned int const at{10 + index * 2};
    word(at,static_cast<uint16_t>((result.size() - at) / 2));
  }};
  auto const emit{[&](std::initializer_list<uint16_t> const words){
    for(auto const value : words) { result.push_back(static_cast<uint8_t>(value)); result.push_back(static_cast<uint8_t>(value >> 8)); }
  }};
  entry(0);
  emit({17,impact,18,health});
  if(victim) emit({11,0,lives});
  emit({0});
  entry(1); emit({0});
  entry(2);
  if(victim) {
    emit({opcode,65526,65526,65526,10,10,10});
    if(opcode == 35) emit({0});
  }
  emit({0});
  return result;
}

} // anonymous namespace

auto main(int const argc, char const *const argv[])->int try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  std::istream &input{argc == 2 ? fixture : std::cin};
  if(!input) throw std::runtime_error{"cannot open contact fixture"};
  unsigned int health{0},impact{0},cost{0},lives{0},opcode{0},ticks{0},cases{0};
  uint64_t expected{0};
  std::array<std::string,0> const names{};
  while(input >> health >> impact >> cost >> lives >> opcode >> ticks) {
    tfx::world::definition const source_definition{program(static_cast<uint16_t>(health),static_cast<uint16_t>(impact),false,0,0),names};
    tfx::world::definition const victim_definition{program(99,static_cast<uint16_t>(cost),true,static_cast<uint16_t>(opcode),static_cast<uint16_t>(lives)),names};
    tfx::world::instance source{source_definition}, victim{victim_definition};
    uint64_t digest{14695981039346656037ULL};
    for(unsigned int tick{0}; tick != ticks; ++tick) {
      auto const result{victim.execute(2,{
        .movement{
          .previous{-20,0,0},
          .current{20,0,0},
        },
        .impact_damage{source.impact_damage},
        .collision_response{[&](uint16_t const damage){ source.apply_contact_damage(damage); }},
      })};
      for(uint32_t const value : {static_cast<uint32_t>(source.health),static_cast<uint32_t>(source.destroyed),static_cast<uint32_t>(victim.custom[0]),static_cast<uint32_t>(victim.destroyed),static_cast<uint32_t>(victim.modified),result.collisions}) {
        for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ ((value >> shift) & 255)) * 1099511628211ULL;
      }
    }
    if(argc == 2) {
      if(!(input >> expected) || digest != expected) throw std::runtime_error{"native contact response differs in case " + std::to_string(cases)};
    } else std::cout << digest << '\n';
    ++cases;
  }
  if(!input.eof() || !cases) throw std::runtime_error{"incomplete contact fixture"};
  if(argc == 2) std::cout << cases << " native two-way contact damage sequences passed" << std::endl;
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
