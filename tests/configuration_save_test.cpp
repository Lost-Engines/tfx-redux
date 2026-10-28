#include <chrono>
#include <fstream>
#include <iostream>
#include "configuration/save.h"
#include "resources/binary.h"

int main() try {
  auto const path{std::filesystem::temp_directory_path() / ("tfx-preferences-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
  if(!std::filesystem::create_directory(path)) throw std::runtime_error{"cannot create private preference fixture"};
  struct cleanup {
    std::filesystem::path const &directory;
    ~cleanup() { std::error_code error;std::filesystem::remove_all(directory,error); }
  } const cleanup{path};
  auto const check{[](bool const value) { if(!value) throw std::runtime_error{"preference file integration mismatch"}; }};
  auto const fresh{tfx::configuration::load(path)};
  check(fresh.control == tfx::flight::control_source::keyboard && fresh.preferred_control == fresh.control);
  check(fresh.model == tfx::simulation::flight_model::simple);
  tfx::configuration::settings state;
  state.detail = 1;
  state.joystick_scale = 513.5f;
  state.hud_green = 96;
  state.effects.wind = 1;
  check(!tfx::configuration::save_preferences(path,state));
  auto const config{path / "tfx.cfg"};
  { std::ofstream output{config,std::ios::binary};output << "CONTROL=0\r\n"; }
  check(tfx::configuration::load(path).model == tfx::simulation::flight_model::arcade);
  std::string const original{"CONTROL=2\r\nMODEL=3\r\nBLASTER=A220 I7 D1 T6\r\nEXTRA=keep this\r\nDETAIL=2\r\nJOYSCALE=512\r\nHUDCOL=120\r\nREALISM=2,1,1,1,0,1,0\r\n"};
  { std::ofstream output{config,std::ios::binary};output << original; }
  check(tfx::configuration::save_preferences(path,state));
  check(!std::filesystem::exists(path / "TFX.CFG"));
  auto const bytes{tfx::resources::read_file(config)};
  std::string const updated{bytes.begin(),bytes.end()};
  check(updated.starts_with("CONTROL=2\r\nMODEL=3\r\nBLASTER=A220 I7 D1 T6\r\nEXTRA=keep this\r\n"));
  auto const loaded{tfx::configuration::load(path)};
  check(loaded.detail == 1 && loaded.joystick_scale == 514 && loaded.hud_green == 96 && loaded.effects.wind == 1);
  check(loaded.control == tfx::flight::control_source::mouse && loaded.model == tfx::simulation::flight_model::military_spec);
  // Refusing an oversized file must leave it byte-for-byte intact.
  std::string const oversized(tfx::configuration::configuration_buffer_size + 1,'x');
  { std::ofstream output{config,std::ios::binary};output << oversized; }
  check(!tfx::configuration::save_preferences(path,state));
  check(tfx::resources::read_file(config) == tfx::resources::bytes(oversized.begin(),oversized.end()));
  check(!tfx::configuration::update_preferences(std::string(tfx::configuration::configuration_buffer_size - 1,'x'),state));
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
