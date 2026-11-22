#include <array>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include "world/instance.h"

namespace {

void require(bool const condition, char const *const message) {
  if(!condition) throw std::runtime_error{message};
}

std::vector<uint8_t> program(bool const percentage = false) {
  /// An original test program, not copied game data: one part, eight entries and a custom health word.
  std::vector<uint8_t> result(26);
  result[0] = 2;
  result[1] = 1;
  result[5] = 1;
  result[8] = 8;
  auto const word{[&](unsigned int const at, uint16_t const value){
    result.at(at) = static_cast<uint8_t>(value);
    result.at(at + 1) = static_cast<uint8_t>(value >> 8);
  }};
  auto const entry{[&](unsigned int const index){
    unsigned int const field{10 + index * 2};
    word(field, static_cast<uint16_t>((result.size() - field) / 2));
  }};
  auto const emit{[&](std::initializer_list<uint16_t> const words){
    for(auto const value : words) {
      result.push_back(static_cast<uint8_t>(value));
      result.push_back(static_cast<uint8_t>(value >> 8));
    }
  }};
  entry(0);
  emit({3, 0, 0, 11, 0, 10, 20, 0, 6, 0, 1, 0x0203, 65534, 5, 0});
  entry(1);
  emit({7, 0, 0});
  entry(2);
  unsigned int const death_branch{static_cast<unsigned int>(result.size()) + (percentage ? 18u : 16u)};
  if(percentage) {
    emit({80, 65526, 65526, 65526, 10, 10, 10, 0, 10, 0, 0});
  } else {
    emit({64, 65526, 65526, 65526, 10, 10, 10, 0, 0, 0});
  }
  word(death_branch, static_cast<uint16_t>((result.size() - death_branch) / 2));
  entry(3);
  emit({16, 3, 0, 1, 27, 0, 4, 6, 4, 5, 0});
  entry(4);
  emit({11, 0, 22, 27, 0, 3, 0x0900, 5, 0});
  entry(5);
  emit({19, 3, 0, 0, 11, 0, 10, 0});
  entry(6);
  emit({73, 65516, 0});
  entry(7);
  emit({73, 0, 0});
  return result;
}

} // anonymous namespace

int main() try {
  std::array<std::string, 2> const names{"intact", "damaged"};
  tfx::world::definition const source{program(), names};
  tfx::world::instance first{source};
  tfx::world::instance second{source};
  require(first.damage_centres().size() == 1 && first.damage_centres()[0] == vec3<int16_t>{0, 0, 0}, "damage centres must enumerate the leading collision boxes");
  require(first.parts[0].name == "intact" && first.custom[0] == 10, "initial SSD state differs");
  for(unsigned int tick{0}; tick != 5; ++tick) first.animate();
  require(first.parts[0].parameters[0] == 2 && !first.stream_cursor(0, 0), "parameter stream endpoint differs");
  tfx::world::instance_context context{
    .movement{
      .previous{-20, 0, 0},
      .current{20, 0, 0},
    },
    .impact_damage{10},
  };
  auto contact{first.execute(2, context)};
  require(contact.collisions == 1 && first.custom[0] == 0 && !first.destroyed, "zero custom health must not pass a strict-negative damage branch");
  context.impact_damage = 1;
  contact = first.execute(2, context);
  require(contact.collisions == 1 && first.custom[0] == 65535 && first.destroyed && first.parts[0].name == "damaged", "damage helper did not replace model and animation");
  first.animate();
  require(first.parts[0].parameters[7] == 9 && first.custom[0] == 22, "animation event must resume using its replacement stream in the same tick");
  require(second.parts[0].name == "intact" && second.custom[0] == 10 && !second.destroyed, "shared immutable definition must not share mutable damage state");
  first.execute(5);
  require(!first.destroyed && first.parts[0].name == "intact" && first.custom[0] == 10, "restoration event differs");
  require(first.execute(999).draws.empty(), "missing event should be ignored as in the original dispatcher");
  context.player_query = true;
  auto const elevated_surface{first.execute(6, context)};
  require(elevated_surface.surface_height == -20 && !elevated_surface.surface_present, "ground height writes must survive without a surface contact");
  require(first.execute(7, context).surface_height == 0, "a written zero ground height must differ from no write");
  context.player_query = false;
  require(!first.execute(6, context).surface_height && !first.execute(999, context).surface_height, "other-object and missing queries must retain the caller's ground height");
  tfx::world::definition const reporting_source{program(true), names};
  tfx::world::instance reporting{reporting_source};
  unsigned int notifications{0};
  context.impact_damage = 10;
  context.damage_response = [&](int const percentage){
    require(percentage == 100, "damage callback did not see the current cumulative percentage");
    ++notifications;
    reporting.execute(5);
  };
  reporting.execute(2, context);
  require(notifications == 1 && !reporting.destroyed && reporting.custom[0] == 10, "damage callbacks must finish before the VM tests its fatal branch");
  std::cout << "Persistent SSD damage, animation events, stream replacement and restoration passed" << std::endl;
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
