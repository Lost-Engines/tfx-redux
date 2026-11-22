#include "assets.h"
#include <stdexcept>

namespace tfx::world {

assets::assets(resources::archive const &archive)
  : game{archive},
    names{std::make_shared<std::vector<std::string> const>(model_names(game.read("ss/ssinfo.fn")))},
    colours{game.contains("coltab.dat") ? game.read("coltab.dat") : resources::bytes{}} {
}

render::colour_map const &assets::model_colours(uint8_t const row) const {
  return colours.row(row);
}

models::shape const &assets::shape(std::string const &name) {
  /// Cache decoded programs by their native model names; no extracted metadata is needed
  if(auto const found{shapes.find(name)}; found != shapes.end()) return found->second;
  return shapes.emplace(name, game.read("3/" + name + ".3")).first->second;
}

definition const &assets::object(std::string const &name) {
  if(auto const found{definitions.find(name)}; found != definitions.end()) return found->second;
  return definitions.emplace(name, definition{game.read("ss/" + name + ".ssd"), names}).first->second;
}

std::span<uint8_t const> assets::texture(models::texture_binding const &binding) {
  /// Bind original global pages or explicitly named raw pages; unresolved actor-bound textures remain absent
  std::string name;
  if(binding.source == models::texture_source::global) {
    name = "spr/global_" + std::to_string(binding.slot + 1) + ".tm";
  } else if(binding.name.starts_with('!')) {
    name = "spr/" + binding.name.substr(1) + ".tm";
  } else {
    return {};
  }
  if(auto const found{textures.find(name)}; found != textures.end()) return found->second;
  if(!game.contains(name)) return {};
  auto decoded{game.read(name)};
  if(decoded.size() != 256 * 192) throw std::runtime_error{"texture extent differs from native 256 by 192 page: " + name};
  return textures.emplace(name, std::move(decoded)).first->second;
}

} // namespace tfx::world
