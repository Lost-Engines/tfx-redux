#include "radar_voice.h"
#include <array>

namespace tfx::audio {
namespace {

unsigned int constexpr zero_sample{78}, twenty_sample{98}, thirty_sample{99};
unsigned int constexpr hundred_sample{21}, altitude_sample{25}, bearing_sample{26}, thousand_sample{35};
// Native 0xa33e also overlaps the following tens metadata. Preserve its
// stored sample choices rather than inventing thousands or an "and" sample.
std::array<unsigned int, 37> constexpr number_samples{
  78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98,
  30, 40, 50, 60, 70, 80, 90, 100, 99, 100, 101, 102, 103, 104, 105, 78,
};

bool append_number(std::vector<unsigned int> &sequence, unsigned int number) {
  unsigned int constexpr hundred{100};
  unsigned int constexpr twenty{20};
  unsigned int constexpr thirty{30};
  unsigned int constexpr ten{10};
  unsigned int const hundreds{number / hundred};
  if(hundreds >= number_samples.size()) return false;                          // Outside the original lookup's defined storage.
  if(hundreds) {
    sequence.push_back(number_samples[hundreds]);
    sequence.push_back(hundred_sample);
    number %= hundred;
    if(!number) return true;
  }
  if(number <= twenty) {
    sequence.push_back(zero_sample + number);
  } else if(number < thirty) {
    sequence.push_back(twenty_sample);
    sequence.push_back(zero_sample + number - twenty);
  } else {
    sequence.push_back(thirty_sample + number / ten - thirty / ten);
    if(number % ten) sequence.push_back(zero_sample + number % ten);
  }
  return true;
}

} // anonymous namespace

std::vector<unsigned int> radar_report_voice(int16_t const bearing_degrees, int16_t const altitude_thousands) {
  int16_t constexpr full_bearing{360};
  int16_t constexpr maximum_altitude{30000};
  if(bearing_degrees < 0 || bearing_degrees > full_bearing || altitude_thousands < 0 || altitude_thousands > maximum_altitude) return {};
  std::vector<unsigned int> result{bearing_sample};
  if(!append_number(result, static_cast<unsigned int>(bearing_degrees))) return {};
  result.push_back(altitude_sample);
  if(!append_number(result, static_cast<unsigned int>(altitude_thousands))) return {};
  result.push_back(thousand_sample);
  return result;
}

} // namespace tfx::audio
