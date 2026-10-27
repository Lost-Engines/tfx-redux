#pragma once

#include "render/camera.h"
#include "render/colour_remap.h"
#include "models/shape.h"

namespace tfx::render {

// Sparse depth queries for small point sprites. Polygon colours still use the
// authored painter's order; only points need to see across terrain tile sorts.
class point_visibility {
  struct vertex { vec2d position, uv; double inverse_depth; };
  struct surface_record {
    size_t begin, count;
    vec3d depth_plane;
    int mode, bottom;
    std::span<uint8_t const> texture;
    colour_map const *remap;
  };
  struct link { size_t surface; int next; };
  vec2<int> size{0,0}, grid{0,0};
  std::vector<vertex> vertices;
  std::vector<surface_record> surfaces;
  std::vector<int> heads;
  std::vector<link> links;
public:
  void begin(vec2<int> const &dimensions);
  void add(std::span<models::model_vertex const> polygon, camera const &view, int mode,
           std::span<uint8_t const> texture, colour_map const *remap);
  bool visible(vec2d const &point, double depth) const;
};

} // namespace tfx::render
