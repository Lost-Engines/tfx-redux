#include "audio.h"
#include <stdexcept>
#include <string>

namespace tfx::configuration {

music_engine parse_music_engine(std::string_view const name) {
  std::string text{name};
  for(auto &c : text) if(c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  if(text == "none") return music_engine::none;
  if(text == "fm" || text == "soundblaster-fm") return music_engine::fm;
  if(text == "lapc" || text == "roland-lapc") return music_engine::lapc;
  if(text == "scc1a" || text == "roland-scc1a") return music_engine::scc1a;
  if(text == "sc55" || text == "roland-sc55") return music_engine::sc55;
  throw std::invalid_argument{"music must be none, soundblaster-fm, roland-lapc, roland-scc1a or roland-sc55"};
}
std::string_view music_engine_name(music_engine const engine) {
  switch(engine) {
  case music_engine::none: return "none";
  case music_engine::fm: return "soundblaster-fm";
  case music_engine::lapc: return "roland-lapc";
  case music_engine::scc1a: return "roland-scc1a";
  case music_engine::sc55: return "roland-sc55";
  }
  throw std::invalid_argument{"unknown music engine"};
}
audio_settings resolve_audio(uint8_t const native_music, audio_preferences const &preferences) {
  audio_settings result;
  switch(native_music) {
  case 1: result = {music_engine::fm, true}; break;
  case 2: result.engine = music_engine::lapc; break;
  case 3: result.engine = music_engine::fm; break;
  case 4: result.engine = music_engine::scc1a; break;
  default: break;
  }
  if(preferences.engine) result.engine = *preferences.engine;
  if(preferences.cd_music) result.cd_music = *preferences.cd_music;
  return result;
}

} // namespace tfx::configuration
