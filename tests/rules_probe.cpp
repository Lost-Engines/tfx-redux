#include <cstdlib>
#include <iostream>
#include <map>
#include <stdexcept>
#include "math/native.h"
#include "missions/rules.h"
#include "resources/archive.h"

auto main(int const argc, char const *const argv[])->int try {
  if(argc != 2) throw std::runtime_error{"rules probe requires the original installation directory"};
  tfx::resources::archive const game{tfx::resources::find_game_archive(argv[1])};
  std::map<std::string, tfx::missions::finish_program> programs;
  std::string name;
  while(std::cin >> name) {
    tfx::missions::result_state result;
    int targets{0};
    unsigned int alive_mask{0};
    std::cin >> result.personal_air_score >> result.enemy_air_score >> result.friendly_air_score >> result.enemy_aircraft_down >> result.friendly_aircraft_down >> result.completed >> result.targets_destroyed >> result.enemy_mobile_down >> result.credited_air_kills >> result.credited_ground_kills >> targets >> alive_mask;
    result.credited_targets = tfx::math::signed_word(static_cast<uint16_t>(targets));
    for(unsigned int i{0}; i != 9; ++i) result.targets.push_back((alive_mask & (1u << i)) ? tfx::missions::target_life::alive : tfx::missions::target_life::dead);
    tfx::missions::variables campaign;
    for(auto const key : {"a2akills", "a2gkills", "num_miss", "num_hours", "miss_val", "score", "fmobile_down"}) {
      int32_t value{0};
      if(!(std::cin >> value)) throw std::runtime_error{"truncated mission result replay"};
      campaign.emplace(key, value);
    }
    if(!programs.contains(name)) {
      auto const source{game.read("missions/" + name + ".mis")};
      programs.try_emplace(name, std::string_view{reinterpret_cast<char const *>(source.data()), source.size()});
    }
    programs.at(name).execute(result, campaign);
    std::cout << '[';
    bool first{true};
    for(auto const key : {"a2akills", "a2gkills", "num_miss", "num_hours", "miss_val", "score", "fmobile_down"}) {
      if(!first) std::cout << ',';
      first = false;
      std::cout << campaign.at(key);
    }
    std::cout << "]\n";
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
