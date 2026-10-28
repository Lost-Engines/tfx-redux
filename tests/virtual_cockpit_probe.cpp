#include <chrono>
#include <iostream>
#include "cockpit/hud.h"
#include "cockpit/frame.h"
#include "cockpit/virtual_displays.h"
#include "cockpit/vector_hud.h"
#include "missions/initialisation.h"
#include "world/assets.h"
#include "world/environment.h"
#include "world/scene.h"
#include "render/view_settings.h"

void require(bool value, char const *message) { if(!value) throw std::runtime_error{message}; }

int main(int argc, char const *const argv[]) try {
  if(argc<2 || argc>3) throw std::runtime_error{"usage: virtual_cockpit_probe DATA_DIR [CAPTURE_DIR]"};
  tfx::resources::archive game{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::executable_tables tables{std::filesystem::path{argv[1]}/"TFX.DAT"};
  tfx::world::assets assets{game};
  tfx::world::mission mission{game,"miss501"};
  auto setup{tfx::missions::initialise(mission.script)};
  tfx::world::environment environment{game,mission.grid_name(),setup.environment};
  auto dac{game.read("colours/"+environment.lighting.palette_name+".col")};
  auto palette{tfx::render::decode_palette(dac)};
  auto const fixed{tfx::render::expand_dac_palette(tables.fixed_flight_palette)};
  for(unsigned i=0;i<256;++i) if(fixed[i]!=vec3<uint8_t>{255,0,255}) palette[i]=vec3<uint8_t>{fixed[i]};
  tfx::render::software_renderer reference{tables}, fast{tables};
  // Compare optimized and general painters, including remap-to-transparent,
  // opaque black overlays, clipping, odd sizes and optional write coverage.
  std::vector<uint8_t> texture(256*192);
  std::vector<uint16_t> replacement(texture.size());
  tfx::render::colour_map remap;
  for(unsigned i=0;i<256;++i) remap[i]=static_cast<uint8_t>((i*17)%256);
  remap[3]=0;remap[0]=27;
  for(size_t i=0;i<texture.size();++i) {
    texture[i]=static_cast<uint8_t>(i*13+i/256);
    if(i%7==0) replacement[i]=static_cast<uint16_t>(0x100|(i%5==0 ? 0 : 6));
  }
  tfx::render::framebuffer canvas;
  for(int y=0;y<200;++y) for(int x=0;x<320;++x) canvas.pixels[static_cast<size_t>(y*320+x)]=static_cast<uint8_t>(x%251);
  std::array<tfx::render::texture_patch,1> const direct_patch{{{{0,0},{256,192},{0,0},{320,200}}}};
  std::vector<uint8_t> direct_cells(256*192,1);
  tfx::render::texture_overlay const direct{.patches{direct_patch},.cells{direct_cells},.source{canvas.pixels}};
  for(auto const &size : {vec2<int>{320,200},vec2<int>{997,603}}) for(unsigned flags=0;flags<32;++flags) {
    for(auto *r : {&reference,&fast}) {
      r->output.resize(size);r->output.pixels.fill(99);
      r->clipping=flags&8 ? tfx::render::raster_clip::pixels({-20,-30},size+vec2<int>{40,50}) : tfx::render::raster_clip::pixels({9,7},size-vec2<int>{13,17});
      if(flags&4) r->output.track_writes();else r->output.coverage.clear();
    }
    for(int i=0;i<12;++i) {
      double const shift{i*1.23};
      std::array<tfx::render::screen_vertex,4> quad{{
        {.position{-20+shift,10.25},.uv{-3.25,191.8},.shade{-1}},
        {.position{size.x*.93,25.25+shift},.uv{270.5,201.25},.shade{-1}},
        {.position{size.x*.85,size.y+17.75},.uv{255.5,0},.shade{-1}},
        {.position{20.25,size.y*.92},.uv{0,-7.5},.shade{-1}},
      }};
      auto const *map{flags&1 ? &remap : nullptr};
      auto const overlay{flags&16 ? direct : flags&2 ? tfx::render::texture_overlay{.texels{replacement}} : tfx::render::texture_overlay{}};
      reference.polygon(quad,0,5,texture,map,overlay); // General, non-stippled painter.
      fast.polygon(quad,0,-1,texture,map,overlay);
    }
    require(reference.output.pixels==fast.output.pixels,"optimized texture pixels differ");
    require(reference.output.coverage==fast.output.coverage,"optimized texture coverage differs");
  }
  // A black live-screen texel must cover the scene, even when remap[0] is
  // non-black. Conversely, an unmodified texel lit to zero stays transparent.
  fast.output.resize({320,200});fast.clipping=tfx::render::raster_clip::pixels({0,0},{319,199});
  fast.output.pixels.fill(99);std::ranges::fill(texture,3);std::ranges::fill(replacement,0);
  replacement[0]=0x100;replacement[1]=0x106;
  std::array<tfx::render::screen_vertex,4> test_quad{{
    {.position{0,0},.uv{0,0},.shade{-1}}, {.position{3,0},.uv{3,0},.shade{-1}},
    {.position{3,1},.uv{3,1},.shade{-1}}, {.position{0,1},.uv{0,1},.shade{-1}},
  }};
  fast.polygon(test_quad,0,-1,texture,&remap,{.texels{replacement}});
  require(fast.output.pixels[0]==0 && fast.output.pixels[1]==6 && fast.output.pixels[2]==99,"live texture opacity/emission changed");
  // Distance remapping must never turn the source transparent texel into a
  // pale UI colour (the crash decal's previously visible rectangular splash).
  for(int mode : {-1,5}) {
    fast.output.pixels.fill(99);std::ranges::fill(texture,0);
    fast.polygon(test_quad,0,mode,texture,&remap);
    require(std::ranges::all_of(fast.output.pixels,[](auto p){return p==99;}),"distance map made transparent texture pixels opaque");
  }
  // Project a 320-column canvas through a 256-column atlas to 640 pixels.
  // Every source column must remain exactly two pixels wide; copying it into
  // the small atlas first would alternate missing and over-wide columns.
  fast.output.resize({640,400});fast.clipping=tfx::render::raster_clip::pixels({0,0},{639,399});
  std::array<tfx::render::screen_vertex,4> const stripes{{
    {.position{0,0},.uv{0,0},.shade{-1}}, {.position{640,0},.uv{256,0},.shade{-1}},
    {.position{640,400},.uv{256,192},.shade{-1}}, {.position{0,400},.uv{0,192},.shade{-1}},
  }};
  for(int mode : {-1,5}) {
    fast.output.pixels.fill(99);fast.polygon(stripes,0,mode,texture,&remap,direct);
    for(int y=0;y<400;++y) for(int x=0;x<640;++x)
      require(fast.output.pixels[static_cast<size_t>(y*640+x)]==static_cast<uint8_t>((x/2)%251),"live pixels were resampled through the atlas");
  }
  tfx::cockpit::virtual_displays displays{tables.instruments()};
  tfx::render::framebuffer pattern;
  for(unsigned i=0;i<3;++i) {
    auto const &origin{tables.instruments().display_origins[i]};
    auto const &extent{tables.instruments().display_sizes[i]};
    for(int y=0;y<=extent.y;++y) for(int x=0;x<=extent.x;++x)
      pattern.write(static_cast<size_t>((origin.y+y)*320+origin.x+x),static_cast<uint8_t>(i*64+x));
  }
  displays.update(pattern);
  auto left{displays.texture("cpt_f22",{tfx::models::texture_source::global,1,{}})};
  auto right{displays.texture("cpt_f22",{tfx::models::texture_source::global,0,{}})};
  require(left.sample(140*256+60,60,140)<0x140 && right.sample(140*256+60,60,140)>=0x180,"outer screens were swapped");
  require(left.sample(160*256+159,159.5,160)+1==right.sample(160*256,.5,160),"centre display seam repeats/skips a column");
  require(!left.sample(0,0,0) && !right.sample(120*256+80,80,120),"overlay changed the cockpit surround");
  require(displays.texture("f22",{tfx::models::texture_source::global,0,{}}).empty(),"overlay reached an external model");
  require(displays.texture("cpt_f22",{tfx::models::texture_source::model,0,"!global_1"}).empty(),"overlay reached an unrelated binding");
  tfx::render::software_renderer native{tables};
  tfx::cockpit::hud hud{tables.instruments(),game,tables.weapons,environment.lighting.schematic_name};
  tfx::cockpit::frame panel_frame{tables.instruments(),game};
  for(auto const *name : {"cpt_efa","cpt_f17","cpt_f22","cptefa_f","cptf17_f","cptf22_f"}) {
    // Exercise the actual scene interpreter/culling path, rather than only
    // painting already-decoded faces. With the eye stationary, head turns
    // must not hide cockpit surfaces as their vertices pass behind the eye.
    std::optional<unsigned int> surface_count;
    for(unsigned setting=0;setting<3;++setting) for(int turn : {-1,0,1}) {
      std::array<vec2<int>,3> const sizes{{{320,200},{960,600},{3840,2160}}};
      std::array<double,3> const fovs{64.01076641616699,90,120};
      auto const size{sizes[setting]};
      tfx::world::scene scene{environment,assets};
      tfx::render::software_renderer renderer{tables};renderer.output.resize(size);
      renderer.clipping=tfx::render::raster_clip::pixels({0,0},size-vec2<int>{1,1});
      auto const view{tfx::render::scene_camera(tfx::render::inspection_camera({500000,0,500000},turn*.4,-.2),{.resolution{vec2<int>{size}},.field_of_view{fovs[setting]}})};
      renderer.output.pixels.fill(125);
      scene.queue_model(name,{500000,0,500000});scene.draw(renderer,view,1);
      require(scene.warnings.empty() && scene.primitives_drawn>0,"cockpit scene failed to draw");
      if(surface_count) require(scene.primitives_drawn==*surface_count,"head turn or FOV made cockpit surfaces disappear");
      else surface_count=scene.primitives_drawn;
      if(setting==1 && argc==3) {
        std::filesystem::create_directories(argv[2]);
        renderer.output.screenshot(std::filesystem::path{argv[2]}/(std::string{name}+"-turn"+std::to_string(turn)+".ppm"),palette);
      }
    }
    auto const plane{std::string_view{name}.find("17")!=std::string_view::npos ? tfx::flight::player_aircraft::nighthawk : std::string_view{name}.find("22")!=std::string_view::npos ? tfx::flight::player_aircraft::raptor : tfx::flight::player_aircraft::eurofighter};
    tfx::weapons::armament inventory{plane,tables.weapons,tables.arming};
    for(auto &pylon : inventory.pylons) { pylon.weapon=1;pylon.count=1; }
    inventory.selected=0;
    tfx::cockpit::hud_state state;
    state.instruments.aircraft=plane;state.instruments.inventory=&inventory;
    state.instruments.fuel=5000;state.instruments.speed=3500;state.instruments.height=-1200;
    state.position={100000,-1200,110000};state.redraw_phase=3;state.refresh_panel=true;
    state.ticks=54u*(4u*3600u+17u*60u+23u);state.chaff=15;state.flares=9;
    state.instruments.throttle=.85f;state.afterburner_pips=3;
    state.hook=true;state.fire_permission_notice=true;state.tracked_weapon_category=1;
    state.instruments.controls.autopilot.enabled=true;
    state.radio_message="AWACS CONFIRMS LOCK";
    for(auto system : {tfx::flight::aircraft_system::landing_gear,tfx::flight::aircraft_system::wheel_brakes,tfx::flight::aircraft_system::left_engine,tfx::flight::aircraft_system::right_engine})
      state.instruments.systems[std::to_underlying(system)]=tfx::flight::system_condition::active;
    tfx::cockpit::hud_selection selection{.left{2},.centre{plane==tfx::flight::player_aircraft::nighthawk ? tfx::cockpit::centre_display::moving_map : tfx::cockpit::centre_display::horizontal_radar},.right{4}};
    tfx::math::random_state random;
    native.output.pixels.fill(0);panel_frame.paint(native.output,0,{0,0});hud.draw(native,selection,state,random);
    auto const first_panel{native.output.pixels};
    for(unsigned repeat=0;repeat<6;++repeat) {
      panel_frame.paint(native.output,0,{0,0});hud.draw(native,selection,state,random);
      for(int y=162;y<168;++y) for(int x=17;x<32;++x)
        require(native.output.pixels[static_cast<size_t>(y*320+x)]==first_panel[static_cast<size_t>(y*320+x)],"fresh virtual panel lost cached engine lamps");
      for(int y=193;y<200;++y) for(int x=13;x<35;++x)
        require(native.output.pixels[static_cast<size_t>(y*320+x)]==first_panel[static_cast<size_t>(y*320+x)],"fresh virtual panel lost reheat pips");
    }
    if(argc==3) {
      std::filesystem::create_directories(argv[2]);
      native.output.screenshot(std::filesystem::path{argv[2]}/(std::string{name}+"-flat.ppm"),palette);
    }
    displays.update(native.output);
    struct region { unsigned page;int x,y,width,height;char const *name; };
    std::array<region,10> const changing{{
      {1,77,97,40,18,"gear/brakes/autopilot/hook"}, {0,49,97,30,19,"weapon warnings"},
      {1,17,128,13,6,"autopilot mode"}, {1,6,141,26,10,"clock"},
      {1,14,153,15,6,"engine lights"}, {1,13,163,21,9,"fuel"},
      {1,14,175,22,9,"throttle"}, {1,14,185,22,6,"afterburner"},
      {0,141,160,8,7,"chaff"}, {1,121,113,39,8,"radio ticker"},
    }};
    auto const region_pixels{[&](region const &r) {
      auto const overlay{displays.texture(name,{tfx::models::texture_source::global,static_cast<uint16_t>(r.page),{}})};
      std::vector<uint16_t> result;
      for(int y=r.y;y<r.y+r.height;++y) for(int x=r.x;x<r.x+r.width;++x)
        result.push_back(overlay.sample(static_cast<unsigned>(y*256+x),x+.5,y+.5));
      return result;
    }};
    std::vector<std::vector<uint16_t>> lit;
    for(auto const &region : changing) lit.push_back(region_pixels(region));
    auto const active_frame{native.output};
    auto idle{state};idle.instruments.systems.fill(tfx::flight::system_condition::normal);
    idle.hook=false;idle.fire_permission_notice=false;idle.tracked_weapon_category.reset();
    idle.instruments.controls.autopilot.enabled=false;idle.instruments.controls.autopilot.mode=tfx::cockpit::autopilot_mode::heading;
    idle.instruments.fuel=2000;idle.instruments.throttle=.35f;idle.chaff=3;
    idle.ticks=54*3600;idle.afterburner_pips=0;idle.radio_message.reset();
    panel_frame.paint(native.output,0,{0,0});hud.draw(native,selection,idle,random);displays.update(native.output);
    for(unsigned i=0;i<changing.size();++i) {
      if(lit[i]==region_pixels(changing[i])) throw std::runtime_error{std::string{"virtual instrument did not update: "}+changing[i].name};
    }
    native.output=active_frame;displays.update(native.output);
    auto const geometry{tfx::models::interpret(assets.shape(name),{.facing=false})};
    require(geometry.warnings.empty(),"cockpit geometry failed to decode");
    for(auto const &corner:tfx::cockpit::vector_hud::glass_vertices) {
      bool found{false};
      for(auto const &face:geometry.primitives) for(auto const &vertex:face.vertices)
        found |= (vertex.position-corner).length()<1e-10;
      require(found,"HUD glass no longer matches the authored cockpit frame");
    }
    for(auto const &size : {vec2<int>{960,600},vec2<int>{1920,1080},vec2<int>{3840,2160}}) {
      tfx::render::software_renderer renderer{tables};renderer.output.resize(size);
      renderer.clipping=tfx::render::raster_clip::pixels({0,0},size-vec2<int>{1,1});
      auto view{tfx::render::inspection_camera({0,0,0},0,-.35)};
      view.near_distance=.001;view.projection_origin={size.x/2.,size.y/2.};view.focal_distance=size.x*.8;
      view.viewport_maximum=vec2<int16_t>{size-vec2<int>{1,1}};
      auto const draw{[&](bool live) {
        renderer.output.pixels.fill(32);
        for(auto const &primitive : geometry.primitives) {
          std::vector<tfx::models::model_vertex> vertices;
          for(auto const &v : primitive.vertices) vertices.push_back({.position{view.transform(v.position)},.uv{vec2d{v.uv}}});
          renderer.primitive(primitive,vertices,view,primitive.texture ? assets.texture(*primitive.texture) : std::span<uint8_t const>{},&assets.model_colours(0),live && primitive.texture ? displays.texture(name,*primitive.texture) : tfx::render::texture_overlay{});
        }
      }};
      draw(false);auto const original{renderer.output.pixels};draw(true);
      require(original!=renderer.output.pixels,"cockpit still uses static screens");
      if(size.x==960 && argc==3) {
        std::filesystem::create_directories(argv[2]);
        renderer.output.screenshot(std::filesystem::path{argv[2]}/(std::string{name}+".ppm"),palette);
      }
      if(std::string_view{name}=="cpt_f22" && size.x>960) for(bool live : {false,true}) {
        auto const start{std::chrono::steady_clock::now()};
        for(unsigned frame=0;frame<40;++frame) draw(live);
        std::cout<<size<< (live ? " live" : " static")<<" canopy ms: "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/40<<'\n';
      }
    }
  }
  std::cout<<"Texture span equivalence, opaque black, instrument mapping and all six cockpit models at left/centre/right views passed\n";
} catch(std::exception const &error) { std::cerr<<error.what()<<'\n';return 1; }
