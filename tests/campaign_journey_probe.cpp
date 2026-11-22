#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include "presentation/pilots.h"
#include "presentation/runtime.h"
#include "resources/archive.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 3) throw std::runtime_error{"campaign_journey_probe DATA_DIRECTORY PRIVATE_SAVE_DIRECTORY"};
  tfx::resources::archive const game{tfx::resources::find_game_archive(argv[1])};
  tfx::presentation::program const schema{game.read("tfx.par")};
  tfx::presentation::pilot_store store{argv[2]};
  tfx::presentation::variables values;
  std::unique_ptr<tfx::presentation::runtime> execution;
  char command{0};
  while(std::cin >> command) {
    if(command == 'R') {
      tfx::presentation::location entry;
      std::cin >> entry.function >> entry.statement;
      values.clear();
      for(auto const &variable : schema.variables) {
        if(variable.text) values[variable.name] = std::string{"TEST"};
        else {
          uint32_t number{0};
          std::cin >> number;
          values[variable.name] = number;
        }
      }
      execution = std::make_unique<tfx::presentation::runtime>(schema,values,entry);
    } else if(command == 'V') {
      std::string name;
      uint32_t number{0};
      std::cin >> name >> number;
      values.at(name) = number;
    } else if(command == 'C') execution->complete();
    else if(command == 'N') {
      auto const *node{execution->run()};
      if(!node) { std::cout << -1 << '\n';continue; }
      auto const at{*execution->position()};
      std::cout << at.function << ' ' << at.statement << ' ' << static_cast<unsigned int>(std::to_underlying(node->kind));
      for(auto const &variable : schema.variables) if(!variable.text) std::cout << ' ' << std::get<uint32_t>(values.at(variable.name));
      auto const state{execution->snapshot(tfx::presentation::save_mode::without_resume)};
      std::cout << ' ' << state.callers.size();
      for(auto const &caller : state.callers) std::cout << ' ' << caller.function << ' ' << caller.statement;
      std::cout << '\n';
    } else if(command == 'S' || command == 'L') {
      unsigned int slot{0}, log{0}, mode{0};
      std::cin >> slot >> log;
      if(command == 'L') execution->restore(store.read(slot,log != 0,schema));
      else {
        std::cin >> mode;
        auto const saved{execution->snapshot(static_cast<tfx::presentation::save_mode>(mode))};
        store.write(slot,log != 0,schema,saved);
        auto const encoded{tfx::presentation::encode_save(schema,store.read(slot,log != 0,schema))};
        std::cout << 'S' << ' ' << encoded.size();
        for(auto const byte : encoded) std::cout << ' ' << static_cast<unsigned int>(byte);
        std::cout << '\n';
      }
    } else throw std::runtime_error{"unknown campaign journey operation"};
    if(!std::cin) throw std::runtime_error{"truncated campaign journey operation"};
  }
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
