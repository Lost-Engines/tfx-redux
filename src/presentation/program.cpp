#include "program.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace tfx::presentation {
namespace {

std::string string(std::span<uint8_t const> const bytes) {
  auto const end{std::ranges::find(bytes,uint8_t{0})};
  if(end == bytes.end()) throw std::runtime_error{"unterminated parser string"};
  return {bytes.begin(),end};
}
uint32_t value(variables const &state, std::string const &name) {
  auto const found{state.find(name)};
  if(found == state.end()) return 0;
  if(auto const number{std::get_if<uint32_t>(&found->second)}) return *number;
  throw std::runtime_error{"numeric parser operation names a string: " + name};
}

} // anonymous namespace

program::program(std::span<uint8_t const> const source) {
  // Native saved-graph layout: linked-list pointers are saved editor bookkeeping, not executable addresses.
  resources::binary_view const view{source};
  unsigned int at{0};
  auto const take{[&](unsigned int const length){ auto const data{view.slice(at,length)};at += length;return data; }};
  auto const word{[&]{ auto const data{take(2)};return resources::binary_view{data}.le16(0); }};
  auto const text{[&]{
    resources::binary_view const header{take(16)};
    auto const content{take(header.le16(10))};
    return text_block{
      .position{header.signed_le16(2),header.signed_le16(0)},
      .alignment{static_cast<text_alignment>(header.byte(4))},
      .shadow{header.byte(6) != 0},
      .reveal{header.byte(7) != 0},
      .font{header.byte(8)},
      .colour{header.byte(9)},
      .text{content.begin(),std::ranges::find(content,uint8_t{0})},
    };
  }};
  unsigned int const variable_count{word()};
  for(unsigned int index{0}; index != variable_count; ++index) {
    resources::binary_view const entry{take(30)};
    variables.push_back({
      .name{string(entry.slice(9,21))},
      .text{entry.byte(8) != 0},
    });
  }
  unsigned int const function_count{word()};
  std::array<unsigned int,15> constexpr payload_sizes{22,18,9,17,0,82,91,0,82,11,9,4,0,82,42};
  for(unsigned int index{0}; index != function_count; ++index) {
    resources::binary_view const header{take(25)};
    function current{
      .name{string(header.slice(14,11))},
      .statements{},
    };
    unsigned int const count{header.le16(12)};
    for(unsigned int node{0}; node != count; ++node) {
      unsigned int const offset{at};
      auto const kind{static_cast<node_kind>(take(13)[0])};
      unsigned int const type{std::to_underlying(kind)};
      if(type >= payload_sizes.size()) throw std::runtime_error{"unknown saved parser node"};
      auto const raw{take(payload_sizes[type])};
      resources::binary_view const payload{raw};
      action decoded;
      switch(kind) {
      case node_kind::animation: case node_kind::image: case node_kind::text: {
        bool const animation{kind == node_kind::animation};
        bool const standalone{kind == node_kind::text};
        clip entry;
        if(!standalone) {
          entry.resource = string(payload.slice(0,9));
          entry.fade_in = payload.byte(9);
          entry.fade_out = payload.byte(10);
        }
        if(standalone) entry.default_text_palette = payload.byte(2) == 1;
        entry.wait_ticks = payload.signed_le16(standalone ? 3 : animation ? 14 : 11);
        if(animation) {
          entry.frame_delay = payload.le16(11);
          entry.play_until_end = payload.byte(13) != 0;
        }
        if(standalone || payload.byte(animation ? 17 : 13)) entry.text = text();
        decoded.emplace<clip>(std::move(entry));
        break;
      }
      case node_kind::branch:
        decoded.emplace<branch>(branch{
          .conditional{payload.byte(0) != 0},
          .variable{payload.le32(1)},
          .comparison{payload.byte(5)},
          .value{payload.le32(6)},
          .subroutine{payload.byte(10) == 1},
          .target{
            .function{payload.byte(11)},
            .statement{payload.byte(12)},
          },
        });
        break;
      case node_kind::assign:
        decoded.emplace<assignment>(assignment{
          .variable{payload.le32(0)},
          .operation{static_cast<assignment_operation>(payload.byte(4))},
          .value{payload.le32(5)},
        });
        break;
      case node_kind::launch: case node_kind::options: case node_kind::load_parser: case node_kind::planner:
        decoded.emplace<host_action>(host_action{
          .resource{string(kind == node_kind::planner ? payload.slice(0,9) : raw)},
          .mode{kind == node_kind::planner ? payload.le16(9) : uint16_t{0}},
        });
        break;
      case node_kind::audio:
        decoded.emplace<audio_request>(audio_request{
          .device{payload.byte(0)},
          .operation{payload.byte(1)},
          .index{payload.signed_le16(2)},
        });
        break;
      case node_kind::condition: case node_kind::ultra_choice: {
        opaque_action entry{
          .payload{raw.begin(),raw.end()},
        };
        if(kind == node_kind::ultra_choice) for(bool const active : {true,payload.le32(34) != 0,payload.le32(38) != 0}) if(active) {
          auto const buffer{take(17)};
          entry.buffers.emplace_back(buffer.begin(),buffer.end());
        }
        decoded.emplace<opaque_action>(std::move(entry));
        break;
      }
      default: break;
      }
      current.statements.push_back({
        .source_offset{offset},
        .kind{kind},
        .data{std::move(decoded)},
      });
    }
    functions.push_back(std::move(current));
  }
  if(at != source.size()) throw std::runtime_error{"unused saved parser bytes"};
  for(auto const &function : functions) for(auto const &node : function.statements) {
    if(auto const test{std::get_if<branch>(&node.data)}) {
      if(test->target.function >= functions.size() || test->target.statement >= functions[test->target.function].statements.size()) throw std::runtime_error{"parser branch leaves its program"};
      if(test->conditional && test->variable >= variables.size()) throw std::runtime_error{"parser branch variable is absent"};
    } else if(auto const assignment{std::get_if<presentation::assignment>(&node.data)}) {
      if(assignment->variable >= variables.size()) throw std::runtime_error{"parser assignment variable is absent"};
    }
  }
}

bool branch_taken(branch const &test, program const &source, presentation::variables const &state) {
  if(!test.conditional) return true;
  if(test.comparison != 0) throw std::runtime_error{"unimplemented parser comparison"};
  return value(state,source.variables.at(test.variable).name) == test.value;
}

void assign(assignment const &assignment, program const &source, presentation::variables &state) {
  auto const &name{source.variables.at(assignment.variable).name};
  auto const before{value(state,name)};
  switch(assignment.operation) {
  case assignment_operation::set: state[name] = assignment.value; break;
  case assignment_operation::add: state[name] = before + assignment.value; break;
  case assignment_operation::subtract: state[name] = before - assignment.value; break;
  default: break;                                                              // Native 0xb28b8 ignores unknown operation bytes.
  }
}

} // namespace tfx::presentation
