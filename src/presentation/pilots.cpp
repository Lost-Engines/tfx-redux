#include "pilots.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace tfx::presentation {
namespace {
std::string lower(std::string source) {
  for(auto &letter : source) if(letter >= 'A' && letter <= 'Z') letter = static_cast<char>(letter + ('a' - 'A'));
  return source;
}
std::filesystem::path existing(std::filesystem::path const &directory, std::string const &name) {
  if(std::filesystem::exists(directory)) for(auto const &entry : std::filesystem::directory_iterator{directory}) {
    if(lower(entry.path().filename().string()) == lower(name)) return entry.path();
  }
  return directory / name;
}
void write_bytes(std::filesystem::path const &path, std::span<uint8_t const> const bytes) {
  std::ofstream output{path,std::ios::binary | std::ios::trunc};
  if(!output || !output.write(reinterpret_cast<char const*>(bytes.data()),static_cast<std::streamsize>(bytes.size()))) throw std::runtime_error{"cannot save " + path.string()};
}
} // anonymous namespace

pilot_assets::pilot_assets(std::span<uint8_t const> const executable) {
  resources::binary_view const view{executable};
  unsigned int constexpr data{973476};
  auto const string{[&](unsigned int const address){
    std::string result;
    for(unsigned int index{0}; index != 256; ++index) {
      auto const byte{view.byte(data + address + index)};
      if(!byte) return result;
      result.push_back(static_cast<char>(byte));
    }
    throw std::runtime_error{"unterminated original pilot string"};
  }};
  std::ranges::copy(view.slice(data + 0xab50,characters.size()),characters.begin());
  std::array<unsigned int,3> constexpr default_addresses{0x35ac,0x35c0,0x35dc};
  for(unsigned int index{0}; index != defaults.size(); ++index) defaults[index] = string(default_addresses[index]);
  for(unsigned int index{0}; index != 20; ++index) {
    unsigned int const record{data + 0x90f0 + index * 6};
    auto const pointer{view.le32(record)};
    if(!pointer) break;
    introduction.push_back({
      .text{string(pointer)},
      .colour{view.byte(record + 4)},
    });
  }
  for(unsigned int index{0}; index != ranks.size(); ++index) ranks[index] = string(view.le32(data + 0x90c0 + index * 4));
  for(unsigned int index{0}; index != statuses.size(); ++index) statuses[index] = string(view.le32(data + 0x90dc + index * 4));
  unsigned int constexpr arrow{data + 0x9054};
  unsigned int const frame{view.le16(arrow + 1)}, pixels{view.byte(arrow + frame) * 2u * view.byte(arrow + frame + 1)};
  auto images{resources::decode_sprites(view.slice(arrow,frame + 2 + pixels))};
  pointer = {
    .image{std::move(images.front())},
    .adjustment{4,4},
  };
}

name_entry::name_entry(pilot_assets const &assets, std::string fallback) : assets{assets}, fallback{std::move(fallback)} {
}
bool name_entry::press(uint8_t const scan, bool const control) {
  if(finished) return true;
  if(scan == 1 || scan == 0x1c) {
    if(scan == 1 || text.empty()) text = fallback;
    control_at_confirmation = control;
    finished = true;
  } else if(scan == 0x0e) {
    if(!text.empty()) text.pop_back();
  } else if(scan < assets.characters.size() && text.size() < edited_name_limit) {
    uint8_t character{assets.characters[scan]};
    if(character >= 'a' && character <= 'z') character = static_cast<uint8_t>(character - ('a' - 'A'));
    if((character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') || character == ' ') text.push_back(static_cast<char>(character));
  }
  return finished;
}
std::string const &name_entry::value() const {
  return text;
}
bool name_entry::qualified() const {
  return control_at_confirmation;
}

pilot_store::pilot_store(std::filesystem::path directory) : directory{std::move(directory)} {
}
std::filesystem::path pilot_store::path(unsigned int const slot, bool const log) const {
  if(slot >= pilot_slots) throw std::runtime_error{"pilot slot must be between zero and nine"};
  return existing(directory,"TFXSAV." + std::to_string(slot) + (log ? "L" : ""));
}
std::array<std::optional<saved_state>,pilot_slots> pilot_store::list(program const &schema) const {
  std::array<std::optional<saved_state>,pilot_slots> result;
  for(unsigned int slot{0}; slot != result.size(); ++slot) if(std::filesystem::exists(path(slot,false))) result[slot] = read(slot,false,schema);
  return result;
}
void pilot_store::write(unsigned int const slot, bool const log, program const &schema, saved_state const &saved) const {
  auto const bytes{encode_save(schema,saved)};
  std::filesystem::create_directories(directory);
  write_bytes(path(slot,log),bytes);
}
saved_state pilot_store::read(unsigned int const slot, bool const log, program const &schema) const {
  auto const filename{path(slot,log)};
  try { return decode_save(schema,resources::read_file(filename)); }
  catch(std::exception const &error) { throw std::runtime_error{"cannot load pilot " + filename.string() + ": " + error.what()}; }
}
score_table pilot_store::load_scores(score_table const &defaults) const {
  auto const file{existing(directory,"tfx.hs")};
  if(!std::filesystem::exists(file)) return defaults;
  if(auto const decoded{decode_scores(resources::read_file(file))}) return *decoded;
  // Native 0xa6aec repairs a failed checksum with its embedded table.
  save_scores(defaults);
  return defaults;
}
void pilot_store::save_scores(score_table const &scores) const {
  std::filesystem::create_directories(directory);
  write_bytes(existing(directory,"tfx.hs"),encode_scores(scores));
}
bool pilot_store::trained() const {
  auto const marker{existing(existing(directory,"WAYTXT"),"A")};
  if(!std::filesystem::exists(marker)) return false;
  auto const bytes{resources::read_file(marker)};
  return bytes.size() >= 2 && bytes[0] == 0x36 && bytes[1] == 0x17;
}
void pilot_store::complete_training() const {
  auto const folder{existing(directory,"WAYTXT")};
  std::filesystem::create_directories(folder);
  std::array<uint8_t,2> constexpr marker{0x36,0x17};
  write_bytes(existing(folder,"A"),marker);
}
void reset_pilot(program const &schema, variables &values) {
  for(auto const &variable : schema.variables) values[variable.name] = variable.text ? variable_value{std::string{}} : variable_value{uint32_t{0}};
}

} // namespace tfx::presentation
