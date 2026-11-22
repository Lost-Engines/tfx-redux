#include <cstdlib>
#include <iostream>
#include <sstream>
#include "presentation/dialogue.h"
#include "resources/binary.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"dialogue probe requires reference executable"};
  tfx::presentation::widget_font const font{tfx::resources::read_file(argv[1])};
  std::string line;
  while(std::getline(std::cin,line)) {
    auto const separator{line.find('|')};
    if(separator == std::string::npos) throw std::runtime_error{"missing dialogue text"};
    std::istringstream fields{line.substr(0,separator)};
    unsigned int kind,fx,fy,fw,fh,fb,x,y,w,h,foreground,background,border;
    fields >> kind >> fx >> fy >> fw >> fh >> fb >> x >> y >> w >> h >> foreground >> background >> border;
    tfx::presentation::dialogue_form const form{
      .position{static_cast<uint8_t>(fx),static_cast<uint8_t>(fy)},
      .size{static_cast<uint8_t>(fw),static_cast<uint8_t>(fh)},
      .background{static_cast<uint8_t>(fb)},
    };
    tfx::render::framebuffer image;
    if(kind == 0) tfx::presentation::draw_dialogue_form(image,form);
    else tfx::presentation::draw_dialogue_widget(image,font,form,{
      .position{static_cast<uint8_t>(x),static_cast<uint8_t>(y)},
      .size{static_cast<uint8_t>(w),static_cast<uint8_t>(h)},
      .text{line.substr(separator + 1)},
      .foreground{static_cast<uint8_t>(foreground)},
      .background{static_cast<uint8_t>(background)},
      .border{static_cast<tfx::presentation::dialogue_border>(border)},
    });
    std::cout.write(reinterpret_cast<char const*>(image.pixels.data()),image.pixels.size());
  }
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
