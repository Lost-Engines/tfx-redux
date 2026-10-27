#include <chrono>
#include <iostream>
#include "cockpit/vector_hud.h"
#include "cockpit/weapon_display.h"
#include "render/view_settings.h"

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
  std::cout<<"Vector HUD: subpixel strokes, AA, additive colour, union joins, cockpit occlusion and all weapon layouts passed\n";
} catch(std::exception const &e){std::cerr<<e.what()<<'\n';return 1;}
