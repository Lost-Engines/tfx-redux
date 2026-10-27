#pragma once

#include "render/framebuffer.h"

namespace tfx::render {

// Original 4-key captures retain eight-plane IFF indices and their DAC palette.
// Optical HUD captures use 24 RGB planes to preserve the final additive colours.
auto encode_ilbm(framebuffer const &image, dac_palette const &colours)->std::vector<uint8_t>;

} // namespace tfx::render
