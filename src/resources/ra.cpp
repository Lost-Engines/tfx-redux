#include "ra.h"
#include <array>
#include <stdexcept>

namespace tfx::resources {
namespace {

// RA v2 decoding follows Benjamin Haisch's DecompressRA (2006), also used
// by our did-unpack tool; see NOTICE.md for attribution and provenance
struct tree {
  std::vector<unsigned int> branches;
  std::vector<unsigned int> leaves;
};

class ra_reader {
private:
  std::span<uint8_t const> source;
  size_t cursor{12};
  unsigned int remaining_bits{0};
  unsigned int bit_buffer{0};

public:
  ra_reader(std::span<uint8_t const> source) : source{source} {
  }
  uint8_t byte() {
    if(cursor == source.size()) throw std::runtime_error{"truncated RA stream"};
    return source[cursor++];
  }
  unsigned int bit() {
    if(!remaining_bits) {
      bit_buffer = byte();
      remaining_bits = 8;
    }
    return (bit_buffer >> --remaining_bits) & 1u;
  }
  unsigned int value(unsigned int const count) {
    unsigned int result{0};
    for(unsigned int i{0}; i != count; ++i) result = (result << 1) | bit();
    return result;
  }
  void read_tree(tree &result) {
    auto const count{static_cast<unsigned int>(byte()) * 2};
    if(count > result.branches.size()) throw std::runtime_error{"RA tree exceeds its table"};
    auto const left_bits{value(3) + 1};
    auto const right_bits{value(3) + 1};
    for(unsigned int i{0}; i != count; ++i) {
      if(bit()) {
        result.branches[i] = 0;
        result.leaves[i] = value(right_bits);
      } else {
        result.branches[i] = value(left_bits);
      }
    }
  }
  unsigned int symbol(tree const &table) {
    unsigned int node{0};
    for(size_t depth{0}; depth != table.branches.size(); ++depth) {
      auto const index{node * 2 + bit()};
      if(index >= table.branches.size()) throw std::runtime_error{"invalid RA tree reference"};
      node = table.branches[index];
      if(!node) return table.leaves[index];
    }
    throw std::runtime_error{"cyclic RA tree"};
  }
};

} // anonymous namespace

bool is_ra(std::span<uint8_t const> const source) {
  /// Recognise the family separately so unsupported versions cannot pass as raw data
  return source.size() >= 3 && source[0] == 'R' && source[1] == 'A' && source[2] == 0;
}

bytes decode_ra(std::span<uint8_t const> const source, size_t const maximum_size) {
  /// Decode interleaved MSB-first bits and raw bytes through one shared cursor
  if(source.size() < 13 || !is_ra(source) || source[3] != 2) throw std::runtime_error{"invalid or unsupported RA v2 stream"};
  auto const expected{binary_view{source}.le32(4)};
  if(expected > maximum_size) throw std::runtime_error{"RA output exceeds configured resource limit"};
  ra_reader reader{source};
  tree counts{
    .branches{std::vector<unsigned int>(512)},
    .leaves{std::vector<unsigned int>(512)},
  };
  tree lengths{
    .branches{std::vector<unsigned int>(512)},
    .leaves{std::vector<unsigned int>(512)},
  };
  tree offsets{
    .branches{std::vector<unsigned int>(32)},
    .leaves{std::vector<unsigned int>(32)},
  };
  bytes output;
  output.reserve(expected);
  reader.bit();
  do {
    auto const low{reader.byte()};
    auto const commands{static_cast<unsigned int>(low | (reader.byte() << 8))};
    reader.read_tree(lengths);
    reader.read_tree(counts);
    reader.read_tree(offsets);
    for(unsigned int command{0}; command != commands; ++command) {
      auto const literals{reader.symbol(counts)};
      if(literals > expected - output.size()) throw std::runtime_error{"RA literals exceed declared output size"};
      for(unsigned int i{0}; i != literals; ++i) output.push_back(reader.byte());
      auto length{reader.symbol(lengths)};
      if(length == 255) {
        auto const high{reader.value(2)};
        length += (high << 8) | reader.byte();
      }
      if(!length) continue;
      auto distance{reader.symbol(offsets)};
      if(distance > 31) throw std::runtime_error{"RA offset bit count exceeds its word"};
      if(distance >= 2) distance = (1u << (distance - 1)) | reader.value(distance - 1);
      ++distance;
      if(distance > output.size()) throw std::runtime_error{"RA match precedes output buffer"};
      if(length > expected - output.size()) throw std::runtime_error{"RA match exceeds declared output size"};
      for(unsigned int i{0}; i != length; ++i) output.push_back(output[output.size() - distance]);
    }
  } while(reader.bit());
  if(output.size() != expected) throw std::runtime_error{"RA decoded size differs from header"};
  return output;
}

} // namespace tfx::resources
