#include "runtime.h"
#include <stdexcept>

namespace tfx::presentation {

runtime::runtime(program const &program, variables &variables, location const entry) : source{program}, state{variables} {
  for(auto const &variable : source.variables) state.try_emplace(variable.name,variable.text ? variable_value{std::string{}} : variable_value{uint32_t{0}});
  if(source.functions.empty()) return;
  if(entry.function >= source.functions.size() || entry.statement >= source.functions[entry.function].statements.size()) throw std::runtime_error{"parser entry leaves its program"};
  cursor = entry;
}

std::optional<location> runtime::position() const {
  return cursor;
}
unsigned int runtime::call_depth() const {
  return static_cast<unsigned int>(callers.size());
}
statement const *runtime::current() const {
  return cursor ? &source.functions[cursor->function].statements[cursor->statement] : nullptr;
}
std::optional<location> runtime::successor() const {
  if(!cursor || cursor->statement + 1 == source.functions[cursor->function].statements.size()) return {};
  return location{
    .function{cursor->function},
    .statement{cursor->statement + 1},
  };
}

statement const *runtime::run() {
  unsigned int constexpr control_flow_budget{65536};
  for(unsigned int step{0}; step != control_flow_budget; ++step) {
    auto const *node{current()};
    if(!node) return nullptr;
    if(node->kind == node_kind::branch) {
      auto const &test{std::get<branch>(node->data)};
      if(branch_taken(test,source,state)) {
        if(test.subroutine) {
          if(callers.size() == saved_return_limit) throw std::runtime_error{"parser calls exceed the reconstruction's safety limit"};
          callers.push_back(*cursor);
        }
        cursor = test.target;
      } else cursor = successor();
    } else if(node->kind == node_kind::assign) {
      assign(std::get<assignment>(node->data),source,state);
      cursor = successor();
    } else if(node->kind == node_kind::return_from_call) {
      if(callers.empty()) throw std::runtime_error{"parser return has no caller"};
      cursor = callers.back();
      callers.pop_back();
      cursor = successor();
    } else return node;
  }
  throw std::runtime_error{"parser control flow does not yield"};
}

void runtime::complete() {
  if(!cursor) throw std::runtime_error{"cannot complete an ended parser action"};
  cursor = successor();
}

saved_state runtime::snapshot(save_mode const mode) const {
  if(source.variables.size() > saved_variable_slots) throw std::runtime_error{"parser exceeds native save storage"};
  saved_state result{
    .values{state},
    .reserved{reserved.empty() ? std::vector<uint32_t>(saved_variable_slots - source.variables.size(),0) : reserved},
    .callers{callers},
    .resume{},
  };
  if(mode == save_mode::without_resume) return result;
  if(!cursor) throw std::runtime_error{"ended parser has no save resume statement"};
  result.resume = cursor;
  switch(mode) {
  case save_mode::without_resume: break;
  case save_mode::previous_statement:
    if(!result.resume->statement) throw std::runtime_error{"first parser statement has no preceding save position"};
    --result.resume->statement;
    break;
  case save_mode::current_statement: break;
  case save_mode::following_statement:
    if(!successor()) throw std::runtime_error{"last parser statement has no following save position"};
    result.resume = successor();
    break;
  default: throw std::runtime_error{"unknown native save resume mode"};
  }
  return result;
}
void runtime::restore(saved_state const &saved) {
  // Validate the entire file state before changing borrowed campaign variables or live control flow.
  encode_save(source,saved);
  for(auto const &variable : source.variables) state[variable.name] = saved.values.at(variable.name);
  callers = saved.callers;
  reserved = saved.reserved;
  if(saved.resume) cursor = saved.resume;
}

} // namespace tfx::presentation
