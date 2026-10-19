#include "precipitation.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include "math/floating_environment.h"

namespace tfx::render {
namespace {
int16_t word(int32_t const value) { return math::signed_word(static_cast<uint16_t>(value)); }
int32_t wrap(uint32_t const value) { return std::bit_cast<int32_t>(value); }
int16_t advect(int16_t const initial, float const wind) {
  // Original x87 operations load both constants from single precision, retain
  // 53-bit arithmetic, then truncate to a signed dword before the word store.
  float constexpr wind_scale{4}, per_frame{.1f};
  long double const result{static_cast<long double>(wind) * wind_scale * per_frame + initial};
  int32_t const truncated{!std::isfinite(result) || result < std::numeric_limits<int32_t>::min() || result >= 2147483648.L ? std::numeric_limits<int32_t>::min() : static_cast<int32_t>(result)};
  return word(truncated);
}
}

std::optional<int16_t> precipitation_count(precipitation_gate const &context) {
  // Native FCOMP/JAE also falls through on an unordered height comparison;
  // negating a greater-than test would instead suppress that frame.
  if(!context.enabled || !context.storm || context.layer_y >= context.player_y || context.view == cockpit::view_mode::virtual_cockpit || context.view == cockpit::view_mode::target_tracking) return std::nullopt;
  int constexpr resting_count{200};
  return word(wrap(static_cast<uint32_t>(resting_count) - static_cast<uint32_t>(context.speed / 2)));
}

precipitation::precipitation(vec3<int16_t> const &drift) : offset{drift} {}
void precipitation::initialise(math::random_state &random) {
  for(auto &point : particles) for(unsigned int axis{0}; axis != 3; ++axis) {
    uint16_t const first{random.next()};
    point[axis] = math::signed_word(static_cast<uint16_t>(first + random.next()));
  }
}
std::span<vec3<int16_t> const> precipitation::field() const { return particles; }
vec3<int16_t> const &precipitation::drift() const { return offset; }
void precipitation::paint(framebuffer &output, precipitation_view const &view, int16_t const requested_count, vec2f const &wind_xz) {
  math::floating_environment const environment;
  int constexpr volume_half_size{512}, volume_mask{1023}, camera_scale{4};
  uint8_t constexpr colour{241};
  int const count{std::clamp<int>(requested_count,0,precipitation_particles)};
  vec3<int16_t> look_ahead;
  // Transpose-camera times {0,0,512}: keep the word narrowing before wrapping
  // particles into the moving 1,024-unit box around this forward-biased view.
  for(unsigned int axis{0}; axis != 3; ++axis) look_ahead[axis] = word((word(view.matrix.data[6 + axis]) * volume_half_size) >> math::matrix_fraction_bits);
  for(int i{0}; i != count; ++i) {
    vec3<int16_t> local;
    for(unsigned int axis{0}; axis != 3; ++axis) local[axis] = static_cast<int16_t>(((particles[static_cast<unsigned int>(i)][axis] - view.camera[axis] * camera_scale + offset[axis] - look_ahead[axis]) & volume_mask) - volume_half_size);
    vec3<int32_t> camera;
    for(unsigned int row{0}; row != 3; ++row) {
      uint32_t sum{0};
      for(unsigned int column{0}; column != 3; ++column) sum += static_cast<uint32_t>(word(view.matrix.data[row * 3 + column])) * static_cast<uint32_t>(local[column]);
      camera[row] = wrap(sum);
    }
    // Native 0x78a9c adds 512 Q15 depth units before its fixed SAR8 divide.
    // This particle projection is independent of the model driver's FOV mode.
    int32_t const depth{wrap(static_cast<uint32_t>(camera.z) + (1u << 24)) >> 8};
    vec2<int16_t> const point{depth <= 16 ? vec2<int16_t>{-1,-1} : vec2<int16_t>{word(camera.x / depth + view.origin.x),word(camera.y / depth + view.origin.y)}};
    if(point.x >= view.minimum.x && point.y >= view.minimum.y && point.x <= view.maximum.x && point.y <= view.maximum.y && point.x >= 0 && point.y >= 0 && point.x < screen_width && point.y < screen_height) output.write(static_cast<unsigned int>(point.y * screen_width + point.x),colour);
  }
  offset.x = advect(offset.x,wind_xz.x);
  offset.z = advect(offset.z,wind_xz.y);
  offset.y = word(offset.y + 30);
}

} // namespace tfx::render
