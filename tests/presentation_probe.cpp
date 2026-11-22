#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <utility>
#include "presentation/runtime.h"
#include "resources/archive.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"presentation_probe DATA_DIRECTORY"};
  tfx::resources::archive const game{tfx::resources::find_game_archive(argv[1])};
  std::map<uint32_t, tfx::presentation::program> programs;
  auto const word{[](auto const value){
    for(unsigned int shift{0}; shift != 32; shift += 8) std::cout.put(static_cast<char>(static_cast<uint32_t>(value) >> shift));
  }};
  auto const string{[&](std::string const &value){
    word(value.size());
    std::cout.write(value.data(), static_cast<std::streamsize>(value.size()));
  }};
  char command{0};
  uint32_t id{0};
  while(std::cin >> command >> id) {
    auto found{programs.find(id)};
    if(found == programs.end()) found = programs.emplace(id, tfx::presentation::program{game.read(id)}).first;
    auto const &program{found->second};
    if(command == 'D') {
      word(program.variables.size());
      for(auto const &variable : program.variables) {
        string(variable.name);
        word(variable.text);
      }
      word(program.functions.size());
      for(auto const &function : program.functions) {
        string(function.name);
        word(function.statements.size());
        for(auto const &node : function.statements) {
          word(node.source_offset);
          word(std::to_underlying(node.kind));
          if(auto const entry{std::get_if<tfx::presentation::clip>(&node.data)}) {
            string(entry->resource);
            word(entry->fade_in);
            word(entry->fade_out);
            word(entry->frame_delay);
            word(entry->wait_ticks);
            word(entry->play_until_end);
            word(entry->default_text_palette);
            word(entry->text.has_value());
            if(entry->text) {
              auto const &text{*entry->text};
              word(text.position.x);
              word(text.position.y);
              word(text.alignment);
              word(text.shadow);
              word(text.reveal);
              word(text.font);
              word(text.colour);
              string(text.text);
            }
          } else if(auto const entry{std::get_if<tfx::presentation::branch>(&node.data)}) {
            word(entry->conditional);
            word(entry->variable);
            word(entry->comparison);
            word(entry->value);
            word(entry->subroutine);
            word(entry->target.function);
            word(entry->target.statement);
          } else if(auto const entry{std::get_if<tfx::presentation::assignment>(&node.data)}) {
            word(entry->variable);
            word(std::to_underlying(entry->operation));
            word(entry->value);
          } else if(auto const entry{std::get_if<tfx::presentation::host_action>(&node.data)}) {
            string(entry->resource);
            word(entry->mode);
          } else if(auto const entry{std::get_if<tfx::presentation::audio_request>(&node.data)}) {
            word(entry->device);
            word(entry->operation);
            word(entry->index);
          } else if(std::holds_alternative<tfx::presentation::opaque_action>(node.data)) throw std::runtime_error{"opaque parser action in the retail comparison"};
        }
      }
    } else if(command == 'R') {
      // A suspended flight returns into its real enclosing parser call stack.
      unsigned int function{0};
      unsigned int statement{0};
      unsigned int depth{0};
      std::cin >> function >> statement >> depth;
      tfx::presentation::saved_state saved{
        .values{},
        .reserved{std::vector<uint32_t>(tfx::presentation::saved_variable_slots - program.variables.size(), 0)},
        .callers{},
        .resume{tfx::presentation::location{
          .function{function},
          .statement{statement},
        }},
      };
      for(unsigned int i{0}; i != depth; ++i) {
        tfx::presentation::location caller;
        std::cin >> caller.function >> caller.statement;
        saved.callers.push_back(caller);
      }
      for(auto const &variable : program.variables) {
        if(variable.text) {
          saved.values[variable.name] = std::string{"TEST"};
        } else {
          uint32_t value{0};
          std::cin >> value;
          saved.values[variable.name] = value;
        }
      }
      tfx::presentation::runtime execution{program, saved.values};
      execution.restore(saved);
      unsigned int constexpr action_budget{4096};
      unsigned int actions{0};
      for(; actions != action_budget; ++actions) {
        auto const *node{execution.run()};
        if(!node) break;
        auto const at{*execution.position()};
        std::cout << at.function << ' ' << at.statement << ' ' << execution.call_depth() << ' ' << static_cast<unsigned int>(std::to_underlying(node->kind)) << ' ';
        if(node->kind == tfx::presentation::node_kind::launch || node->kind == tfx::presentation::node_kind::options || node->kind == tfx::presentation::node_kind::quit || node->kind == tfx::presentation::node_kind::planner || node->kind == tfx::presentation::node_kind::load_parser) break;
        execution.complete();
      }
      if(actions == action_budget) throw std::runtime_error{"campaign segment did not reach a host boundary"};
      std::cout << -1 << ' ';
      for(auto const &variable : program.variables) {
        if(!variable.text) std::cout << std::get<uint32_t>(saved.values.at(variable.name)) << ' ';
      }
      auto const final{execution.snapshot(tfx::presentation::save_mode::without_resume)};
      std::cout << final.callers.size();
      for(auto const &caller : final.callers) std::cout << ' ' << caller.function << ' ' << caller.statement;
      std::cout << '\n';
    } else {
      unsigned int function{0};
      unsigned int statement{0};
      uint32_t before{0};
      std::cin >> function >> statement >> before;
      auto const &node{program.functions.at(function).statements.at(statement)};
      tfx::presentation::variables variables;
      if(command == 'B') {
        auto const &branch{std::get<tfx::presentation::branch>(node.data)};
        if(branch.conditional) variables[program.variables.at(branch.variable).name] = before;
        bool const taken{tfx::presentation::branch_taken(branch, program, variables)};
        std::cout << taken << ' ' << (taken && branch.subroutine) << '\n';
      } else if(command == 'A') {
        auto const &assignment{std::get<tfx::presentation::assignment>(node.data)};
        auto const &name{program.variables.at(assignment.variable).name};
        variables[name] = before;
        tfx::presentation::assign(assignment, program, variables);
        std::cout << std::get<uint32_t>(variables.at(name)) << '\n';
      } else {
        throw std::runtime_error{"unknown presentation comparison operation"};
      }
    }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
