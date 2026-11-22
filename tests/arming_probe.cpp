#include <cstdlib>
#include <fstream>
#include <iostream>
#include "resources/archive.h"
#include "resources/executable_tables.h"
#include "weapons/loadout.h"

int main(int const argc, char const *const argv[]) try {
  bool const checking{argc == 3 && std::string_view{argv[1]} == "--check"};
  if(argc != 2 && !checking) throw std::runtime_error{"arming_probe DATA_DIRECTORY | --synthetic | --check FIXTURE"};
  bool const synthetic{checking || std::string_view{argv[1]} == "--synthetic"};
  std::vector<tfx::weapons::definition> definitions(tfx::weapons::weapon_count);
  tfx::weapons::arming_tables tables;
  if(synthetic) {
    for(unsigned int index{0}; index != tables.next.size(); ++index) tables.next[index] = static_cast<uint16_t>((index + 1) % tables.next.size());
    for(unsigned int weapon{0}; weapon != definitions.size(); ++weapon) {
      for(unsigned int plane{0}; plane != 3; ++plane) definitions[weapon].mount_masks[plane] = static_cast<uint16_t>((weapon * 317u + plane * 193u + 0x155u) & 1023u);
    }
  } else {
    auto const path{tfx::resources::find_game_archive(argv[1])};
    tfx::resources::executable_tables const assets{path.parent_path() / "TFX.DAT"};
    definitions = assets.weapons;
    tables = assets.arming;
  }
  std::ifstream fixture;
  if(checking) {
    fixture.open(argv[2]);
    if(!fixture) throw std::runtime_error{"cannot open arming fixture"};
  }
  std::istream &input{checking ? fixture : std::cin};
  unsigned int aircraft{0};
  unsigned int occupied{0};
  unsigned int slot{0};
  unsigned int cases{0};
  int weapon{-1};
  while(input >> aircraft >> weapon >> occupied >> slot) {
    tfx::weapons::armament inventory{static_cast<tfx::flight::player_aircraft>(aircraft), definitions, tables};
    for(unsigned int index{0}; index != tfx::flight::store_slots; ++index) {
      if(occupied & (1u << index)) inventory.assign(index, tfx::weapons::weapon_id::sidewinder_s);
    }
    unsigned int allowed{0};
    if(weapon >= 0) {
      for(unsigned int mount{0}; mount != tfx::flight::store_slots; ++mount) {
        if(inventory.can_fit(mount, static_cast<tfx::weapons::weapon_id>(weapon))) allowed |= 1u << mount;
      }
    }
    inventory.assign(slot, weapon < 0 ? std::nullopt : std::optional{static_cast<tfx::weapons::weapon_id>(weapon)});
    std::array<uint32_t, 1 + tfx::flight::store_slots * 3> result{};
    result[0] = allowed;
    for(unsigned int index{0}; index != tfx::flight::store_slots; ++index) {
      auto const &pylon{inventory.pylons[index]};
      result[1 + index * 3] = pylon.weapon;
      result[2 + index * 3] = pylon.count;
      result[3 + index * 3] = pylon.visible;
    }
    if(checking) {
      uint64_t expected{0};
      if(!(input >> expected)) throw std::runtime_error{"incomplete arming fixture"};
      uint64_t digest{14695981039346656037ull};
      for(auto const value : result) {
        for(unsigned int byte{0}; byte != 4; ++byte) digest = (digest ^ ((value >> (byte * 8)) & 255u)) * 1099511628211ull;
      }
      if(digest != expected) throw std::runtime_error{"native arming fixture differs at case " + std::to_string(cases)};
    } else {
      for(auto const value : result) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(checking) std::cout << cases << " original arming permission/assignment fixtures passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
