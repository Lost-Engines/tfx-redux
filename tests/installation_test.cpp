#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include "resources/installation.h"

namespace {

void check(bool const condition, char const *message) {
  if(!condition) throw std::runtime_error{message};
}
void environment(char const *name, std::filesystem::path const &value) {
#ifdef _WIN32
  if(_putenv_s(name, value.string().c_str())) throw std::runtime_error{"cannot set test environment"};
#else
  if(setenv(name, value.c_str(), 1)) throw std::runtime_error{"cannot set test environment"};
#endif // _WIN32
}

} // anonymous namespace

int main(int const, char const *const argv[]) try {
  namespace fs = std::filesystem;
  auto const binary_directory{fs::absolute(argv[0]).parent_path()};
  auto const original_directory{fs::current_path()};
  auto const root{fs::temp_directory_path() / ("tfx-installation-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
  if(!fs::create_directory(root)) throw std::runtime_error{"cannot create private installation fixture"};
  struct cleanup {
    fs::path const &original;
    fs::path const &temporary;
    ~cleanup() {
      std::error_code error;
      fs::current_path(original, error);
      fs::remove_all(temporary, error);
    }
  } const cleanup{
    .original{original_directory},
    .temporary{root},
  };
  fs::current_path(root);
  environment("HOME", root / "home with spaces");
  environment("XDG_DATA_HOME", root / "data with spaces");
#ifdef _WIN32
  check(fs::equivalent(tfx::resources::default_game_directory(), binary_directory), "Windows assets must follow the executable, not the working directory");
#else
  static_cast<void>(binary_directory);
  check(tfx::resources::default_game_directory() == root / "data with spaces/tfx", "absolute XDG_DATA_HOME ignored");
  environment("XDG_DATA_HOME", "relative/data");
  check(tfx::resources::default_game_directory() == root / "home with spaces/.local/share/tfx", "relative XDG_DATA_HOME must be ignored");
  environment("XDG_DATA_HOME", "");
  check(tfx::resources::default_game_directory() == root / "home with spaces/.local/share/tfx", "empty XDG_DATA_HOME must use HOME");
  auto const defaults{tfx::resources::default_game_directory()};
  fs::create_directories(defaults);
  {
  std::ofstream file{defaults / "DID.DAT"};
}
  check(tfx::resources::select_game_archive(std::nullopt) == defaults / "DID.DAT", "default installation not found");
#endif // _WIN32
  {
    std::ofstream file{"did.dat"};
  }
  check(fs::equivalent(tfx::resources::select_game_archive(std::nullopt), root / "did.dat"), "local lowercase installation must take precedence");
  auto const selected{root / "another installation"};
  fs::create_directory(selected);
  {
    std::ofstream file{selected / "DID.DAT"};
  }
  check(tfx::resources::select_game_archive(selected) == selected / "DID.DAT", "explicit installation ignored");
  check(tfx::resources::select_game_archive(selected / "DID.DAT") == selected / "DID.DAT", "explicit archive file ignored");
  bool refused{false};
  try {
    tfx::resources::select_game_archive(root / "missing");
  }
  catch(std::runtime_error const &) {
    refused = true;
  }
  check(refused, "invalid explicit installation silently fell back to another copy");
  auto const directories{tfx::resources::rom_directories(selected, std::nullopt)};
  check(directories == std::vector<fs::path>{selected, tfx::resources::default_game_directory()}, "ROM discovery must prefer the game directory and then the fetch destination");
  auto const explicit_roms{root / "custom ROMs"};
  check(tfx::resources::rom_directories(selected, explicit_roms) == std::vector<fs::path>{explicit_roms}, "explicit ROM directory must override discovery even when missing");
  auto const fetched{tfx::resources::default_game_directory()};
  check(tfx::resources::rom_directories(fetched, std::nullopt) == std::vector<fs::path>{fetched}, "default ROM directory tried twice");
  check(tfx::resources::rom_directories(fetched / ".", std::nullopt).size() == 1, "equivalent spellings of the default ROM directory tried twice");
  // Windows executable and temporary directories can be on different drives.
  auto const relative{fs::relative(fetched)};
  if(!relative.empty()) check(tfx::resources::rom_directories(relative, std::nullopt).size() == 1, "relative and absolute copies of the default ROM directory tried twice");
  std::cout << "Default game/ROM lookup, local precedence and explicit overrides passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
