#include <chrono>
#include <iostream>
#include "cockpit/vector_hud.h"
#include "cockpit/weapon_display.h"
#include "render/view_settings.h"
#include "cockpit/mouse_orbit.h"

void require(bool ok,char const *message){if(!ok)throw std::runtime_error{message};}
int main(int argc,char const *const argv[]) try {
  if(argc<2||argc>3)throw std::runtime_error{"usage: redux_hud_probe DATA_DIR [CAPTURE_DIR]"};
  tfx::resources::archive game{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::executable_tables tables{std::filesystem::path{argv[1]}/"TFX.DAT"};
  tfx::cockpit::vector_hud hud{tables.instruments()};
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
  // The optical boresight must follow the aircraft, stay on the real glass,
  // and disappear when the glass is behind the head camera.
  for(auto const &size : {vec2<int>{960,600},vec2<int>{3840,2160}}) for(double fov:{10.,64.,100.,140.,170.}) {
    for(double yaw:{-.4,0.,.4,1.5707963267948966,3.141592653589793}) {
      auto view{tfx::render::scene_camera(tfx::render::inspection_camera({0,0,-.25},yaw,0),{.resolution{vec2<int>{size}},.field_of_view{fov}})};
      tfx::render::framebuffer frame;frame.resize(size);
      hud.begin(size);hud.attach_to_glass(view,{0,0,0},tfx::math::rotation({0,0,0}));
      hud.line({80,60},{240,60},{0,0},{319,199});
      hud.line({160,0},{160,97},{0,0},{319,199});
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
