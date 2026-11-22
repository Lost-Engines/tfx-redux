#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "missions/rules.h"

namespace {

void require(bool const value, char const *const message) {
  if(!value) throw std::runtime_error{message};
}
tfx::missions::variables campaign() {
  return {{"a2akills", 0}, {"a2gkills", 0}, {"num_miss", 0}, {"num_hours", 0}, {"miss_val", 0}, {"score", 1000}};
}

} // anonymous namespace

auto main()->int try {
  namespace mission = tfx::missions;
  mission::finish_program const hop1{
    "FINISH:\r\nsetpar score += pairscore\r\n"
    "if eplanes_down > 0 setpar score += 5000\r\n"
    "if eplanes_down == 1 return 1\r\nreturn 0\r\nEND:\r\n"
  };
  mission::result_state result{
    .personal_air_score{100},
    .enemy_aircraft_down{1},
    .credited_air_kills{1},
  };
  auto values{campaign()};
  hop1.execute(result, values);
  require(values.at("score") == 6200 && values.at("miss_val") == 1, "Hop 1 score/result differs from native FINISH");
  require(values.at("a2akills") == 1 && values.at("num_miss") == 1 && values.at("num_hours") == 1, "native common statistics differ");
  result.enemy_aircraft_down = 65537;
  values = campaign();
  hop1.execute(result, values);
  require(values.at("miss_val") == 1, "native numeric predicates compare signed low words");
  mission::finish_program const logic{
    "FINISH:\r\nif dead(targ#1) or dead(targ#2) and dead(targ#3) return 1\r\nreturn 0\r\nEND:\r\n"
  };
  result.targets = {mission::target_life::dead, mission::target_life::alive, mission::target_life::alive};
  values = campaign();
  logic.execute(result, values);
  require(values.at("miss_val") == 0, "native AND/OR terms must evaluate from left to right");
  mission::finish_program const nested{
    "FINISH:\r\nif completed == 1 {\r\nif eplanes_down == 10 {\r\nreturn 2\r\n}\r\n"
    "if eplanes_down > 0 {\r\nreturn 1\r\n}\r\n}\r\nreturn 0\r\nEND:\r\n"
  };
  result.completed = 1;
  result.enemy_aircraft_down = 1;
  values = campaign();
  nested.execute(result, values);
  require(values.at("miss_val") == 0, "native failed nested IF skips the next braced sibling");
  mission::finish_program const assignment{"FINISH:\r\nsetpar score == 2147483647\r\nsetpar score += 1\r\nreturn 0\r\nsetpar score == 0\r\nEND:\r\n"};
  values = campaign();
  assignment.execute(result, values);
  require(values.at("score") == -2147483647 - 1, "assignment must wrap as a native dword and stop at RETURN");
  mission::finish_program const no_short_circuit{"FINISH:\r\nif 0 == 1 and dead(targ#1) return 1\r\nreturn 0\r\nEND:\r\n"};
  result.targets.clear();
  bool rejected{false};
  try {
    no_short_circuit.execute(result, values);
  } catch(std::out_of_range const &) {
    rejected = true;
  }
  require(rejected, "native predicates are evaluated even when an earlier AND term is false");
  rejected = false;
  try {
    mission::finish_program invalid{"FINISH:\r\nif score >= 1 return 1\r\nEND:\r\n"};
  } catch(std::runtime_error const &) {
    rejected = true;
  }
  require(rejected, "unsupported rule comparisons must be explicit failures");
  std::cout << "Native mission scoring, comparison width, boolean order and return checks passed" << std::endl;
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
