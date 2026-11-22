#pragma once

#include <string>
#include <vector>
#include "audio/osl_resource.h"

namespace tfx::audio {

class music_catalogue {
private:
  std::array<std::vector<std::string>, 3> names;

public:
  explicit music_catalogue(std::span<uint8_t const> launcher);
  std::span<std::string const> files(osl_device device) const;
};

} // namespace tfx::audio
