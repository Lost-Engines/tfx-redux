#include <chrono>
#include <iostream>
#include "cockpit/hud.h"
#include "cockpit/virtual_displays.h"
#include "missions/initialisation.h"
#include "world/assets.h"
#include "world/environment.h"

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
  for(auto const &size : {vec2<int>{320,200},vec2<int>{997,603}}) for(unsigned flags=0;flags<16;++flags) {
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
      auto const overlay{flags&2 ? std::span<uint16_t const>{replacement} : std::span<uint16_t const>{}};
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
  fast.polygon(test_quad,0,-1,texture,&remap,replacement);
  require(fast.output.pixels[0]==0 && fast.output.pixels[1]==6 && fast.output.pixels[2]==99,"live texture opacity/emission changed");
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
  require(left[140*256+60]<0x140 && right[140*256+60]>=0x180,"outer screens were swapped");
  require(left[160*256+159]+1==right[160*256],"centre display seam repeats/skips a column");
  require(!left[0] && !right[120*256+80],"overlay changed the cockpit surround");
  require(displays.texture("f22",{tfx::models::texture_source::global,0,{}}).empty(),"overlay reached an external model");
  require(displays.texture("cpt_f22",{tfx::models::texture_source::model,0,"!global_1"}).empty(),"overlay reached an unrelated binding");
  tfx::render::software_renderer native{tables};
  tfx::cockpit::hud hud{tables.instruments(),game,tables.weapons,environment.lighting.schematic_name};
  for(auto const *name : {"cpt_efa","cpt_f17","cpt_f22","cptefa_f","cptf17_f","cptf22_f"}) {
    auto const plane{std::string_view{name}.find("17")!=std::string_view::npos ? tfx::flight::player_aircraft::nighthawk : std::string_view{name}.find("22")!=std::string_view::npos ? tfx::flight::player_aircraft::raptor : tfx::flight::player_aircraft::eurofighter};
    tfx::weapons::armament inventory{plane,tables.weapons,tables.arming};
    for(auto &pylon : inventory.pylons) { pylon.weapon=1;pylon.count=1; }
    inventory.selected=0;
    tfx::cockpit::hud_state state;
    state.instruments.aircraft=plane;state.instruments.inventory=&inventory;
    state.instruments.fuel=5000;state.instruments.speed=3500;state.instruments.height=-1200;
    state.position={100000,-1200,110000};state.redraw_phase=3;
    tfx::cockpit::hud_selection selection{.left{2},.centre{tfx::cockpit::centre_display::moving_map},.right{4}};
    tfx::math::random_state random;
    native.output.pixels.fill(0);hud.draw(native,selection,state,random);
    displays.update(native.output);
    auto const geometry{tfx::models::interpret(assets.shape(name),{.facing=false})};
    require(geometry.warnings.empty(),"cockpit geometry failed to decode");
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
          renderer.primitive(primitive,vertices,view,primitive.texture ? assets.texture(*primitive.texture) : std::span<uint8_t const>{},&assets.model_colours(0),live && primitive.texture ? displays.texture(name,*primitive.texture) : std::span<uint16_t const>{});
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
  std::cout<<"Texture span equivalence, opaque black, instrument mapping and all six cockpit models passed\n";
} catch(std::exception const &error) { std::cerr<<error.what()<<'\n';return 1; }
