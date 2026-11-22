#include "music_catalogue.h"
#include <algorithm>

namespace tfx::audio {

music_catalogue::music_catalogue(std::span<uint8_t const> const launcher) {
  resources::binary_view const data{launcher};
  std::array<unsigned int, 3> constexpr starts{0x50f5, 0x4f6e, 0x52a0};
  for(unsigned int device{0}; device != names.size(); ++device) {
    auto at{starts[device]};
    for(unsigned int index{0}; index != 32; ++index) {
      auto const prefix{data.slice(at, 6)};
      if(std::string_view{reinterpret_cast<char const *>(prefix.data()), prefix.size()} != "tunes\\") break;
      std::string name;
      while(auto const byte{data.byte(at++)}) {
        if(name.size() == 64) throw std::runtime_error{"unterminated native music filename"};
        name.push_back(byte == '\\' ? '/' : static_cast<char>(byte));
      }
      names[device].push_back(std::move(name));
    }
    if(names[device].empty()) throw std::runtime_error{"native music filename list is missing"};
  }
}
std::span<std::string const> music_catalogue::files(osl_device const device) const {
  switch(device) {
  case osl_device::fm:
    return names[0];
  case osl_device::lapc:
    return names[1];
  case osl_device::sound_canvas:
    return names[2];
  }
  throw std::invalid_argument{"unknown OSL music device"};
}

} // namespace tfx::audio
