#include "vector_hud.h"
#include <cmath>
#include <numbers>
#include <utility>

namespace tfx::cockpit {
void vector_hud::begin(vec2<int> const &dimensions) {
  size=vec2<int>{dimensions};scale=std::min(size.x/320.0,size.y/200.0);
  offset={(size.x-320*scale)/2,(size.y-200*scale)/2};
  ink.assign(render::framebuffer::pixel_count(size),0);painted=false;
}
void vector_hud::stroke(vec2d const &from, vec2d const &to, vec2<int> const &low, vec2<int> const &high, double const width) {
  auto const a{offset+(from+vec2d{.5,.5})*scale}, b{offset+(to+vec2d{.5,.5})*scale};
  auto const delta{b-a};double const length2{delta.dot(delta)}, radius{std::max(1.0,width*scale)/2};
  int const x0{std::max({0,static_cast<int>(std::ceil(offset.x+low.x*scale-.5)),static_cast<int>(std::floor(std::min(a.x,b.x)-radius-1))})};
  int const x1{std::min({size.x-1,static_cast<int>(std::ceil(offset.x+(high.x+1)*scale-.5))-1,static_cast<int>(std::ceil(std::max(a.x,b.x)+radius))})};
  int const y0{std::max({0,static_cast<int>(std::ceil(offset.y+low.y*scale-.5)),static_cast<int>(std::floor(std::min(a.y,b.y)-radius-1))})};
  int const y1{std::min({size.y-1,static_cast<int>(std::ceil(offset.y+(high.y+1)*scale-.5))-1,static_cast<int>(std::ceil(std::max(a.y,b.y)+radius))})};
  for(int y=y0;y<=y1;++y) for(int x=x0;x<=x1;++x) {
    vec2d const point{x+.5,y+.5};double const t{length2 ? std::clamp((point-a).dot(delta)/length2,0.0,1.0) : 0};
    double const coverage{std::clamp(radius+.5-(point-a-delta*t).length(),0.0,1.0)};
    auto &value{ink[static_cast<size_t>(y*size.x+x)]};
    value=std::max(value,static_cast<uint8_t>(std::lround(coverage*255)));painted|=value!=0;
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
  for(int y=0;y<size.y;++y) {
    int const row{static_cast<int>(std::floor((y-offset.y)/scale))};
    if(row<0||row>=200) continue;
    for(int x=0;x<size.x;++x) {
      int const column{static_cast<int>(std::floor((x-offset.x)/scale))};
      if(column>=0&&column<320&&occlusion[static_cast<size_t>(row*320+column)]) target.hud_coverage[static_cast<size_t>(y*size.x+x)]=0;
    }
  }
}
} // namespace tfx::cockpit
