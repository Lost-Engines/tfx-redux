#pragma once

namespace tfx::render {

// Native 0xca780 resets the driver each camera update. Only missile-nose view
// calls 0xca6fc(-1,-1), halving the model focal length and visibility scale.
enum class aperture { normal, missile_nose };
constexpr unsigned int projection_shift(aperture const value) {
  return value == aperture::missile_nose ? 9 : 8;
}
constexpr unsigned int projection_focal_length(aperture const value) {
  return value == aperture::missile_nose ? 128 : 256;
}

} // namespace tfx::render
