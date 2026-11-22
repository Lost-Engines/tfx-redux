#include <chrono>
#include <cstdlib>
#include <iostream>
#include "resources/archive.h"

namespace {
struct temporary_directory {
  std::filesystem::path path{std::filesystem::temp_directory_path() / ("tfx-resource-files-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
  temporary_directory() {
    if(!std::filesystem::create_directory(path)) throw std::runtime_error{"cannot create private resource fixture directory"};
  }
  ~temporary_directory() {
    std::error_code error;
    std::filesystem::remove_all(path,error);
  }
};
void write(std::filesystem::path const &path, std::span<uint8_t const> const bytes) {
  std::ofstream output{path,std::ios::binary};
  if(!output.write(reinterpret_cast<char const*>(bytes.data()),static_cast<std::streamsize>(bytes.size()))) throw std::runtime_error{"cannot write resource fixture"};
}
} // anonymous namespace
int main() try {
  temporary_directory directory;
  tfx::resources::bytes bytes(4);
  auto const word{[&](uint16_t const value){ bytes.push_back(static_cast<uint8_t>(value));bytes.push_back(static_cast<uint8_t>(value >> 8)); }};
  auto const dword{[&](uint32_t const value){ word(static_cast<uint16_t>(value));word(static_cast<uint16_t>(value >> 16)); }};
  tfx::resources::bytes const archived{'a','r','c','h','i','v','e'};
  bytes.insert(bytes.end(),archived.begin(),archived.end());
  auto const index{static_cast<uint32_t>(bytes.size())};
  for(unsigned int i{0}; i != 4; ++i) bytes[i] = static_cast<uint8_t>(index >> (8 * i));
  word(1);word(0);word(0);word(0);
  dword(tfx::resources::hash_short_name("leafsam"));dword(4);dword(static_cast<uint32_t>(archived.size()));
  write(directory.path / "DID.DAT",bytes);
  tfx::resources::bytes const loose{'l','o','o','s','e'};
  write(directory.path / "leaf.sam",loose);
  std::filesystem::create_directory(directory.path / "TUNES");
  tfx::resources::bytes const compressed{0x52,0x41,0,2,3,0,0,0,0,0x7d,0,0,1,1,0,1,0x40,1,0xfe,1,5,0,0x61,0x62,0x63};
  write(directory.path / "TUNES/INTRO.RLD",compressed);
  tfx::resources::archive const files{directory.path / "DID.DAT"};
  if(files.read("leaf.sam") != archived) throw std::runtime_error{"loose file incorrectly overrides the native archive"};
  if(!files.contains("tunes\\intro.rld ") || files.read("tunes/intro.rld ") != tfx::resources::bytes{'a','b','c'}) throw std::runtime_error{"DOS filename padding/case or loose RA decoding changed"};
  if(files.contains("../DID.DAT") || files.contains("missing.sam")) throw std::runtime_error{"missing resource unexpectedly exists"};
  tfx::resources::archive const memory_only{bytes};
  if(memory_only.contains("tunes/intro.rld ")) throw std::runtime_error{"in-memory archive acquired an unrelated filesystem"};
  return EXIT_SUCCESS;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
