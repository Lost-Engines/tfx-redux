#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "presentation/runtime.h"

namespace {

void require(bool const condition, char const *const message) {
  if(!condition) throw std::runtime_error{message};
}
struct graph_writer {
  tfx::resources::bytes bytes;
  void byte(unsigned int const value) {
    bytes.push_back(static_cast<uint8_t>(value));
  }
  void word(unsigned int const value) {
    byte(value);
    byte(value >> 8);
  }
  void dword(uint32_t const value) {
    word(value);
    word(value >> 16);
  }
  void zeros(unsigned int const count) {
    for(unsigned int i{0}; i != count; ++i) byte(0);
  }
  void text(std::string const &text, unsigned int const width) {
    for(unsigned int i{0}; i != width; ++i) byte(i < text.size() ? static_cast<uint8_t>(text[i]) : 0);
  }
  void function(std::string const &name, unsigned int const count) {
    zeros(12);
    word(count);
    text(name, 11);
  }
  void node(unsigned int const kind) {
    byte(kind);
    zeros(12);
  }
  void assign(unsigned int const operation, uint32_t const value) {
    node(10);
    dword(0);
    byte(operation);
    dword(value);
  }
};

} // anonymous namespace

int main() try {
  graph_writer writer;
  writer.word(1);
  writer.zeros(9);
  writer.text("score", 21);
  writer.word(2);
  writer.function("main", 5);
  writer.assign(0, 0xfffffffeu);
  writer.node(3);
  writer.byte(0);
  writer.dword(0);
  writer.byte(0);
  writer.dword(0);
  writer.byte(1);
  writer.byte(1);
  writer.byte(0);
  writer.dword(0);
  writer.node(5);
  writer.text("501", 82);
  writer.assign(2, 4);
  writer.node(7);
  writer.function("sub", 3);
  writer.assign(1, 5);
  writer.node(5);
  writer.text("502", 82);
  writer.node(4);
  tfx::presentation::program const program{writer.bytes};
  tfx::presentation::variables values;
  tfx::presentation::runtime runtime{program, values};
  auto const *action{runtime.run()};
  require(action && std::get<tfx::presentation::host_action>(action->data).resource == "502" && runtime.call_depth() == 1, "subroutine did not suspend at its host action");
  auto const saved{runtime.snapshot()};
  require(saved.callers == std::vector<tfx::presentation::location>{{0, 1}} && saved.resume == tfx::presentation::location{1, 1}, "save must retain native call-site/current statement indices");
  require(runtime.snapshot(tfx::presentation::save_mode::previous_statement).resume == tfx::presentation::location{1, 0} && runtime.snapshot(tfx::presentation::save_mode::following_statement).resume == tfx::presentation::location{1, 2}, "native save resume modes differ");
  tfx::presentation::variables loaded;
  tfx::presentation::runtime restored{program, loaded};
  restored.restore(tfx::presentation::decode_save(program, tfx::presentation::encode_save(program, saved)));
  restored.complete();
  auto const *resumed{restored.run()};
  require(resumed && std::get<tfx::presentation::host_action>(resumed->data).resource == "501" && restored.call_depth() == 0 && loaded == values, "saved nested execution did not return to the statement after its call");
  runtime.complete();
  action = runtime.run();
  require(action && action->kind == tfx::presentation::node_kind::launch && std::get<tfx::presentation::host_action>(action->data).resource == "501", "gosub did not return to the next host action");
  require(std::get<uint32_t>(values.at("score")) == 3 && runtime.call_depth() == 0, "assignment wrap or return stack differs");
  runtime.complete();
  action = runtime.run();
  require(action && action->kind == tfx::presentation::node_kind::quit && std::get<uint32_t>(values.at("score")) == 0xffffffffu, "post-host numeric flow differs");
  runtime.complete();
  require(!runtime.run(), "completed root parser did not finish");
  for(unsigned int length{0}; length != writer.bytes.size(); ++length) {
    bool rejected{false};
    try {
      tfx::presentation::program incomplete{std::span{writer.bytes}.first(length)};
    } catch(std::runtime_error const &) {
      rejected = true;
    }
    require(rejected, "truncated graph was accepted");
  }
  std::cout << "Parser graph bounds, wrapping assignments, subroutine return and explicit host suspension passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
