#include "vector_hud.h"
#include "render/view_settings.h"
#include <cmath>
#include <numbers>
#include <utility>

namespace tfx::cockpit {
void vector_hud::begin(vec2<int> const &dimensions) {
  size=vec2<int>{dimensions};scale=std::min(size.x/320.0,size.y/200.0);
  offset={(size.x-320*scale)/2,(size.y-200*scale)/2};
  ink.assign(render::framebuffer::pixel_count(size),0);painted=false;glass.reset();
}
void vector_hud::attach_to_glass(render::camera const &view, vec3d const &model_position, mat3d const &rotation) {
  glass.emplace(glass_projection{view,mat3d{rotation},{}});
  std::array<vec3d,6> points;
  vec3d centre{0,0,0};
  for(size_t i=0;i<points.size();++i) {
    points[i]=view.transform(model_position+rotation*glass_vertices[i]);centre+=points[i];
  }
  for(size_t i=0;i<points.size();++i) {
    auto normal{points[i].cross(points[(i+1)%points.size()])};
    if(normal.dot(centre)<0) normal=-normal;
    glass->edges[i]=normal/normal.length();
  }
}
vec3d vector_hud::glass_ray(vec2d const &point) const {
  auto const local{(point-glass_origin)/glass_focal_distance};
  auto const world{glass->rotation*vec3d{local.x,local.y,1}};
  return {world.dot(glass->view.right),world.dot(glass->view.down),world.dot(glass->view.forward)};
}
bool vector_hud::on_glass(vec2d const &pixel) const {
  auto const local{(pixel-glass->view.projection_origin)/glass->view.focal_distance};
  vec3d const ray{local.x,local.y,1};
  return std::ranges::all_of(glass->edges,[&](auto const &edge){ return edge.dot(ray)>=0; });
}
void vector_hud::stroke(vec2d const &from, vec2d const &to, vec2<int> const &low, vec2<int> const &high, double const width) {
  vec2d a{offset+(from+vec2d{.5,.5})*scale}, b{offset+(to+vec2d{.5,.5})*scale};
  double stroke_scale{scale};
  vec2<int> clip_low{std::max(0,static_cast<int>(std::ceil(offset.x+low.x*scale-.5))),std::max(0,static_cast<int>(std::ceil(offset.y+low.y*scale-.5)))};
  vec2<int> clip_high{std::min(size.x-1,static_cast<int>(std::ceil(offset.x+(high.x+1)*scale-.5))-1),std::min(size.y-1,static_cast<int>(std::ceil(offset.y+(high.y+1)*scale-.5))-1)};
  if(glass) {
    double first{0},last{1};
    auto const clip{[&](double begin,double end) {
      if(begin<0 && end<0) return false;
      if(begin<0) first=std::max(first,begin/(begin-end));
      if(end<0) last=std::min(last,begin/(begin-end));
      return first<=last;
    }};
    for(unsigned axis=0;axis<2;++axis) {
      if(!clip(from[axis]-low[axis],to[axis]-low[axis]) || !clip(high[axis]-from[axis],high[axis]-to[axis])) return;
    }
    auto const start{glass_ray(from+vec2d{.5,.5})},end{glass_ray(to+vec2d{.5,.5})};
    if(!clip(start.z-1e-5,end.z-1e-5)) return;
    for(auto const &edge:glass->edges) if(!clip(start.dot(edge),end.dot(edge))) return;
    // Clip before dividing: at a side-on head angle, off-screen rays can
    // approach zero depth. Do not turn those into huge raster coordinates.
    double const focal{glass->view.focal_distance};
    auto const &origin{glass->view.projection_origin};
    for(unsigned axis=0;axis<2;++axis) {
      if(!clip(start[axis]*focal+start.z*(origin[axis]+1),end[axis]*focal+end.z*(origin[axis]+1))) return;
      if(!clip(-start[axis]*focal+start.z*(size[axis]+1-origin[axis]),-end[axis]*focal+end.z*(size[axis]+1-origin[axis]))) return;
    }
    auto const ray_a{start+(end-start)*first},ray_b{start+(end-start)*last};
    a=glass->view.project(ray_a);b=glass->view.project(ray_b);
    stroke_scale=glass->view.focal_distance/glass_focal_distance/std::min(ray_a.z,ray_b.z);
    clip_low={0,0};clip_high=size-vec2<int>{1,1};
  }
  auto const delta{b-a};double const length2{delta.dot(delta)}, radius{std::max(1.0,width*stroke_scale)/2};
  int const x0{std::max(clip_low.x,static_cast<int>(std::floor(std::min(a.x,b.x)-radius-1)))};
  int const x1{std::min(clip_high.x,static_cast<int>(std::ceil(std::max(a.x,b.x)+radius)))};
  int const y0{std::max(clip_low.y,static_cast<int>(std::floor(std::min(a.y,b.y)-radius-1)))};
  int const y1{std::min(clip_high.y,static_cast<int>(std::ceil(std::max(a.y,b.y)+radius)))};
  for(int y=y0;y<=y1;++y) {
    int first{x0},last{x1};
    if(delta.y!=0) {
      // Only visit the strip around this row's part of the stroke. A banked
      // horizon previously tested every pixel in its large bounding rectangle.
      double const reach{radius+.5};
      double const t0{std::clamp((y+.5-reach-a.y)/delta.y,0.0,1.0)};
      double const t1{std::clamp((y+.5+reach-a.y)/delta.y,0.0,1.0)};
      double const p0{a.x+delta.x*t0},p1{a.x+delta.x*t1};
      first=std::max(first,static_cast<int>(std::floor(std::min(p0,p1)-reach))-1);
      last=std::min(last,static_cast<int>(std::ceil(std::max(p0,p1)+reach))+1);
    }
    for(int x=first;x<=last;++x) {
      vec2d const point{x+.5,y+.5};double const t{length2 ? std::clamp((point-a).dot(delta)/length2,0.0,1.0) : 0};
      double const coverage{std::clamp(radius+.5-(point-a-delta*t).length(),0.0,1.0)};
      if(glass && coverage>0 && !on_glass(point)) continue;
      auto &value{ink[static_cast<size_t>(y*size.x+x)]};
      value=std::max(value,static_cast<uint8_t>(std::lround(coverage*255)));painted|=value!=0;
    }
  }
}
void vector_hud::line(vec2d const &from, vec2d const &to, vec2<int> const &low, vec2<int> const &high, uint8_t const pattern) {
  if(pattern==255) { stroke(from,to,low,high,.65);return; }
  // Three-unit dashes follow the segment, independent of resolution and bank.
  auto const delta{to-from};double const length{delta.length()};
  if(!length) { stroke(from,to,low,high,.65);return; }
  for(double at=0;at<length;at+=5) stroke(from+delta*(at/length),from+delta*(std::min(at+3,length)/length),low,high,.65);
}
void vector_hud::circle(vec2d const &centre, double const radius, render::raster_clip const &clip, bool const dotted) {
  int const segments{std::max(32,static_cast<int>(std::ceil(2*std::numbers::pi*radius*scale/2)))};
  vec2<int> const high{clip.glyph_limit-vec2<int>{1,1}};
  auto const point{[&](double angle){return centre+vec2d{std::cos(angle),std::sin(angle)}*radius;}};
  if(dotted) {
    int const dots{static_cast<int>(std::ceil(2*std::numbers::pi*radius/4))};
    for(int i=0;i<dots;++i) { auto const p{point(i*2*std::numbers::pi/dots)};stroke(p,p,clip.glyph_minimum,high,.65); }
  } else for(int i=0;i<segments;++i) stroke(point(i*2*std::numbers::pi/segments),point((i+1)*2*std::numbers::pi/segments),clip.glyph_minimum,high,.65);
}
void vector_hud::pixel(vec2d const &point, render::raster_clip const &clip) { stroke(point,point,clip.glyph_minimum,clip.glyph_limit-vec2<int>{1,1},.65); }
void vector_hud::bitmap(std::span<uint8_t const> const pixels, vec2<int> const &dimensions, vec2d const &origin, render::raster_clip const &clip) {
  // The lettering keeps its authored stroke skeleton and advances. Connect
  // diagonal neighbours only where no orthogonal stroke joins the same dots.
  auto const set{[&](int x,int y){return x>=0 && y>=0 && x<dimensions.x && y<dimensions.y && pixels[static_cast<size_t>(y*dimensions.x+x)];}};
  auto const high{clip.glyph_limit-vec2<int>{1,1}};
  for(int y=0;y<dimensions.y;++y) for(int x=0;x<dimensions.x;++x) if(set(x,y)) {
    auto const p{origin+vec2d{static_cast<double>(x),static_cast<double>(y)}};
    bool linked{false};
    for(auto const &d : {vec2<int>{1,0},vec2<int>{0,1},vec2<int>{1,1},vec2<int>{-1,1}}) {
      if(!set(x+d.x,y+d.y) || (d.x && d.y && (set(x+d.x,y)||set(x,y+d.y)))) continue;
      stroke(p,p+vec2d{d},clip.glyph_minimum,high,.65);linked=true;
    }
    if(!linked) stroke(p,p,clip.glyph_minimum,high,.65);
  }
}
void vector_hud::glyph(std::span<uint8_t const> const rows, vec2d const &origin, render::raster_clip const &clip) {
  std::vector<uint8_t> pixels(rows.size()*8);
  for(size_t y=0;y<rows.size();++y) for(unsigned x=0;x<8;++x) pixels[y*8+x]=rows[y]&(128>>x);
  bitmap(pixels,{8,static_cast<int>(rows.size())},origin,clip);
}
void vector_hud::sprite(resources::indexed_image const &image, vec2d const &centre, render::raster_clip const &clip) {
  bitmap(image.pixels,vec2<int>{image.size},centre-vec2d{static_cast<double>(image.size.x/2),static_cast<double>(image.size.y/2)},clip);
}
void vector_hud::mask(resources::monochrome_image const &image, vec2<int> const &origin, render::raster_clip const &clip) {
  auto const at{[&](instrument_mask kind){return &image==&assets.masks[std::to_underlying(kind)];}};
  auto const centre{vec2d{origin}};
  auto const low{vec2<int>{clip.glyph_minimum}}, high{clip.glyph_limit-vec2<int>{1,1}};
  auto const segment{[&](vec2d const &a,vec2d const &b){line(centre+a,centre+b,low,high);}};
  if(at(instrument_mask::short_range_sight)||at(instrument_mask::medium_range_sight)||at(instrument_mask::long_range_sight)) {
    bool const large{!at(instrument_mask::long_range_sight)};
    vec2d const c{large ? 38.0 : 28.0,large ? 38.0 : 22.0};
    if(large) circle(centre+c,38,clip,at(instrument_mask::medium_range_sight));
    if(!at(instrument_mask::short_range_sight)) circle(centre+c,22,clip);
    segment(c+vec2d{-28,0},c+vec2d{-6,0});segment(c+vec2d{6,0},c+vec2d{28,0});pixel(centre+c,clip);return;
  }
  if(at(instrument_mask::strafe_sight)) {circle(centre+vec2d{10,8},8,clip);pixel(centre+vec2d{10,8},clip);return;}
  if(at(instrument_mask::impact_sight)) {circle(centre+vec2d{2,2},2,clip);return;}
  std::vector<uint8_t> pixels(static_cast<size_t>(image.size.x*image.size.y));
  for(unsigned y=0;y<image.size.y;++y) for(unsigned x=0;x<image.size.x;++x) pixels[y*image.size.x+x]=image.rows[y*((image.size.x+7)/8)+x/8]&(128>>(x%8));
  bitmap(pixels,vec2<int>{image.size},centre,clip);
}
void vector_hud::compose(render::framebuffer &target, std::span<uint8_t const> const occlusion) const {
  if(!painted) return;
  target.hud_coverage=ink;
  // Later panel/menu writes must cover the optical layer. The HUD itself is
  // above precipitation and the world, which were drawn before capture.
  if(occlusion.empty()) return;
  render::interface_raster const mapping{size};
  mapping.covered_spans(occlusion,[&](int row,int first,int last) {
    int const left{mapping.columns[first]},right{mapping.columns[last]};
    for(int y=mapping.rows[row];y<mapping.rows[row+1];++y)
      std::fill_n(target.hud_coverage.data()+y*size.x+left,right-left,uint8_t{0});
  });
}
} // namespace tfx::cockpit
