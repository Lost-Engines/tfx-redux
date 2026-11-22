#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "presentation/frontend.h"

namespace {
void require(bool const condition, char const *const message) {
  if(!condition) throw std::runtime_error{message};
}
struct scratch_directory {
  std::filesystem::path path{std::filesystem::temp_directory_path() / ("tfx-pilot-check-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
  scratch_directory() {
    if(!std::filesystem::create_directory(path)) throw std::runtime_error{"cannot create private pilot-test directory"};
  }
  ~scratch_directory() {
    std::error_code ignored;
    std::filesystem::remove_all(path, ignored);
  }
};
} // anonymous namespace
int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"pilot_probe DATA_DIRECTORY"};
  auto const archive{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::archive const game{archive};
  tfx::resources::executable_tables const tables{archive.parent_path() / "TFX.DAT"};
  tfx::presentation::program const schema{game.read("tfx.par")};
  scratch_directory const scratch;
  tfx::presentation::pilot_store store{scratch.path};
  tfx::presentation::frontend first{game, tables, scratch.path, "english", true};
  tfx::presentation::scene_input input;
  uint32_t ticks{0};
  auto const step{[&](tfx::presentation::frontend &frontend, unsigned int const count){
    for(unsigned int index{0}; index != count; ++index) {
      auto const *action{frontend.advance({ticks++, ticks * 3}, input)};
      require(!action, "front-end unexpectedly reached an external host action");
    }
  }};
  step(first, 50);
  first.press(0x1c, false);                                                    // Create, then accept each original default name.
  first.press(0x1c, false);
  first.press(1, false);
  first.press(0x1c, true);
  require(std::get<std::string>(first.values().at("firstname")) == "DIRK", "first-name default differs");
  require(std::get<std::string>(first.values().at("pilotname")) == "LOOSE CANNON", "surname Escape default differs");
  require(std::get<std::string>(first.values().at("callsign")) == "INFERNO", "callsign default differs");
  require(std::get<uint32_t>(first.values().at("rank")) == 1, "Control qualification differs");
  first.press(0x1c, false);                                                    // Empty slot zero.
  step(first, 100);
  auto saved{store.read(0, false, schema)};
  require(saved.resume && saved.resume->function == 0 && saved.resume->statement == 5, "enrolment saved the wrong native resume statement");
  require(saved.values == first.values(), "new pilot did not persist its complete state");

  tfx::presentation::frontend loaded{game, tables, scratch.path, "english", true};
  step(loaded, 50);
  loaded.press(0x50, false);
  loaded.press(0x1c, false);
  loaded.press(0x1c, false);
  step(loaded, 100);
  require(loaded.values() == first.values(), "loading selected pilot did not restore its complete campaign variables");
  // The archive's authored FWS menu choice leads through its briefings to mission 501.
  input.mouse_buttons = 1;
  loaded.advance({ticks++, ticks * 3}, input, vec2<int16_t>{120, 76});
  input.mouse_buttons = 0;
  tfx::presentation::statement const *launch{nullptr};
  for(unsigned int index{0}; index != 20000 && !launch; ++index) {
    input.key = tfx::presentation::scene_key::escape;
    launch = loaded.advance({ticks++, ticks * 3}, input);
  }
  require(launch && launch->kind == tfx::presentation::node_kind::launch, "FWS did not yield a flight launch");
  require(std::get<tfx::presentation::host_action>(launch->data).resource == "%mission" && std::get<uint32_t>(loaded.values().at("mission")) == 501, "FWS selected the wrong first engagement");
  // A supplied flight result tests the parser host boundary, independently of flight/combat proof.
  loaded.values()["score"] = uint32_t{123};
  loaded.values()["miss_val"] = uint32_t{1};
  loaded.values()["outcome"] = uint32_t{0};
  loaded.complete();
  launch = nullptr;
  for(unsigned int index{0}; index != 20000 && !launch; ++index) {
    input.key = tfx::presentation::scene_key::escape;
    launch = loaded.advance({ticks++, ticks * 3}, input);
  }
  require(launch && std::get<uint32_t>(loaded.values().at("mission")) == 502, "training did not continue to the second engagement");
  require(std::get<uint32_t>(loaded.values().at("score")) == 10123, "training overwrote the pilot's accumulated score");

  // Existing lower-case DOS files and the original completed-training marker remain interoperable.
  std::filesystem::rename(scratch.path / "TFXSAV.0", scratch.path / "tfxsav.0");
  store.write(0, false, schema, saved);
  require(!std::filesystem::exists(scratch.path / "TFXSAV.0"), "save created a duplicate file differing only in case");
  saved.values["status"] = uint32_t{1};
  store.write(0, false, schema, saved);
  tfx::presentation::frontend retired{game, tables, scratch.path, "english", true};
  step(retired, 50);
  retired.press(0x50, false);
  retired.press(0x1c, false);
  retired.press(0x1c, false);
  retired.press(0x15, false);                                                  // Delete means mark empty in the chooser, not remove the on-disk save.
  require(std::filesystem::exists(scratch.path / "tfxsav.0"), "inactive pilot confirmation prematurely removed the original file");
  retired.press(0x1c, false);
  retired.press(0x1c, false);
  retired.press(0x1c, false);
  retired.press(0x1c, false);
  step(retired, 100);
  require(std::get<uint32_t>(store.read(0, false, schema).values.at("status")) == 0, "empty/inactive slot replacement did not save a new active pilot");
  require(!store.trained(), "fresh test pilot was already trained");
  store.complete_training();
  require(store.trained(), "completed-training marker did not roundtrip");
  tfx::presentation::frontend qualified{game, tables, scratch.path, "english", true};
  step(qualified, 50);
  qualified.press(0x1c, false);
  qualified.press(0x1c, false);
  qualified.press(0x1c, false);
  qualified.press(0x1c, false);
  require(std::get<uint32_t>(qualified.values().at("rank")) == 1, "completed training did not qualify a new pilot");
  first.values()["score"] = uint32_t{150};
  auto const click{[&](int16_t const x, int16_t const y){
    input.mouse_buttons = 1;
    first.advance({ticks++, ticks * 3}, input, vec2<int16_t>{x, y});
    input.mouse_buttons = 0;
    step(first, 100);
  }};
  click(145, 62);                                                              // Arcade from the original main menu.
  require(first.function_name() == "arcade", "Arcade menu did not open");
  require(!store.read(0, true, schema).resume, "Arcade log unexpectedly stored a resume position");
  click(150, 123);                                                             // The original view-scores action also submits the current pilot's score.
  auto const scores{store.load_scores(tables.scores().defaults)};
  require(scores.front().score == 150 && scores.front().callsign == "INFERNO", "Arcade score action did not persist the current pilot");
  require(!first.pointer_visible(), "high-score view did not suspend the original menu");
  first.press(0x39, false);
  step(first, 100);
  require(first.pointer_visible(), "high-score dismissal did not return to Arcade options");
  click(150, 147);
  require(first.function_name() == "main", "Arcade log restore did not return to the original main menu");
  require(std::get<uint32_t>(first.values().at("score")) == 150, "Arcade log restore lost saved pilot statistics");
  auto corrupt{tfx::resources::read_file(scratch.path / "tfx.hs")};
  corrupt[17] ^= 1;
  {
    std::ofstream file{scratch.path / "tfx.hs", std::ios::binary};
    file.write(reinterpret_cast<char const *>(corrupt.data()), corrupt.size());
  }
  require(store.load_scores(tables.scores().defaults) == tables.scores().defaults, "invalid score checksum did not restore the original roster");
  require(tfx::presentation::decode_scores(tfx::resources::read_file(scratch.path / "tfx.hs")) == tables.scores().defaults, "invalid score file was not repaired");
  // Follow the authored Tour path through squadron selection and its automatic planner review.
  input.key = tfx::presentation::scene_key::escape;
  click(150, 104);
  for(unsigned int frame{0}; frame != 20000 && !(first.function_name() == "todselct" && first.pointer_visible()); ++frame) {
    input.key = tfx::presentation::scene_key::escape;
    require(!first.advance({ticks++, ticks * 3}, input), "Tour intro reached an unexpected host action");
  }
  require(first.function_name() == "todselct" && first.pointer_visible(), "Tour did not reach squadron selection");
  step(first, 50);                                                             // Let the original squadron menu finish its entry fade.
  click(100, 150);                                                             // Blue Angels / Eurofighter.
  launch = nullptr;
  for(unsigned int frame{0}; frame != 20000 && !launch; ++frame) {
    input.mouse_buttons = 2;
    input.key = tfx::presentation::scene_key::escape;
    launch = first.advance({ticks++, ticks * 3}, input);
  }
  require(launch && launch->kind == tfx::presentation::node_kind::launch && std::get<uint32_t>(first.values().at("mission")) == 801, "Tour briefing/review did not return to the authored mission 801 launch");
  first.values()["miss_val"] = uint32_t{1};
  first.values()["outcome"] = uint32_t{0};
  first.complete();
  launch = nullptr;
  for(unsigned int frame{0}; frame != 20000 && !launch; ++frame) {
    input.key = tfx::presentation::scene_key::escape;
    launch = first.advance({ticks++, ticks * 3}, input);
  }
  require(launch && launch->kind == tfx::presentation::node_kind::launch && std::get<uint32_t>(first.values().at("mission")) == 802, "Tour result did not resume its next briefing/review and mission");
  std::cout << "Pilot creation/load/replacement, training flow, native resume/log restore, original Arcade score display/storage, Tour review/progression and qualification marker passed in a private directory\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
