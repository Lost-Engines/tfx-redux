#include "definition.h"
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include "resources/binary.h"

namespace tfx::world {
namespace {

unsigned int operands(unsigned int const opcode, resources::binary_view const &view, size_t const at) {
  /// Native SSD dispatch layouts at 2.05 file 0xb537c; scripts stay little-endian
  if(opcode == 9 || (opcode >= 20 && opcode <= 27)) return 1 + view.le16(at + 2);
  if(opcode == 10) return 2 + view.le16(at + 2);
  switch(opcode) {
  case 0:
  case 16:
  case 19:
  case 33:
    return 0;
  case 1:
  case 2:
  case 4:
  case 7:
  case 15:
  case 17:
  case 18:
  case 54:
  case 56:
  case 59:
  case 60:
  case 62:
  case 70:
  case 73:
  case 83:
    return 1;
  case 67:
  case 69:
  case 3:
  case 6:
  case 8:
  case 11:
  case 46:
  case 61:
  case 63:
  case 66:
  case 68:
    return 2;
  case 38:
  case 39:
  case 40:
  case 41:
  case 42:
  case 43:
  case 44:
  case 45:
  case 37:
  case 57:
  case 58:
  case 72:
  case 74:
    return 3;
  case 78:
  case 79:
    return 4;
  case 84:
    return 5;
  case 5:
  case 12:
  case 13:
  case 47:
  case 48:
  case 85:
  case 86:
  case 87:
    return 6;
  case 28:
  case 29:
  case 35:
  case 51:
  case 52:
  case 53:
  case 82:
    return 7;
  case 14:
  case 31:
  case 49:
  case 50:
  case 64:
    return 8;
  case 55:
  case 65:
  case 80:
  case 81:
    return 9;
  default:
    throw std::runtime_error{"unknown scenery opcode " + std::to_string(opcode)};
  }
}

} // anonymous namespace

unsigned int instruction_operands(unsigned int const opcode, std::span<uint8_t const> const bytes, unsigned int const arguments) {
  return operands(opcode, resources::binary_view{bytes}, arguments);
}

definition::definition(std::span<uint8_t const> const source, std::span<std::string const> const names) : definition{source, std::make_shared<std::vector<std::string> const>(names.begin(), names.end())} {
}

definition::definition(std::span<uint8_t const> const source, std::shared_ptr<std::vector<std::string> const> catalog)
  : bytecode{source.begin(), source.end()},
    names{std::move(catalog)} {
  /// Recover known initial parts and intact draw calls; conditional runtime events are not guessed
  resources::binary_view const view{source};
  view.slice(0, 10);
  parts.resize(view.byte(1));
  flags = view.le16(4);
  bounding_radius = view.signed_le16(2);
  custom_bytes = view.byte(0);
  unsigned int const entry_count{view.byte(8)};
  unsigned int const anchor_count{view.byte(9)};
  for(unsigned int i{0}; i != anchor_count; ++i) anchors.emplace_back(view.signed_le16(10 + i * 6), view.signed_le16(12 + i * 6), view.signed_le16(14 + i * 6));
  auto const table{10 + anchor_count * 6};
  auto const entry{[&view, table, entry_count](unsigned int const index){
    if(index >= entry_count) throw std::runtime_error{"missing scenery entry"};
    auto const field{table + index * 2};
    return static_cast<unsigned int>(static_cast<int>(field) + view.signed_le16(field) * 2);
  }};
  for(unsigned int index{0}; index != entry_count; ++index) entries.push_back(entry(index));
  std::map<unsigned int, uint16_t> custom;
  auto const run{[&](auto const &self, unsigned int at, bool const initialise, unsigned int const depth)->void {
    if(depth > 32) throw std::runtime_error{"scenery helper nesting exceeds limit"};
    std::set<unsigned int> visited;
    for(unsigned int step{0}; step != 4096; ++step) {
      if(!visited.insert(at).second) return;
      auto const opcode{view.le16(at)};
      unsigned int const args{at + 2};
      unsigned int const count{operands(opcode, view, args)};
      view.slice(args, count * 2);
      auto const a{[&view, args](unsigned int const index){return view.le16(args + index * 2);}};
      if(opcode == 0) return;
      if(opcode == 1) {
        at = static_cast<unsigned int>(static_cast<int>(args) + math::signed_word(a(0)) * 2);
        continue;
      }
      if(opcode == 2) self(self, static_cast<unsigned int>(static_cast<int>(args) + math::signed_word(a(0)) * 2), initialise, depth + 1);
      if(initialise) {
        if(opcode == 3 || opcode == 5) {
          auto &part{parts.at(a(0))};
          if(a(1) >= names->size()) throw std::runtime_error{"scenery model index exceeds name table"};
          part.name = names->at(a(1));
          part.model_index = a(1);
          part.rotation_mode = opcode == 5 ? static_cast<uint8_t>(a(2)) : 0;
          if(opcode == 5) part.rotation = vec3<math::angle>{a(3), a(4), a(5)};
        }
        if(opcode == 9 || (opcode >= 20 && opcode <= 27)) {
          parameter_stream stream{
            .part{a(0)},
            .parameter{opcode == 9 ? 0u : opcode - 20u},
            .words{},
          };
          for(unsigned int i{2}; i != count; ++i) stream.words.push_back(a(i));
          std::erase_if(streams, [&stream](auto const &old){return old.part == stream.part && old.parameter == stream.parameter;});
          streams.push_back(std::move(stream));
        }
        if(opcode == 10) {
          for(unsigned int i{2}; i != count; ++i) custom[a(0) + i - 2] = a(i);
        }
        if(opcode == 11) custom[a(0)] = a(1);
        if(opcode == 37 && custom.contains(a(0))) {
          if(custom.at(a(0)) == a(1)) {
            at = static_cast<unsigned int>(static_cast<int>(args + 4) + math::signed_word(a(2)) * 2);
            continue;
          }
        } else if(opcode == 37 || (opcode >= 38 && opcode <= 45) || opcode == 58 || (opcode >= 66 && opcode <= 69)) throw std::runtime_error{"scenery initialiser needs unresolved runtime state"};
      } else if(opcode == 4 || opcode == 6 || opcode == 7 || opcode == 8 || opcode == 46 || opcode == 54 || opcode == 60 || opcode == 74) {
        unsigned int const slot{a(0)};
        if(slot < parts.size()) {
          vec3d anchor;
          if(opcode == 6 || opcode == 8 || opcode == 46 || opcode == 74) {
            if(a(1) < anchors.size()) anchor = anchors[a(1)];
          }
          auto const duplicate{std::ranges::any_of(draws, [slot, &anchor](auto const &draw){return draw.slot == slot && draw.anchor == anchor;})};
          if(!duplicate) {
            draws.push_back({
              .slot{slot},
              .anchor{vec3d{anchor}},
              .kind{opcode == 60 ? part_draw_kind::ground_shadow : part_draw_kind::model},
              .placement{opcode == 8 || opcode == 74 ? part_position::fixed_anchor : opcode == 6 || opcode == 46 ? part_position::integer_anchor : part_position::assembly_origin},
              .orientation{opcode == 46 || opcode == 54 ? part_orientation::world_axes : part_orientation::terrain_axes},
              .delivery{opcode == 4 || opcode == 6 ? part_delivery::immediate : part_delivery::queued},
              .part{parts.at(slot)},
            });
          }
        }
      }
      if(opcode == 14 || opcode == 31 || opcode == 49 || opcode == 50 || opcode == 64 || opcode == 65 || opcode == 80 || opcode == 81) return;
      at = args + count * 2;
    }
    throw std::runtime_error{"scenery instruction limit exceeded"};
  }};
  run(run, entry(0), true, 0);
  if(entry_count > 1) run(run, entry(1), false, 0);
}

std::vector<std::string> model_names(std::span<uint8_t const> const source) {
  /// Read the shipped ssinfo.fn authoring names used by SSD model-index operands
  std::vector<std::string> result;
  bool quoted{false};
  std::string name;
  for(auto const value : source) {
    if(value == '"') {
      if(quoted) {
        result.push_back(std::move(name));
        name.clear();
      }
      quoted = !quoted;
    } else if(quoted) name += static_cast<char>(value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value);
  }
  if(quoted || result.empty()) throw std::runtime_error{"invalid scenery model-name table"};
  return result;
}

} // namespace tfx::world
