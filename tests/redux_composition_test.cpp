#include <chrono>
#include <iostream>
#include <stdexcept>
#include "cockpit/vector_hud.h"
#include "render/view_settings.h"
#include "resources/executable_tables.h"

namespace {
using namespace tfx;
void require(bool value, char const *message) { if(!value) throw std::runtime_error{message}; }

// Independent destination-pixel reference: deliberately retain the simple
// inverse sampling used before the run-based compositor.
void reference_interface(render::framebuffer &target, render::framebuffer const &source, bool mask_only = false) {
  render::interface_layout const layout{target.size};
  for(int y=0;y<target.size.y;++y) for(int x=0;x<target.size.x;++x) {
    int const sx{static_cast<int>(std::floor((x-layout.offset.x)/layout.scale))};
    int const sy{static_cast<int>(std::floor((y-layout.offset.y)/layout.scale))};
    if(sx<0 || sx>=320 || sy<0 || sy>=200) continue;
    auto const src{static_cast<size_t>(sy*320+sx)},dst{static_cast<size_t>(y*target.size.x+x)};
    if(source.coverage[src]) {
      if(!mask_only) target.pixels[dst]=source.pixels[src];
      if(!target.hud_coverage.empty()) target.hud_coverage[dst]=0;
    }
  }
}

void check() {
  render::framebuffer ui;ui.track_writes();
  for(size_t i=0;i<ui.pixels.size();++i) ui.pixels[i]=static_cast<uint8_t>(i*47+i/320);
  for(auto const &size:{vec2<int>{1,1},{160,100},{320,200},{321,201},{997,603},{101,499},{4097,513},{3840,2160}}) {
    render::framebuffer actual;actual.resize(size);
    for(unsigned mask=0;mask<4;++mask) {
      actual.pixels.fill(91);actual.hud_coverage.assign(actual.pixels.size(),173);
      for(size_t i=0;i<ui.coverage.size();++i)
        ui.coverage[i]=static_cast<uint8_t>(mask==0 ? 0 : mask==1 ? 255 : mask==2 ? ((i*37+i/320)%17<6 ? 1 : 0) : (i%320==0 || i%320==319 || i/320==0 || i/320==199));
      auto expected{actual};reference_interface(expected,ui);
      render::composite_interface(actual,ui);
      require(actual.pixels==expected.pixels && actual.hud_coverage==expected.hud_coverage,"scaled UI/optical coverage differs from destination sampling");
    }
    ui.coverage.assign(ui.pixels.size(),255);
    render::framebuffer expected;expected.resize(size);reference_interface(expected,ui);
    require(render::scale_image(ui,size).pixels==expected.pixels,"image scaling changed source pixels/letterbox boundaries");
  }
  render::framebuffer colours;colours.resize({256,256});
  colours.hud_coverage.resize(colours.pixels.size());
  render::palette palette;
  for(unsigned i=0;i<256;++i) palette[i]={static_cast<uint8_t>(i),static_cast<uint8_t>(i*71),static_cast<uint8_t>(255-i)};
  for(unsigned coverage=0;coverage<256;++coverage) for(unsigned index=0;index<256;++index) {
    auto const at{coverage*256+index};colours.pixels[at]=static_cast<uint8_t>(index);colours.hud_coverage[at]=static_cast<uint8_t>(coverage);
  }
  std::vector<render::rgba_pixel> output;
  for(unsigned pass=0;pass<3;++pass) {
    if(pass==1) colours.hud_coverage.clear();
    if(pass==2) {colours.resize({13,7});colours.pixels.fill(47);}
    colours.colourise(palette,output);
    require(output==colours.colourise(palette),"reused colour buffer differs from allocating conversion");
    for(size_t at=0;at<output.size();++at) {
      unsigned const coverage{colours.hud_coverage.empty() ? 0u : colours.hud_coverage[at]};
      for(unsigned channel=0;channel<3;++channel) {
        unsigned const light{(static_cast<unsigned>(palette[6][channel])*coverage*9+1275)/2550};
        auto const expected{static_cast<uint8_t>(std::min(255u,static_cast<unsigned>(palette[colours.pixels[at]][channel])+light))};
        require(output[at][channel]==expected,"palette expansion/additive HUD colour changed");
      }
      require(output[at].a==255,"presentation alpha changed");
    }
  }
  colours.hud_coverage.resize(1);
  bool rejected{false};try {colours.colourise(palette,output);} catch(std::logic_error const&) {rejected=true;}
  require(rejected,"mismatched HUD coverage was accepted");
  std::cout<<"UI scaling and sparse masks match independent per-pixel references through 4K\n";
}

void benchmark(std::filesystem::path const &directory) {
  render::framebuffer image;image.resize({3840,2160});
  render::framebuffer ui;ui.track_writes();
  for(int y=120;y<200;++y) for(int x=0;x<320;++x) ui.write(static_cast<size_t>(y*320+x),static_cast<uint8_t>(x/8+y/8));
  resources::executable_tables const tables{directory/"TFX.DAT"};cockpit::vector_hud hud{tables.instruments()};
  render::palette palette;for(unsigned i=0;i<256;++i) palette[i]={static_cast<uint8_t>(i),static_cast<uint8_t>(255-i),static_cast<uint8_t>(i/2)};
  auto const time{[&](char const *name,auto const &run) {
    for(int i=0;i<3;++i) run();
    auto const start{std::chrono::steady_clock::now()};
    for(int i=0;i<40;++i) run();
    std::cout<<name<<": "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/40<<" ms\n";
  }};
  time("4K interface",[&]{render::composite_interface(image,ui);});
  time("4K diagonal HUD",[&]{hud.begin(image.size);hud.line({0,0},{319,199},{0,0},{319,199});hud.line({20,180},{300,20},{0,0},{319,199});hud.compose(image,{});});
  time("4K HUD mask",[&]{hud.compose(image,ui.coverage);});
  std::vector<render::rgba_pixel> pixels;
  time("4K colour conversion",[&]{image.colourise(palette,pixels);});
  uint64_t hash{14695981039346656037ull};for(auto const &p:pixels) for(auto value:p) hash=(hash^value)*1099511628211ull;
  std::cout<<"RGBA hash: "<<hash<<'\n';
}
}
int main(int argc,char const *const argv[]) try {
  if(argc==3 && std::string_view{argv[1]}=="--benchmark") benchmark(argv[2]);
  else if(argc==1) check();
  else throw std::runtime_error{"usage: redux_composition_tests [--benchmark DATA_DIR]"};
} catch(std::exception const &error) { std::cerr<<error.what()<<'\n';return 1; }
