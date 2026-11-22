#include "screenshot.h"
#include <format>
#include <fstream>
#include "render/ilbm.h"

namespace tfx::platform {

auto save_screenshot(std::filesystem::path const &directory,
    render::framebuffer const &image, render::dac_palette const &colours)->std::filesystem::path {
  unsigned int number{0};
  auto filename{[&]{return directory / std::format("GRAB_{:02}.LBM", number);}};
  auto path{filename()};
  while(std::filesystem::exists(path)) {
    ++number;
    path = filename();
  }
  auto const encoded{render::encode_ilbm(image, colours)};
  std::ofstream output;
  output.exceptions(std::ios::failbit | std::ios::badbit);
  output.open(path, std::ios::binary);
  output.write(reinterpret_cast<char const *>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
  output.close();
  return path;
}

} // namespace tfx::platform
