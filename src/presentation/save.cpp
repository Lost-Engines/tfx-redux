#include "save.h"
#include <algorithm>
#include <stdexcept>

namespace tfx::presentation {
namespace {
void validate(location const position, program const &schema) {
  if(position.function >= schema.functions.size() || position.statement >= schema.functions[position.function].statements.size()) throw std::runtime_error{"saved parser position leaves its program"};
}
void validate_schema(program const &schema) {
  if(schema.variables.size() > saved_variable_slots) throw std::runtime_error{"native save schema exceeds thirty variable slots"};
}
} // anonymous namespace

saved_state decode_save(program const &schema, std::span<uint8_t const> const source) {
  // Native 0xb3c14 reads fixed-order variables, a word stack depth, return pairs, then an optional resume pair.
  validate_schema(schema);
  resources::binary_view const view{source};
  saved_state result;
  unsigned int at{0};
  for(auto const &variable : schema.variables) {
    if(variable.text) {
      auto const field{view.slice(at,saved_string_bytes)};
      auto const end{std::ranges::find(field,uint8_t{0})};
      if(end == field.end()) throw std::runtime_error{"saved pilot string has no terminator"};
      result.values[variable.name] = std::string{field.begin(),end};
      at += saved_string_bytes;
    } else { result.values[variable.name] = view.le32(at);at += 4; }
  }
  for(unsigned int index{static_cast<unsigned int>(schema.variables.size())}; index != saved_variable_slots; ++index) { result.reserved.push_back(view.le32(at));at += 4; }
  unsigned int const depth{view.le16(at)};
  at += 2;
  if(depth > saved_return_limit) throw std::runtime_error{"saved parser return stack exceeds its native capacity"};
  auto const position{[&]{
    location const value{
      .function{view.le16(at)},
      .statement{view.le16(at + 2)},
    };
    at += 4;
    return value;
  }};
  for(unsigned int index{0}; index != depth; ++index) {
    auto const caller{position()};
    validate(caller,schema);
    result.callers.push_back(caller);
  }
  auto const resume{position()};
  if(resume.function != 0xffff) { validate(resume,schema);result.resume = resume; }
  if(at != source.size()) throw std::runtime_error{"saved pilot has trailing bytes"};
  return result;
}

resources::bytes encode_save(program const &schema, saved_state const &state) {
  validate_schema(schema);
  if(state.callers.size() > saved_return_limit || state.reserved.size() != saved_variable_slots - schema.variables.size()) throw std::runtime_error{"invalid native save storage size"};
  resources::bytes result;
  auto const word{[&](uint16_t const value){ result.push_back(static_cast<uint8_t>(value));result.push_back(static_cast<uint8_t>(value >> 8)); }};
  auto const dword{[&](uint32_t const value){ word(static_cast<uint16_t>(value));word(static_cast<uint16_t>(value >> 16)); }};
  for(auto const &variable : schema.variables) {
    auto const found{state.values.find(variable.name)};
    if(found == state.values.end()) throw std::runtime_error{"save is missing parser variable: " + variable.name};
    if(variable.text) {
      auto const string{std::get_if<std::string>(&found->second)};
      if(!string || string->size() >= saved_string_bytes || string->find('\0') != std::string::npos) throw std::runtime_error{"saved pilot string exceeds its native field"};
      result.insert(result.end(),string->begin(),string->end());
      // DOS copied the whole field, including stale bytes after the terminator. New files initialise that padding.
      result.insert(result.end(),saved_string_bytes - string->size(),uint8_t{0});
    } else {
      auto const number{std::get_if<uint32_t>(&found->second)};
      if(!number) throw std::runtime_error{"saved numeric variable contains text"};
      dword(*number);
    }
  }
  for(auto const value : state.reserved) dword(value);
  word(static_cast<uint16_t>(state.callers.size()));
  auto const position{[&](location const value){
    validate(value,schema);
    word(static_cast<uint16_t>(value.function));
    word(static_cast<uint16_t>(value.statement));
  }};
  for(auto const caller : state.callers) position(caller);
  if(state.resume) position(*state.resume);
  else { word(0xffff);word(0xffff); }
  return result;
}

} // namespace tfx::presentation
