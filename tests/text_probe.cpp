#include <cstdlib>
#include <iostream>
#include <map>
#include "presentation/text.h"
#include "resources/archive.h"
#include "resources/executable_tables.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"text_probe DATA_DIRECTORY"};
  auto const directory{tfx::resources::find_game_archive(argv[1]).parent_path()};
  tfx::resources::archive const game{directory / "DID.DAT"};
  tfx::resources::executable_tables const tables{directory / "TFX.DAT"};
  tfx::presentation::program const root{game.read("tfx.par")};
  tfx::presentation::variables variables;
  for(unsigned int index{0}; index != root.variables.size(); ++index) {
    auto const &variable{root.variables[index]};
    if(variable.text) variables[variable.name] = std::string{variable.name == "callsign" ? "MiG" : variable.name == "firstname" ? "Ada" : "Slowriot"};
    else variables[variable.name] = variable.name == "score" ? uint32_t{0xfffffffeu} : 7 + index * 12345u;
  }
  std::map<uint32_t,tfx::presentation::program> programs;
  auto const word{[](auto const value){ for(unsigned int shift{0}; shift != 32; shift += 8) std::cout.put(static_cast<char>(static_cast<uint32_t>(value) >> shift)); }};
  uint32_t id{0};unsigned int function{0},node{0},visible{0};std::string language;
  while(std::cin >> id >> function >> node >> language >> visible) {
    auto found{programs.find(id)};
    if(found == programs.end()) found = programs.emplace(id,tfx::presentation::program{game.read(id)}).first;
    auto const &source{*std::get<tfx::presentation::clip>(found->second.functions.at(function).statements.at(node).data).text};
    auto const text{tfx::presentation::prepare_text(source,variables,tables.presentation_fonts(),[&](std::string_view const filename){
      auto const content{game.read(language + "/" + std::string{filename})};
      return std::string{content.begin(),content.end()};
    })};
    word(text.lines.size());word(text.characters);word(text.font);word(text.colour);word(text.shadow);
    for(auto const &line : text.lines) {
      word(line.text.size());std::cout.write(line.text.data(),static_cast<std::streamsize>(line.text.size()));
      word(line.position.x);word(line.position.y);word(line.width);
    }
    tfx::render::framebuffer output,coverage;
    tfx::presentation::draw_text(output,text,tables.presentation_fonts(),visible,&coverage);
    for(auto const *frame : {&output,&coverage}) std::cout.write(reinterpret_cast<char const*>(frame->pixels.data()),static_cast<std::streamsize>(frame->pixels.size()));
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
