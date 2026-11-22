#include <cstdlib>
#include <iostream>
#include "presentation/player.h"
#include "resources/archive.h"
#include "resources/executable_tables.h"

int main(int const argc, char const *const argv[]) try {
  bool const corpus{argc == 3 && std::string_view{argv[1]} == "--corpus"};
  if(argc != 2 && !corpus) throw std::runtime_error{"player_probe [--corpus] DATA_DIRECTORY"};
  auto const directory{tfx::resources::find_game_archive(argv[corpus ? 2 : 1]).parent_path()};
  tfx::resources::archive const game{directory / "DID.DAT"};
  tfx::resources::executable_tables const tables{directory / "TFX.DAT"};
  tfx::presentation::variables variables;
  tfx::presentation::program const root{game.read("tfx.par")};
  for(unsigned int index{0}; index != root.variables.size(); ++index) {
    auto const &variable{root.variables[index]};
    if(variable.text) variables[variable.name] = std::string{variable.name == "callsign" ? "MiG" : variable.name == "firstname" ? "Ada" : "Slowriot"};
    else variables[variable.name] = variable.name == "score" ? uint32_t{0xfffffffeu} : 7 + index * 12345u;
  }
  tfx::presentation::dac_palette current;
  for(unsigned int index{0}; index != current.size(); ++index) current[index] = static_cast<uint8_t>(index % 64);
  if(corpus) {
    unsigned int clips{0};uint64_t frames{0};
    std::array<std::string_view,9> constexpr names{"tfx","cdintro","cddeath1","cdbusted","cdcolint","cdsomint","cdfwsint","cdyugmid","cdyugint"};
    for(auto const name : names) {
      tfx::presentation::program const program{game.read(std::string{name} + ".par")};
      for(auto const &function : program.functions) for(auto const &node : function.statements) {
        auto const clip{std::get_if<tfx::presentation::clip>(&node.data)};
        if(!clip) continue;
        auto const media{node.kind == tfx::presentation::node_kind::text ? tfx::resources::bytes{} : game.read(node.kind == tfx::presentation::node_kind::image ? "iff/raw/" + clip->resource + ".lbm" : "anim/" + clip->resource + ".256")};
        for(std::string const language : {"english","french","german","spanish","italian"}) {
          if(language != "english" && (!clip->text || clip->text->text.find('^') == std::string::npos)) continue;
          tfx::presentation::scene_input input;
          tfx::presentation::sequence_controls sequence;
          tfx::presentation::scene_player player{node.kind,*clip,media,tables.presentation_fonts(),variables,[&](std::string_view const filename){
            auto const content{game.read(language + "/" + std::string{filename})};return std::string{content.begin(),content.end()};
          },current,tables.text_palette,{0,0},input};
          bool done{false};
          for(unsigned int frame{0}; frame != 1200 && !done; ++frame) {
            if(frame % 300 == 299) input.key = tfx::presentation::scene_key::space;
            done = player.advance({frame / 4,frame * 3 / 4},input,sequence);
            ++frames;
          }
          if(!done) throw std::runtime_error{"corpus scene did not end: " + std::string{name} + "/" + function.name + "/" + clip->resource};
          ++clips;
        }
      }
    }
    std::cout << clips << " complete corpus scenes played across " << frames << " frame/ramp iterations, including all five languages\n";
    return EXIT_SUCCESS;
  }
  uint32_t id{0};unsigned int function{0},node{0};
  while(std::cin >> id >> function >> node) {
    tfx::presentation::program const program{game.read(id)};
    std::cout << "C " << id << ' ' << function << ' ' << node << '\n';
    auto const &statement{program.functions.at(function).statements.at(node)};
    auto clip{std::get<tfx::presentation::clip>(statement.data)};
    clip.fade_in = 0;clip.fade_out = 0;clip.wait_ticks = 12;
    auto const media{statement.kind == tfx::presentation::node_kind::text ? tfx::resources::bytes{} : game.read("iff/raw/" + clip.resource + ".lbm")};
    tfx::presentation::scene_input input;
    tfx::presentation::sequence_controls sequence;
    tfx::presentation::scene_player player{statement.kind,clip,media,tables.presentation_fonts(),variables,[&](std::string_view const filename){
      auto const content{game.read("english/" + std::string{filename})};return std::string{content.begin(),content.end()};
    },current,tables.text_palette,{0,0},input};
    // Fade steps are verified independently; leave the fast text clock stationary during the ramp.
    while(player.stage() == tfx::presentation::playback_phase::fade_in) player.advance({0,0},input,sequence);
    for(unsigned int frame{0}; frame != 64 && player.stage() == tfx::presentation::playback_phase::content; ++frame) {
      if(frame == 3) input.key = tfx::presentation::scene_key::space;
      if(frame == 10) input.key = tfx::presentation::scene_key::escape;
      player.advance({frame / 4,frame * 3},input,sequence);
      uint64_t digest{14695981039346656037ULL};
      for(auto const pixel : player.image().pixels) digest = (digest ^ pixel) * 1099511628211ULL;
      std::cout << digest << ' ' << player.visible_characters() << '\n';
    }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
