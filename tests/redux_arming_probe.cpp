#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include "presentation/arming_screen.h"

namespace {
void require(bool const value, char const *message) { if(!value) throw std::runtime_error{message}; }
}

int main(int const argc, char const *const argv[]) try {
  if(argc != 3) throw std::runtime_error{"usage: redux_arming_probe DATA_DIRECTORY OUTPUT_DIRECTORY"};
  std::filesystem::path const data{argv[1]}, output{argv[2]};
  std::filesystem::create_directories(output);
  tfx::resources::archive const game{tfx::resources::find_game_archive(data)};
  tfx::resources::executable_tables const tables{data / "TFX.DAT"};
  for(unsigned int craft{1}; craft <= 3; ++craft) {
    auto const type{static_cast<tfx::flight::player_aircraft>(craft)};
    tfx::weapons::armament inventory{type,tables.weapons,tables.arming};
    tfx::presentation::arming_preview preview{game,tables,type};preview.start();
    tfx::presentation::arming_state state;
    std::set<unsigned int> mounts;
    for(int y{12}; y < 180; y += 2) for(int x{12}; x < 200; x += 2) {
      vec2<int16_t> const position{static_cast<int16_t>(x),static_cast<int16_t>(y)};
      if(auto const picked{preview.pick_cursor(position,state)}) mounts.insert(*picked);
    }
    require(mounts.size() >= 4,"camera-ray picking did not reach the aircraft pylons");
    // Rotate the same craft and repeat: mouse coordinates retain their screen
    // meaning instead of using a fixed X/Z map tied to one aircraft pose.
    state.angles.y = static_cast<tfx::math::angle>(state.angles.y + 0x4000);
    std::set<unsigned int> rotated;
    for(int y{12}; y < 180; y += 2) for(int x{12}; x < 200; x += 2) {
      if(auto const picked{preview.pick_cursor({static_cast<int16_t>(x),static_cast<int16_t>(y)},state)}) rotated.insert(*picked);
    }
    require(rotated.size() >= 4,"rotating the aircraft broke screen-space picking");
    tfx::presentation::arming_controller placement{inventory,tables.arming_art(),0};
    bool placed{false};
    for(unsigned int weapon{1}; weapon < tables.weapons.size() && !placed; ++weapon) {
      for(int y{12}; y < 180 && !placed; ++y) for(int x{12}; x < 200 && !placed; ++x) {
        vec2<int16_t> const position{static_cast<int16_t>(x),static_cast<int16_t>(y)};
        auto const mount{preview.pick_cursor(position,placement.state())};
        if(!mount || !inventory.can_fit(*mount,static_cast<tfx::weapons::weapon_id>(weapon))) continue;
        placement.choose(static_cast<tfx::weapons::weapon_id>(weapon));
        placement.advance(position,{0,0},1,mount);
        require(inventory.pylons[*mount].weapon == weapon && inventory.pylons[*mount].count,"cursor-ray placement did not attach the selected weapon");
        std::ofstream controls{output / ("controls-" + std::to_string(craft) + ".json")};
        controls << "{\"mount\":[" << x << ',' << y << "]";
        auto const first_store{weapon > 6 ? weapon - 5 : 1};
        auto const &row{tables.arming_art().rows[weapon - first_store]};
        controls << ",\"first_store\":" << first_store;
        controls << ",\"weapon\":[" << (row.minimum.x + row.maximum.x) / 2 << ',' << (row.minimum.y + row.maximum.y) / 2 << "]";
        for(auto const &region : tables.arming_art().regions) {
          if(region.action == tfx::presentation::arming_action::done || region.action == tfx::presentation::arming_action::defaults) controls << ",\"" << (region.action == tfx::presentation::arming_action::done ? "done" : "defaults") << "\":[" << (region.minimum.x + region.maximum.x) / 2 << ',' << (region.minimum.y + region.maximum.y) / 2 << ']';
        }
        controls << "}\n";
        placed = true;
      }
    }
    require(placed,"no visible compatible pylon could be armed");
    placement.cancel();
    tfx::presentation::arming_screen screen{game,tables,inventory,type,0,tfx::presentation::arming_entry::preflight,{1200,750}};
    for(unsigned int frame{0}; frame != 45; ++frame) screen.advance({100,75},{0,0},0);
    screen.image().screenshot(output / ("arming-" + std::to_string(craft) + ".ppm"),screen.colours());
    screen.press(1);
    for(unsigned int frame{0}; frame != 45; ++frame) screen.advance({100,75},{0,0},0);
    require(screen.cancelled(),"preflight Escape entered the mission");
    tfx::presentation::arming_controller rearm{inventory,tables.arming_art(),0,tfx::presentation::arming_entry::rearming};
    rearm.press(1);require(rearm.state().mode == tfx::presentation::arming_mode::confirm_return,"rearming Escape skipped confirmation");
    rearm.press(0x31);require(rearm.state().mode == tfx::presentation::arming_mode::idle,"No did not return to rearming");
    rearm.press(1);rearm.press(0x15);require(rearm.state().mode == tfx::presentation::arming_mode::cancelled,"Yes did not leave rearming");
    std::cout << "Aircraft " << craft << ": " << mounts.size() << " pylons before rotation, " << rotated.size() << " after; Escape checks passed\n";
  }
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
