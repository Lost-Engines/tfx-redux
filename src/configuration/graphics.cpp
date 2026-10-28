#include "graphics.h"
#include <array>
#include <charconv>
#include <chrono>
#include <format>
#include <fstream>
#include <sstream>

namespace tfx::configuration {
namespace {
std::array<std::string_view,5> constexpr names{"RESOLUTION","WINDOW_SCALE","FOV","DRAW_DISTANCE","LOD_SCALE"};
std::string_view trim(std::string_view text) {
  auto const begin{text.find_first_not_of(" \t\r\n")};
  if(begin==text.npos) return {};
  return text.substr(begin,text.find_last_not_of(" \t\r\n")-begin+1);
}
std::string lower(std::string_view text) {
  std::string result{text};
  for(auto &c:result) if(c>='A' && c<='Z') c=static_cast<char>(c-'A'+'a');
  return result;
}
unsigned int integer(std::string_view text) {
  unsigned int result{0};auto const [end,error]{std::from_chars(text.data(),text.data()+text.size(),result)};
  if(error!=std::errc{} || end!=text.data()+text.size()) throw std::invalid_argument{"Enter a whole number."};
  return result;
}
double number(std::string_view text) {
  double result{0};auto const [end,error]{std::from_chars(text.data(),text.data()+text.size(),result)};
  if(error!=std::errc{} || end!=text.data()+text.size() || !std::isfinite(result)) throw std::invalid_argument{"Enter a finite number."};
  return result;
}
}
void set_graphics_value(graphics_settings &settings, graphics_field const field, std::string_view value) {
  auto next{settings};value=trim(value);
  switch(field) {
    case graphics_field::resolution: {
      auto const text{lower(value)};
      if(text=="auto") { next.video.automatic_resolution=true;break; }
      auto const separator{text.find('x')};
      if(separator==text.npos) throw std::invalid_argument{"Use AUTO or WIDTHxHEIGHT."};
      auto const width{integer(std::string_view{text}.substr(0,separator))},height{integer(std::string_view{text}.substr(separator+1))};
      if(width==0 || height==0 || width>16384 || height>16384) throw std::invalid_argument{"Size must be 1 to 16384 per side."};
      if(static_cast<uint64_t>(width)*height>64*1024*1024) throw std::invalid_argument{"Size must be at most 64M pixels."};
      next.video.resolution={static_cast<int>(width),static_cast<int>(height)};next.video.automatic_resolution=false;
      break;
    }
    case graphics_field::window_scale: {
      auto const text{lower(value)};auto const parsed{text=="fit" ? 0u : integer(value)};
      if(parsed>16) throw std::invalid_argument{"Window scale must be 0 to 16."};
      next.window_scale=parsed;break;
    }
    case graphics_field::field_of_view: {
      auto const parsed{number(value)};
      if(parsed<10 || parsed>170) throw std::invalid_argument{"FOV must be 10 to 170 degrees."};
      next.video.field_of_view=parsed;break;
    }
    case graphics_field::draw_distance: {
      auto const parsed{lower(value)=="infinite" ? 0u : integer(value)};
      if(parsed>10000) throw std::invalid_argument{"Distance must be 0 to 10000."};
      next.video.draw_distance=parsed;break;
    }
    case graphics_field::lod_scale: {
      auto const parsed{number(value)};
      if(parsed<0 || parsed>100) throw std::invalid_argument{"LOD scale must be 0 to 100."};
      next.video.lod_scale=parsed;break;
    }
  }
  settings=next;
}
std::string graphics_value(graphics_settings const &settings, graphics_field const field) {
  switch(field) {
    case graphics_field::resolution: return settings.video.automatic_resolution ? "AUTO" : std::format("{}x{}",settings.video.resolution.x,settings.video.resolution.y);
    case graphics_field::window_scale: return settings.window_scale ? std::to_string(settings.window_scale) : "FIT";
    case graphics_field::field_of_view: return std::format("{:.6g}",settings.video.field_of_view);
    case graphics_field::draw_distance: return settings.video.draw_distance ? std::to_string(settings.video.draw_distance) : "INFINITE";
    case graphics_field::lod_scale: return std::format("{:.6g}",settings.video.lod_scale);
  }
  throw std::invalid_argument{"Unknown graphics option."};
}
graphics_settings parse_graphics(std::string_view text) {
  if(text.size()>4096) throw std::runtime_error{"REDUX.CFG is too large"};
  graphics_settings result;
  while(!text.empty()) {
    auto const end{text.find('\n')};auto const line{trim(text.substr(0,end))};
    auto const equals{line.find('=')};
    if(!line.empty() && line.front()!='#' && equals!=line.npos) {
      auto const key{trim(line.substr(0,equals))};
      for(unsigned int i=0;i<names.size();++i) if(key==names[i]) set_graphics_value(result,static_cast<graphics_field>(i),line.substr(equals+1));
    }
    if(end==text.npos) break;
    text.remove_prefix(end+1);
  }
  return result;
}
graphics_settings load_graphics(std::filesystem::path const &directory) {
  auto const path{directory/graphics_filename};
  if(!std::filesystem::exists(path)) return {};
  std::ifstream input{path,std::ios::binary};
  if(!input) throw std::runtime_error{"Cannot read "+path.string()};
  std::array<char,4097> buffer{};input.read(buffer.data(),buffer.size());
  if(input.bad()) throw std::runtime_error{"Cannot read "+path.string()};
  return parse_graphics({buffer.data(),static_cast<size_t>(input.gcount())});
}
void save_graphics(std::filesystem::path const &directory, graphics_settings const &settings) {
  std::ostringstream text;
  text<<"# TFX Redux graphics. Command-line options override these at startup.\n";
  for(unsigned int i=0;i<names.size();++i) {
    auto const field{static_cast<graphics_field>(i)};
    // Persist full precision; the compact value shown in the menu is only a label.
    auto const value{field==graphics_field::field_of_view ? std::format("{}",settings.video.field_of_view) : field==graphics_field::lod_scale ? std::format("{}",settings.video.lod_scale) : graphics_value(settings,field)};
    auto checked{settings};set_graphics_value(checked,field,value);
    text<<names[i]<<'='<<value<<'\n';
  }
  auto const destination{directory/graphics_filename};auto temporary{destination};
  temporary += ".tmp-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  try {
    { std::ofstream output{temporary,std::ios::binary};output<<text.str();output.close();if(!output) throw std::runtime_error{"Cannot save "+destination.string()}; }
    std::filesystem::rename(temporary,destination);
  } catch(...) { std::error_code error;std::filesystem::remove(temporary,error);throw; }
}
} // namespace tfx::configuration
