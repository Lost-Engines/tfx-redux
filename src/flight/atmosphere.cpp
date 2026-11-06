#include "atmosphere.h"
#include <cmath>

namespace tfx::flight {
namespace {

using calculation = double;
double constexpr density_height_scale{20}, density_correction_start{10};
double constexpr density_correction{.088}, density_correction_scale{.01};
double constexpr minimum_corrected_density{.088};
double constexpr default_surface_kelvin{288.15}, celsius_to_kelvin{273};
double constexpr sound_temperature_factor{401.8};

} // anonymous namespace

atmosphere_sample sample_atmosphere(double const altitude_km, double const down_metres, std::optional<int16_t> const surface_celsius, atmosphere_parameters const &parameters) {
  auto const height{static_cast<calculation>(altitude_km)};
  atmosphere_sample result{
    .density{static_cast<double>((density_height_scale - height) / (density_height_scale + height) * parameters.sea_level_density)},
    .sea_level_kelvin{surface_celsius ? static_cast<double>(static_cast<calculation>(*surface_celsius) + celsius_to_kelvin) : default_surface_kelvin},
  };
  if(height > density_correction_start) {
    auto const excess{height - density_correction_start};
    result.density = static_cast<double>(excess * (excess * density_correction) * density_correction_scale + result.density);
    // The original applies this floor only after its above-ten-km correction.
    if(result.density < minimum_corrected_density) result.density = minimum_corrected_density;
  }
  result.local_kelvin = static_cast<double>(result.sea_level_kelvin - static_cast<calculation>(parameters.temperature_lapse_per_metre) * parameters.temperature_lapse_limit_metres);
  if(-static_cast<calculation>(down_metres) < parameters.temperature_lapse_limit_metres) result.local_kelvin = static_cast<double>(static_cast<calculation>(parameters.temperature_lapse_per_metre) * down_metres + result.sea_level_kelvin);
  // The Watcom sqrt helper receives a double argument, not an x87 temporary.
  double const sound_squared{static_cast<double>(static_cast<calculation>(result.local_kelvin) * sound_temperature_factor)};
  result.sound_metres_per_second = static_cast<double>(std::sqrt(static_cast<calculation>(sound_squared)));
  return result;
}

} // namespace tfx::flight
