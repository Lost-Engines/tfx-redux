#include <cstdlib>
#include <iomanip>
#include <iostream>
#include "presentation/score_player.h"
#include "presentation/scores.h"
#include "resources/archive.h"

namespace {
tfx::resources::bytes unhex(std::string_view const text) {
  auto const digit{[](char const value){ return static_cast<unsigned int>(value <= '9' ? value - '0' : value - 'a' + 10); }};
  tfx::resources::bytes result;
  for(unsigned int at{0}; at < text.size(); at += 2) result.push_back(static_cast<uint8_t>(digit(text[at]) * 16 + digit(text[at + 1])));
  return result;
}
void hex(std::span<uint8_t const> const bytes) {
  char constexpr digits[]{"0123456789abcdef"};
  for(auto const byte : bytes) std::cout << digits[byte >> 4] << digits[byte & 15];
  std::cout << '\n';
}
} // anonymous namespace
int main(int const argc, char const *const argv[]) try {
  if(argc == 5) {
    auto const archive{tfx::resources::find_game_archive(argv[1])};
    tfx::resources::archive const game{archive};
    auto const executable{tfx::resources::read_file(archive.parent_path() / "TFX.DAT")};
    tfx::presentation::fonts const fonts{executable};
    tfx::presentation::score_assets const assets{executable};
    std::string source;std::cin >> source;
    int const selected{std::stoi(argv[3])};
    tfx::presentation::score_player player{fonts,assets,tfx::presentation::unpack_scores(unhex(source)),selected < 0 ? std::nullopt : std::optional<unsigned int>{static_cast<unsigned int>(selected)},game.read("iff/raw/hiscore.lbm")};
    for(unsigned int frame{0}; frame != std::stoul(argv[4]); ++frame) {
      player.advance(0);
      auto const &pixels{player.image().pixels};
      std::cout.write(reinterpret_cast<char const*>(pixels.data()),pixels.size());
    }
    return EXIT_SUCCESS;
  }
  if(argc != 2) throw std::runtime_error{"score_probe TFX_DAT, or score_probe DATA_DIRECTORY --frames SLOT COUNT"};
  tfx::presentation::score_assets const assets{tfx::resources::read_file(argv[1])};
  std::string operation,source;
  while(std::cin >> operation) {
    if(operation == "defaults") { hex(tfx::presentation::pack_scores(assets.defaults));continue; }
    std::cin >> source;
    if(operation == "encode") hex(tfx::presentation::encode_scores(tfx::presentation::unpack_scores(unhex(source))));
    else if(operation == "decode") {
      auto const decoded{tfx::presentation::decode_scores(unhex(source))};
      if(decoded) hex(tfx::presentation::pack_scores(*decoded));
      else std::cout << "invalid\n";
    } else if(operation == "insert") {
      auto scores{tfx::presentation::unpack_scores(unhex(source))};
      tfx::presentation::score_entry entry;
      std::cin >> entry.score >> std::quoted(entry.firstname) >> std::quoted(entry.surname) >> std::quoted(entry.callsign);
      auto const slot{tfx::presentation::insert_score(scores,std::move(entry))};
      std::cout << (slot ? static_cast<int>(*slot) : -1) << ' ';
      hex(tfx::presentation::pack_scores(scores));
    }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
