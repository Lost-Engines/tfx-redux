#include "gus_memory.h"
#include <numeric>
#include <stdexcept>
#include <utility>

namespace tfx::audio {

gus_sample_ram::gus_sample_ram(unsigned int const ram_kib) : free_bytes{0} {
  unsigned int constexpr bank_kib{256}, maximum_kib{1024}, bytes_per_kib{1024};
  if(!ram_kib || ram_kib > maximum_kib || ram_kib % bank_kib) throw std::invalid_argument{"Gravis RAM must be 256, 512, 768 or 1024 KiB"};
  free_bytes = ram_kib * bytes_per_kib - streaming_bytes;
}
std::optional<uint32_t> gus_sample_ram::allocate(uint32_t const bytes) {
  // GUS.COM command 5 uses JBE: an exact fit fails, and zero bytes can succeed.
  if(bytes >= free_bytes) return {};
  auto const address{next_address};
  next_address += bytes;
  free_bytes -= bytes;
  return address;
}
uint32_t gus_sample_ram::remaining() const {
  return free_bytes;
}

gus_residency plan_gus_residency(std::span<uint32_t const> const sizes, unsigned int const ram_kib) {
  // Links occupy nine signed bits; -1 terminates the list, indices are 0..255.
  unsigned int constexpr maximum_samples{256}, maximum_sample_bytes{65535};
  if(sizes.size() > maximum_samples) throw std::out_of_range{"Gravis sample list exceeds its native link indices"};
  std::vector<unsigned int> order(sizes.size());
  std::iota(order.begin(),order.end(),0u);
  for(auto const size : sizes) if(size > maximum_sample_bytes) throw std::out_of_range{"Gravis sample exceeds its native word length"};
  // Native swaps strictly larger later entries into place. Equal lengths can
  // be displaced indirectly; stable_sort would change their allocation order.
  for(unsigned int first{0}; first != order.size(); ++first) {
    for(unsigned int next{first + 1}; next != order.size(); ++next) {
      if(sizes[order[first]] < sizes[order[next]]) std::swap(order[first],order[next]);
  }
    }
  gus_sample_ram ram{ram_kib};
  gus_residency result{
    .addresses{},
    .remaining{0},
  };
  result.addresses.resize(sizes.size());
  for(auto const index : order) if(sizes[index]) result.addresses[index] = ram.allocate(sizes[index]);
  result.remaining = ram.remaining();
  return result;
}

} // namespace tfx::audio
