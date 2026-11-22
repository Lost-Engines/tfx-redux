#include "rules.h"
#include <algorithm>
#include <bit>
#include <charconv>
#include <optional>
#include <stdexcept>
#include <utility>
#include "math/native.h"

namespace tfx::missions {
namespace {

class parser {
private:
  std::vector<std::string> words;
  unsigned int cursor{0};

  std::string_view peek() const {
    return cursor == words.size() ? std::string_view{} : words[cursor];
  }
  std::string take() {
    if(cursor == words.size()) throw std::runtime_error{"truncated mission FINISH statement"};
    return words[cursor++];
  }
  operand read_operand() {
    auto value{take()};
    if(std::ranges::all_of(value, [](char const c){return c >= '0' && c <= '9';})) {
      int32_t number{0};
      auto const parsed{std::from_chars(value.data(), value.data() + value.size(), number)};
      if(parsed.ec == std::errc::result_out_of_range) throw std::runtime_error{"mission operand exceeds a native dword"};
      return {
        .value{number},
      };
    }
    return {
      .value{std::move(value)},
    };
  }
  predicate read_predicate() {
    auto const word{peek()};
    if(word == "alive" || word == "dead") {
      bool const alive{take() == "alive"};
      if(take() != "targ") throw std::runtime_error{"mission target test requires targ#N"};
      auto const value{read_operand()};
      auto const *index{std::get_if<int32_t>(&value.value)};
      if(!index || *index < 1) throw std::runtime_error{"mission target number must be positive"};
      return {
        .value{target_test{
          .index{static_cast<unsigned int>(*index - 1)},
          .expected{alive ? target_life::alive : target_life::dead},
        }},
      };
    }
    auto first{read_operand()};
    auto const operation{take()};
    comparison compare;
    if(operation == "<") {
      compare = comparison::less;
    } else if(operation == ">") {
      compare = comparison::greater;
    } else if(operation == "==") {
      compare = comparison::equal;
    } else {
      throw std::runtime_error{"unsupported mission comparison: " + operation};
    }
    return {
      .value{numeric_test{
        .first{std::move(first)},
        .operation{compare},
        .second{read_operand()},
      }},
    };
  }
  std::vector<statement> body() {
    if(peek() != "{") {
      std::vector<statement> result;
      result.push_back(read_statement());
      return result;
    }
    take();
    std::vector<statement> result;
    while(peek() != "}") result.push_back(read_statement());
    take();
    return result;
  }
  statement read_statement() {
    auto const command{take()};
    if(command == "return") {
      auto value{take()};
      int32_t number{0};
      auto const *begin{value.data()};
      if(value.starts_with('+')) ++begin;
      auto const parsed{std::from_chars(begin, value.data() + value.size(), number)};
      if(parsed.ec == std::errc::result_out_of_range) throw std::runtime_error{"mission return exceeds a native dword"};
      return {
        .value{return_value{
          .value{number},
        }},
      };
    }
    if(command == "setpar") {
      auto variable{take()};
      auto const token{take()};
      assignment_operation operation;
      if(token == "==") {
        operation = assignment_operation::set;
      } else if(token == "+=") {
        operation = assignment_operation::add;
      } else if(token == "-=") {
        operation = assignment_operation::subtract;
      } else {
        throw std::runtime_error{"unsupported mission assignment: " + token};
      }
      return {
        .value{assignment{
          .variable{std::move(variable)},
          .operation{operation},
          .value{read_operand()},
        }},
      };
    }
    if(command == "if") {
      condition test{
        .tests{read_predicate()},
        .operations{},
      };
      while(peek() == "and" || peek() == "or") {
        test.operations.push_back(take() == "and" ? logical_operation::conjunction : logical_operation::disjunction);
        test.tests.push_back(read_predicate());
      }
      bool const braced{peek() == "{"};
      auto when_true{body()};
      std::vector<statement> when_false;
      if(peek() == "else") {
        take();
        when_false = body();
      }
      return {
        .value{conditional{
          .test{std::move(test)},
          .braced{braced},
          .when_true{std::move(when_true)},
          .when_false{std::move(when_false)},
        }},
      };
    }
    throw std::runtime_error{"unsupported mission FINISH command: " + command};
  }

public:
  explicit parser(std::string_view source) {
    /// Token boundaries retain native parentheses/hash/comma separators and standalone operators
    bool finish{false};
    while(!source.empty()) {
      auto const newline{source.find('\n')};
      auto const count{newline == source.npos ? source.size() : newline + 1};
      auto line{source.substr(0, count)};
      source.remove_prefix(count);
      if(auto const comment{line.find(';')}; comment != line.npos) line = line.substr(0, comment);
      auto const first{line.find_first_not_of(" \t\r\n")};
      if(first == line.npos) continue;
      line.remove_prefix(first);
      if(!finish) {
        if(line.starts_with("FINISH:")) finish = true;
        continue;
      }
      if(line.starts_with("END:")) break;
      while(!line.empty()) {
        auto const begin{line.find_first_not_of(" \t\r\n,#()")};
        if(begin == line.npos) break;
        line.remove_prefix(begin);
        auto const end{line.find_first_of(" \t\r\n,#(){}")};
        auto const length{end == line.npos ? line.size() : end == 0 ? 1 : end};
        words.emplace_back(line.substr(0, length));
        line.remove_prefix(length);
      }
    }
    if(!finish) throw std::runtime_error{"mission lacks a FINISH section"};
  }
  std::vector<statement> compile() {
    std::vector<statement> result;
    while(cursor != words.size()) result.push_back(read_statement());
    return result;
  }
};

int32_t variable(variables const &campaign, std::string const &name) {
  auto const found{campaign.find(name)};
  if(found == campaign.end()) throw std::runtime_error{"mission references an undeclared campaign variable: " + name};
  return found->second;
}
int32_t evaluate(operand const &input, result_state const &state, variables const &campaign) {
  if(auto const *value{std::get_if<int32_t>(&input.value)}) return *value;
  auto const &name{std::get<std::string>(input.value)};
  if(name == "pairscore") return state.personal_air_score;
  if(name == "eairscore") return state.enemy_air_score;
  if(name == "fairscore") return state.friendly_air_score;
  if(name == "eplanes_down") return state.enemy_aircraft_down;
  if(name == "fplanes_down") return state.friendly_aircraft_down;
  if(name == "completed") return state.completed;
  if(name == "targets_dead") return state.targets_destroyed;
  if(name == "emobile_down") return state.enemy_mobile_down;
  return variable(campaign, name);
}
bool evaluate(predicate const &input, result_state const &state, variables const &campaign) {
  if(auto const *target{std::get_if<target_test>(&input.value)}) return state.targets.at(target->index) == target->expected;
  auto const &test{std::get<numeric_test>(input.value)};
  auto const first{math::signed_word(static_cast<uint16_t>(evaluate(test.first, state, campaign)))};
  auto const second{math::signed_word(static_cast<uint16_t>(evaluate(test.second, state, campaign)))};
  switch(test.operation) {
  case comparison::less:
    return first < second;
  case comparison::greater:
    return first > second;
  case comparison::equal:
    return first == second;
  }
  throw std::logic_error{"unknown mission comparison"};
}
bool evaluate(condition const &test, result_state const &state, variables const &campaign) {
  /// Native boolean terms evaluate from left to right without precedence or short-circuiting
  bool result{evaluate(test.tests.at(0), state, campaign)};
  for(unsigned int i{0}; i != test.operations.size(); ++i) {
    bool const value{evaluate(test.tests.at(i + 1), state, campaign)};
    result = test.operations[i] == logical_operation::conjunction ? result && value : result || value;
  }
  return result;
}
int32_t add(int32_t const first, int32_t const second) {
  return std::bit_cast<int32_t>(static_cast<uint32_t>(first) + static_cast<uint32_t>(second));
}
std::optional<int32_t> execute(std::vector<statement> const &statements, result_state const &state, variables &campaign) {
  for(unsigned int index{0}; index != statements.size(); ++index) {
    auto const &statement{statements[index]};
    if(auto const *result{std::get_if<return_value>(&statement.value)}) return result->value;
    if(auto const *branch{std::get_if<conditional>(&statement.value)}) {
      auto const result{execute(evaluate(branch->test, state, campaign) ? branch->when_true : branch->when_false, state, campaign)};
      if(result) return result;
      // native 0x7aafc reuses the looked-ahead line: a following opening brace
      // is skipped even without ELSE (observable in the nested IFs of miss605)
      if(branch->braced && index + 1 != statements.size()) {
        auto const *next{std::get_if<conditional>(&statements[index + 1].value)};
        if(next && next->braced) ++index;
      }
      continue;
    }
    auto const &change{std::get<assignment>(statement.value)};
    variable(campaign, change.variable);
    int32_t const value{evaluate(change.value, state, campaign)};
    auto &destination{campaign.at(change.variable)};
    switch(change.operation) {
    case assignment_operation::set:
      destination = value;
      break;
    case assignment_operation::add:
      destination = add(destination, value);
      break;
    case assignment_operation::subtract:
      destination = std::bit_cast<int32_t>(static_cast<uint32_t>(destination) - static_cast<uint32_t>(value));
      break;
    }
  }
  return std::nullopt;
}

} // anonymous namespace

finish_program::finish_program(std::string_view const source) : statements{parser{source}.compile()} {
}

void finish_program::execute(result_state const &results, variables &campaign) const {
  /// Original 0x7ae80 applies common statistics before FINISH, then counts a nonzero mission result
  for(auto const name : {"a2akills", "a2gkills", "score", "num_hours", "num_miss", "miss_val"}) variable(campaign, name);
  campaign.at("a2akills") = add(campaign.at("a2akills"), results.credited_air_kills);
  campaign.at("a2gkills") = add(add(campaign.at("a2gkills"), results.credited_ground_kills), results.credited_targets);
  campaign.at("score") = add(campaign.at("score"), results.personal_air_score);
  campaign.at("num_hours") = add(campaign.at("num_hours"), 1);
  if(auto const result{missions::execute(statements, results, campaign)}) campaign.at("miss_val") = *result;
  if(campaign.at("miss_val") != 0) campaign.at("num_miss") = add(campaign.at("num_miss"), 1);
}

} // namespace tfx::missions
