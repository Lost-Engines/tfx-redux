#include "point_visibility.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace tfx::render {
namespace { int constexpr bin_size{64}; }
void point_visibility::begin(vec2<int> const &dimensions) {
  size = dimensions;grid = (size + vec2<int>{bin_size-1,bin_size-1}) / bin_size;
  heads.assign(static_cast<size_t>(grid.x * grid.y),-1);
  vertices.clear();surfaces.clear();links.clear();
}
void point_visibility::add(std::span<models::model_vertex const> const polygon, camera const &view,
                           int const mode, std::span<uint8_t const> const texture, colour_map const *const remap) {
  // Procedural smoke/cloud stipple and logical colour operations are not solid
  // occluders. Their ordinary painter pass remains responsible for their ink.
  if(polygon.size()<3 || mode==4 || mode>=10) return;
  size_t const begin{vertices.size()};
  vec2d minimum{static_cast<double>(size.x),static_cast<double>(size.y)}, maximum{0,0};
  for(auto const &v : polygon) {
    auto const p{view.project(v.position,v.projection_scale)};
    vertices.push_back({vec2d{p},vec2d{v.uv},1/v.position.z});
    minimum.x=std::min(minimum.x,p.x);minimum.y=std::min(minimum.y,p.y);
    maximum.x=std::max(maximum.x,p.x);maximum.y=std::max(maximum.y,p.y);
  }
  auto const &a{vertices[begin]};
  vec3d plane{0,0,0};bool found{false};
  for(size_t i{1}; i+1<polygon.size(); ++i) {
    auto const &b{vertices[begin+i]}, &c{vertices[begin+i+1]};
    auto const ab{b.position-a.position}, ac{c.position-a.position};
    double const determinant{ab.x*ac.y-ab.y*ac.x};
    if(std::abs(determinant)<1e-10) continue;
    double const db{b.inverse_depth-a.inverse_depth}, dc{c.inverse_depth-a.inverse_depth};
    plane.x=(db*ac.y-dc*ab.y)/determinant;
    plane.y=(ab.x*dc-ac.x*db)/determinant;
    plane.z=a.inverse_depth-plane.x*a.position.x-plane.y*a.position.y;
    found=true;break;
  }
  if(!found) { vertices.resize(begin);return; }
  size_t const index{surfaces.size()};
  surfaces.push_back({begin,polygon.size(),vec3d{plane},mode,static_cast<int>(std::lround(maximum.y)),texture,remap});
  int const x0{std::clamp(static_cast<int>(std::floor(minimum.x))/bin_size,0,grid.x-1)};
  int const x1{std::clamp(static_cast<int>(std::floor(maximum.x))/bin_size,0,grid.x-1)};
  int const y0{std::clamp(static_cast<int>(std::floor(minimum.y))/bin_size,0,grid.y-1)};
  int const y1{std::clamp(static_cast<int>(std::floor(maximum.y))/bin_size,0,grid.y-1)};
  for(int y{y0}; y<=y1; ++y) for(int x{x0}; x<=x1; ++x) {
    auto &head{heads[static_cast<size_t>(y*grid.x+x)]};
    links.push_back({index,head});head=static_cast<int>(links.size()-1);
  }
}
bool point_visibility::visible(vec2d const &ray, double const depth) const {
  vec2<int> const point{ray};
  if(point.x<0 || point.y<0 || point.x>=size.x || point.y>=size.y) return false;
  double const x{ray.x}, y{ray.y}, inverse{1/depth};
  for(int link_index{heads[static_cast<size_t>((point.y/bin_size)*grid.x+point.x/bin_size)]}; link_index>=0; link_index=links[static_cast<size_t>(link_index)].next) {
    auto const &surface{surfaces[links[static_cast<size_t>(link_index)].surface]};
    auto const &p{surface.depth_plane};
    if(p.x*x+p.y*y+p.z <= inverse*1.0001) continue; // Coplanar runway lights remain visible.
    if(surface.mode==0 && ((point.x+point.y)&1)==0) continue;
    if(surface.mode==1 && (point.x&3)!=(point.y&1 ? 3 : 1)) continue;
    if(surface.mode==7 && (point.y&1)) continue;
    bool inside{false};
    double left{-std::numeric_limits<double>::infinity()}, right{std::numeric_limits<double>::infinity()};
    vec2d left_uv{0,0}, right_uv{0,0};
    auto const verts{std::span{vertices}.subspan(surface.begin,surface.count)};
    for(size_t i{0}; i<verts.size(); ++i) {
      auto const &a{verts[i]}, &b{verts[(i+1)%verts.size()]};
      if((a.position.y>y)==(b.position.y>y)) continue;
      double const t{(y-a.position.y)/(b.position.y-a.position.y)};
      double const edge{a.position.x+(b.position.x-a.position.x)*t};
      auto const uv{a.uv+(b.uv-a.uv)*t};
      if(edge>x) { inside=!inside;if(edge<right) { right=edge;right_uv=uv; } }
      else if(edge>left) { left=edge;left_uv=uv; }
    }
    if(!inside) continue;
    int const first_byte{static_cast<int>(std::floor(left/4))};
    if(surface.mode==2) {
      if(point.y&1) continue;
      int const phase{(first_byte+(((surface.bottom&~1)-point.y)/2)+(surface.bottom&1))&1};
      if((point.x&7)!=phase*4+3) continue;
    }
    if(surface.mode==3) {
      int const row{surface.bottom-point.y};
      if(row%3) continue;
      int const offset{(surface.bottom&1 ? 0 : 2)+((row/3)&1)*2};
      int const start{(first_byte+(offset&3))*4+3};
      if(point.x<start || (point.x-start)%16) continue;
    }
    if(!surface.texture.empty()) {
      auto const uv{left_uv+(right_uv-left_uv)*((x-left)/(right-left))};
      int const u{std::clamp(static_cast<int>(uv.x),0,255)}, v{std::clamp(static_cast<int>(uv.y),0,191)};
      auto const colour{surface.texture[static_cast<size_t>(v*256+u)]};
      if(!colour || !(surface.remap ? (*surface.remap)[colour] : colour)) continue;
    }
    return false;
  }
  return true;
}
} // namespace tfx::render
