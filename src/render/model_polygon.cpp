#include "integer_model_polygon.h"
#include "model_polygon.h"
#include "rounded_model_polygon.h"
#include <algorithm>
#include "integer_model_texture.h"
#include "integer_polygon.h"
#include "pattern_raster.h"

namespace tfx::render {

bool paint_model_polygon(framebuffer &output, models::primitive const &source, std::span<models::model_vertex const> const vertices, camera const &view, std::span<uint8_t const> const texture, colour_map const *const remap, uint16_t &seed) {
  if(source.kind != models::primitive_kind::polygon || view.projection != projection_mode::original_model) return false;
  uint8_t const index{static_cast<uint8_t>(source.colour)};
  uint8_t const ink{remap ? (*remap)[index] : index};
  if(!source.rounded && !texture.empty()) {
    static colour_map constexpr identity{[]{
      colour_map result;
      for(unsigned int i{0}; i != result.size(); ++i) result[i] = static_cast<uint8_t>(i);
      return result;
    }()};
    return integer_model_texture(output,vertices,view,texture,remap ? *remap : identity);
  }
  if(source.rounded && texture.empty()) return rounded_model_polygon(output,vertices,view,ink,source.mode,seed);
  if(!source.rounded && texture.empty()) {
    if(source.mode == -1) return integer_model_polygon(output,vertices,view,ink,remap);
    if(std::ranges::all_of(vertices,[](auto const &vertex){ return vertex.shade < 0; })) {
      if(auto const screen{project_model_face(vertices,view)}) {
        pattern_polygon(output,seed,integer_polygon_spans(*screen,view.viewport_minimum,view.viewport_maximum),ink,source.mode);
        return true;
      }
    }
  }
  return false;
}

} // namespace tfx::render
