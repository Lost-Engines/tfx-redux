#include <chrono>
#include <iostream>
#include "cockpit/vector_hud.h"
#include "cockpit/weapon_display.h"
#include "render/view_settings.h"
#include "cockpit/mouse_orbit.h"

void require(bool ok,char const *message){if(!ok)throw std::runtime_error{message};}
namespace {
using namespace tfx;
void reference_stroke(render::framebuffer &target, vec2d const &from, vec2d const &to, vec2<int> const &low, vec2<int> const &high) {
  render::interface_layout const layout{target.size};
  auto const a{layout.project(from+vec2d{.5,.5})},b{layout.project(to+vec2d{.5,.5})},delta{b-a};
  double const length2{delta.dot(delta)},radius{std::max(1.0,.65*layout.scale)/2};
  vec2<int> clip_low,clip_high;
  for(unsigned axis=0;axis<2;++axis) {
    clip_low[axis]=std::max(0,static_cast<int>(std::ceil(layout.offset[axis]+low[axis]*layout.scale-.5)));
    clip_high[axis]=std::min(target.size[axis]-1,static_cast<int>(std::ceil(layout.offset[axis]+(high[axis]+1)*layout.scale-.5))-1);
  }
  target.hud_coverage.assign(target.pixels.size(),0);
  for(int y=clip_low.y;y<=clip_high.y;++y) for(int x=clip_low.x;x<=clip_high.x;++x) {
    vec2d const point{x+.5,y+.5};
    double const t{length2 ? std::clamp((point-a).dot(delta)/length2,0.0,1.0) : 0};
    double const coverage{std::clamp(radius+.5-(point-a-delta*t).length(),0.0,1.0)};
    target.hud_coverage[static_cast<size_t>(y*target.size.x+x)]=static_cast<uint8_t>(std::lround(coverage*255));
  }
}

void check_stroke_reference(cockpit::instrument_assets const &assets) {
  cockpit::vector_hud hud{assets};
  std::array<std::pair<vec2d,vec2d>,9> const lines{{
    {{0,0},{319,199}},{{319,199},{0,0}},{{12.25,188.1},{307.8,3.75}},
    {{0,50.25},{319,50.25}},{{80.25,-20},{80.25,220}},{{0,80},{319,80.001}},
    {{-40,-20},{350,230}},{{110.25,82.75},{110.25,82.75}},{{170,130},{-100,30}},
  }};
  for(auto const &size:{vec2<int>{320,200},{997,603},{3840,2160}}) for(auto const &[from,to]:lines) {
    render::framebuffer actual;actual.resize(size);
    hud.begin(size);hud.line(from,to,{7,4},{312,194});hud.compose(actual,{});
    auto expected{actual};reference_stroke(expected,from,to,{7,4},{312,194});
    require(actual.hud_coverage==expected.hud_coverage,"HUD stroke strip lost or changed antialiased pixels");
    if(from.x==0 && from.y==0) {
      std::vector<uint8_t> mask(320*200);
      for(int y=0;y<200;++y) for(int x=0;x<320;++x) mask[static_cast<size_t>(y*320+x)]=static_cast<uint8_t>(((x/7+y/11)&1)*255);
      hud.compose(actual,mask);
      render::interface_layout const layout{size};
      for(int y=0;y<size.y;++y) for(int x=0;x<size.x;++x) {
        int const sx{static_cast<int>(std::floor((x-layout.offset.x)/layout.scale))},sy{static_cast<int>(std::floor((y-layout.offset.y)/layout.scale))};
        if(sx>=0 && sx<320 && sy>=0 && sy<200 && mask[static_cast<size_t>(sy*320+sx)]) expected.hud_coverage[static_cast<size_t>(y*size.x+x)]=0;
      }
      require(actual.hud_coverage==expected.hud_coverage,"sparse cockpit occlusion changed HUD coverage");
    }
  }
}
}
int main(int argc,char const *const argv[]) try {
  if(argc<2||argc>3)throw std::runtime_error{"usage: redux_hud_probe DATA_DIR [CAPTURE_DIR]"};
  tfx::resources::archive game{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::executable_tables tables{std::filesystem::path{argv[1]}/"TFX.DAT"};
  tfx::cockpit::vector_hud hud{tables.instruments()};
  check_stroke_reference(tables.instruments());
  tfx::render::palette palette;palette.fill({55,60,70});palette[6]={0,255,0};
  for(auto const &size : {vec2<int>{320,200},vec2<int>{1001,603},vec2<int>{1920,1080}}) {
    tfx::render::framebuffer frame;frame.resize(size);
    hud.begin(size);hud.line({110.25,30.5},{205.75,85.25},{100,20},{215,90});hud.compose(frame,{});
    require(frame.hud_coverage.size()==frame.pixels.size(),"HUD dimensions lost");
    require(std::ranges::any_of(frame.hud_coverage,[](auto x){return x>0&&x<255;}),"HUD has no antialiased edges");
    auto first{frame.hud_coverage};
    hud.line({110.25,30.5},{205.75,85.25},{100,20},{215,90});hud.compose(frame,{});
    require(first==frame.hud_coverage,"overlapping strokes brighten joins");
    hud.begin(size);hud.line({110.5,30.5},{206,85.25},{100,20},{215,90});hud.compose(frame,{});
    require(first!=frame.hud_coverage,"HUD snapped movement to original pixels");
    auto rgb{frame.colourise(palette)};
    for(size_t i=0;i<rgb.size();++i) {
      require(rgb[i].r==55 && rgb[i].b==70 && rgb[i].g>=60,"HUD replaced/darkened background colour");
      if(!frame.hud_coverage[i])require(rgb[i].g==60,"HUD changed uncovered pixels");
    }
    std::vector<uint8_t> covered(320*200,255);hud.compose(frame,covered);
    require(std::ranges::all_of(frame.hud_coverage,[](auto x){return x==0;}),"cockpit failed to cover optical HUD");
    // Complete HUD layouts, using the real font, masks and heading/pitch banks.
    tfx::cockpit::weapon_display display{tables.instruments(),game};
    tfx::render::software_renderer native{tables};
    auto start{std::chrono::steady_clock::now()};
    for(unsigned mode=0;mode<3;++mode) {
      tfx::cockpit::weapon_display_state state;
      state.mode=static_cast<tfx::cockpit::aiming_mode>(mode);state.cannon=false;
      state.weapon="AIM 9L";state.range=100;state.ammunition=4;state.limited=true;
      state.attitude.angles={0,0,8192};state.attitude.projection_angles={0,8192};
      state.flight.speed=4100;state.flight.height=-800;state.flight.throttle=.8f;
      state.flight.mach=.86;state.flight.load=1.2f;
      hud.begin(size);native.output.track_writes();
      {tfx::cockpit::capture_hud capture{native.output,&hud};display.draw_forward(native,state);}
      require(native.output.hud_capture==nullptr,"capture outlived its scope");
      require(std::ranges::all_of(native.output.coverage,[](auto x){return !x;}),"HUD also painted old bitmap layer");
      frame.hud_coverage.clear();hud.compose(frame,{});
      require(std::ranges::count_if(frame.hud_coverage,[](auto x){return x!=0;})>500,"HUD layout lost its symbols");
      if(argc==3) {
        std::filesystem::create_directories(argv[2]);
        frame.screenshot(std::filesystem::path{argv[2]}/(std::to_string(size.x)+"-"+std::to_string(mode)+".ppm"),palette);
      }
    }
    std::cout<<size.x<<'x'<<size.y<<" vector HUD layouts: "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/3<<" ms each (includes optional captures)\n";
  }
  // The real Military-spec layout should use the glass, not sit in a small
  // patch at its bottom. Measure angular coverage independently of the HUD's
  // own focal length; both resolution and the camera FOV must cancel out.
  for(auto const &size : {vec2<int>{1001,603},vec2<int>{1920,1080}}) for(double fov : {64.,100.,140.}) {
    auto const view{tfx::render::scene_camera(tfx::render::inspection_camera({0,0,-.25},0,0),{.resolution{vec2<int>{size}},.field_of_view{fov}})};
    vec2d glass_low{size},glass_high{0,0};
    for(auto const &vertex : tfx::cockpit::vector_hud::glass_vertices) {
      auto const point{view.project(view.transform(vertex))};
      for(unsigned axis=0;axis<2;++axis) {
        glass_low[axis]=std::min(glass_low[axis],point[axis]);
        glass_high[axis]=std::max(glass_high[axis],point[axis]);
      }
    }
    auto const glass_size{glass_high-glass_low};
    for(auto aircraft : {tfx::flight::player_aircraft::eurofighter,tfx::flight::player_aircraft::nighthawk,tfx::flight::player_aircraft::raptor}) {
      tfx::cockpit::weapon_display display{tables.instruments(),game};
      tfx::cockpit::weapon_display_state state;
      state.aircraft=aircraft;state.cannon=false;state.ammunition=1760;
      state.attitude.projection_origin=vec2<int16_t>{tfx::cockpit::vector_hud::glass_origin};
      state.attitude.focal_distance=tfx::cockpit::vector_hud::glass_focal_distance;
      state.flight.speed=4500;state.flight.height=-1400;state.flight.mach=.85;
      state.flight.load=1;state.flight.throttle=.85f;
      state.aerodynamic.emplace(tfx::cockpit::aerodynamic_hud_state{.down_metres_per_second{12}});
      tfx::render::software_renderer native{tables};
      tfx::render::framebuffer frame;frame.resize(size);
      hud.begin(size);hud.attach_to_glass(view,{0,0,0},tfx::math::rotation({0,0,0}));
      {tfx::cockpit::capture_hud capture{native.output,&hud};display.draw_forward(native,state);}
      hud.compose(frame,{});
      vec2<int> low{size},high{0,0};
      for(int y=0;y<size.y;++y) for(int x=0;x<size.x;++x) if(!frame.hud_coverage.empty() && frame.hud_coverage[static_cast<size_t>(y*size.x+x)]) {
        low.x=std::min(low.x,x);low.y=std::min(low.y,y);
        high.x=std::max(high.x,x);high.y=std::max(high.y,y);
      }
      require(high.x-low.x > .82*glass_size.x,"virtual HUD leaves too much unused glass at the sides");
      require(low.y-glass_low.y < .08*glass_size.y,"virtual HUD sits too far below the top of the glass");
      require(glass_high.y-high.y < .10*glass_size.y,"virtual HUD leaves too much unused glass below");
      if(argc==3 && size.x==1920 && fov==64) frame.screenshot(std::filesystem::path{argv[2]}/("glass-"+std::to_string(std::to_underlying(aircraft))+".ppm"),palette);
    }
  }
  // The optical boresight must follow the aircraft, stay on the real glass,
  // and disappear when the glass is behind the head camera.
  for(auto const &size : {vec2<int>{960,600},vec2<int>{3840,2160}}) for(double fov:{10.,64.,100.,140.,170.}) {
    for(double yaw:{-.4,0.,.4,1.5707963267948966,3.141592653589793}) {
      auto view{tfx::render::scene_camera(tfx::render::inspection_camera({0,0,-.25},yaw,0),{.resolution{vec2<int>{size}},.field_of_view{fov}})};
      tfx::render::framebuffer frame;frame.resize(size);
      hud.begin(size);hud.attach_to_glass(view,{0,0,0},tfx::math::rotation({0,0,0}));
      auto const origin{tfx::cockpit::vector_hud::glass_origin};
      hud.line({80,origin.y},{240,origin.y},{0,0},{319,199});
      hud.line({origin.x,0},{origin.x,97},{0,0},{319,199});
      hud.compose(frame,{});
      size_t visible{0};
      for(int y=0;y<size.y;++y) for(int x=0;x<size.x;++x) {
        if(frame.hud_coverage.empty() || !frame.hud_coverage[static_cast<size_t>(y*size.x+x)]) continue;
        ++visible;
        vec3d const ray{(x+.5-view.projection_origin.x)/view.focal_distance,(y+.5-view.projection_origin.y)/view.focal_distance,1};
        auto const &glass{tfx::cockpit::vector_hud::glass_vertices};
        vec3d centre{0,0,0};for(auto const &v:glass) centre+=view.transform(v);
        for(size_t i=0;i<glass.size();++i) {
          auto normal{view.transform(glass[i]).cross(view.transform(glass[(i+1)%glass.size()]))};
          if(normal.dot(centre)<0) normal=-normal;
          require(normal.dot(ray)>=-1e-10,"HUD escaped cockpit glass");
        }
      }
      if(yaw>3) require(!visible,"HUD appeared behind pilot");
      else {
        auto const world_direction{vec3d{.5/tfx::cockpit::vector_hud::glass_focal_distance,.5/tfx::cockpit::vector_hud::glass_focal_distance,1}};
        vec3d const camera_direction{world_direction.dot(view.right),world_direction.dot(view.down),world_direction.dot(view.forward)};
        auto const p{view.project(camera_direction)};
        if(camera_direction.z<=0 || p.x<2 || p.y<2 || p.x>=size.x-2 || p.y>=size.y-2) continue;
        require(visible>0,"head turn or FOV removed visible HUD");
        bool hit{false};
        for(int dy=-2;dy<=2;++dy) for(int dx=-2;dx<=2;++dx) {
          int x=static_cast<int>(p.x)+dx,y=static_cast<int>(p.y)+dy;
          if(x>=0 && y>=0 && x<size.x && y<size.y && frame.hud_coverage[static_cast<size_t>(y*size.x+x)]) hit=true;
        }
        require(hit,"optical boresight no longer points along the aircraft");
      }
    }
  }
  std::cout<<"Vector HUD: subpixel strokes, AA, additive colour, union joins, cockpit occlusion and all weapon layouts passed\n";
} catch(std::exception const &e){std::cerr<<e.what()<<'\n';return 1;}
