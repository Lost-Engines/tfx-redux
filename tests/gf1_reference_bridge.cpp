#include "audio/gf1/device.h"

// The Python original-driver host supplies DOS DMA bytes and advances the
// actual chip. No sample scheduling or driver instructions live in this bridge.
extern "C" {
using dma_callback = void (*)(void*, unsigned int);
void *gf1_new(dma_callback const callback, unsigned int const ram_kib) {
  return new tfx::audio::gf1_device{[callback](std::span<std::byte> const target){
    callback(target.data(),static_cast<unsigned int>(target.size()));
  },ram_kib};
}
void gf1_delete(void *const device) {
  delete static_cast<tfx::audio::gf1_device*>(device);
}
unsigned int gf1_read(void *const device, unsigned int const port, unsigned int const size) {
  return static_cast<tfx::audio::gf1_device*>(device)->read(port,size);
}
void gf1_write(void *const device, unsigned int const port, unsigned int const size, unsigned int const value) {
  static_cast<tfx::audio::gf1_device*>(device)->write(port,size,value);
}
void gf1_dma_count(void *const device, unsigned int const count) {
  static_cast<tfx::audio::gf1_device*>(device)->dma_count(count);
}
unsigned int gf1_advance(void *const device, double const milliseconds) {
  return static_cast<tfx::audio::gf1_device*>(device)->advance(milliseconds);
}
void gf1_sample(void *const device, int16_t *const output) {
  auto const sample{static_cast<tfx::audio::gf1_device*>(device)->sample()};
  output[0] = sample[0];
  output[1] = sample[1];
}
}
