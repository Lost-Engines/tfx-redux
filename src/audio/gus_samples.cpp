#include "gus_samples.h"
#include <algorithm>
#include <bit>
#include <mutex>
#include <utility>
#include "audio/gf1/device.h"
#include "audio/gus_memory.h"

namespace tfx::audio {

struct gus_samples::implementation {
  static unsigned int constexpr chip_rate{44100}, ram_kib{1024};
  std::vector<gus_sample_data> samples;
  std::vector<gus_sample_descriptor> descriptors;
  gus_residency residency;
  gus_scheduler scheduler;
  std::span<uint8_t const> dma_source;
  gf1_device chip;
  gus_driver driver;
  struct voice_queue {
    std::vector<unsigned int> items;
    unsigned int started{0};
  };
  std::array<voice_queue,gus_scheduler::voice_count> queues;
  std::mutex mutex;
  unsigned int const output_rate;
  uint64_t phase{0};
  std::array<int16_t,2> held{};
  bool primed{false};

  static gus_residency plan(std::span<gus_sample_data const> const samples) {
    std::vector<uint32_t> sizes;
    unsigned int constexpr maximum_sample_bytes{65535};
    for(auto const &sample : samples) {
      if(sample.pcm.size() > maximum_sample_bytes) throw std::out_of_range{"Gravis sample exceeds its native word length"};
      sizes.push_back(static_cast<uint32_t>(sample.pcm.size()));
    }
    return plan_gus_residency(sizes,ram_kib);
  }
  static std::vector<gus_sample_descriptor> describe(std::span<gus_sample_data const> const samples) {
    std::vector<gus_sample_descriptor> result;
    for(auto const &sample : samples) result.push_back({
      .sample{
        .retrigger_delay{sample.retrigger_delay},
        .samples{static_cast<uint32_t>(sample.pcm.size())},
        .available{!sample.pcm.empty()},
      },
      .resident{true},
    });
    return result;
  }
  void byte(gus_register const address, uint8_t const value) {
    chip.write(gus_driver::register_select,1,std::to_underlying(address));
    chip.write(gus_driver::data_high,1,value);
  }
  void word(gus_register const address, uint16_t const value) {
    chip.write(gus_driver::register_select,1,std::to_underlying(address));
    chip.write(gus_driver::data_low,2,value);
  }
  implementation(std::span<gus_sample_data const> const source, gus_parameters const &parameters, unsigned int const output_rate)
    : samples{source.begin(),source.end()}, descriptors{describe(source)}, residency{plan(source)}, scheduler{descriptors},
      chip{[this](std::span<std::byte> const target){
        if(target.size() != dma_source.size()) throw std::runtime_error{"Gravis DMA transfer has the wrong length"};
        std::ranges::copy(std::as_bytes(dma_source),target.begin());
      },ram_kib},
      driver{parameters,{
        .read{[this](uint16_t const port, uint8_t const size){ return static_cast<uint16_t>(chip.read(port,size)); }},
        .write{[this](gus_port_write const &event){ chip.write(event.port,event.size,event.value); }},
      }}, output_rate{output_rate} {
    if(!output_rate) throw std::invalid_argument{"PCM output rate must not be zero"};
    chip.write(gus_driver::card_base,1,0x0b);
    byte(gus_register::active_voices,0xdf);                                    // Original driver activates all 32 physical oscillators.
    for(unsigned int voice{0}; voice != gus_driver::physical_voices; ++voice) {
      chip.write(gus_driver::voice_select,1,voice);
      byte(gus_register::voice_control,3);
      byte(gus_register::ramp_control,3);
      word(gus_register::frequency,1024);
      word(gus_register::loop_begin_high,0);
      word(gus_register::loop_begin_low,0);
      word(gus_register::loop_end_high,0);
      word(gus_register::loop_end_low,0);
      byte(gus_register::ramp_rate,1);
      byte(gus_register::ramp_begin,16);
      byte(gus_register::ramp_end,224);
      word(gus_register::volume,0);
      word(gus_register::current_high,0);
      word(gus_register::current_low,0);
      byte(gus_register::pan,7);
    }
    // Preserve the loader's descending pairwise order, including displaced ties.
    std::vector<unsigned int> order;
    for(unsigned int index{0}; index != samples.size(); ++index) if(!samples[index].pcm.empty()) {
      if(!residency.addresses[index]) throw std::runtime_error{"Gravis sample bank exceeds one MiB of resident RAM"};
      if(!samples[index].rate) throw std::invalid_argument{"Gravis sample rate must not be zero"};
      order.push_back(index);
    }
    std::ranges::sort(order,[this](unsigned int const a, unsigned int const b){ return *residency.addresses[a] < *residency.addresses[b]; });
    for(auto const index : order) {
      dma_source = samples[index].pcm;
      chip.dma_count(static_cast<unsigned int>(dma_source.size() - 1));
      // GUS.COM discards the low four destination bits, even though its RAM
      // allocator leaves samples unaligned. Preserve that native overwrite.
      chip.write(gus_driver::register_select,1,0x42);
      chip.write(gus_driver::data_low,2,*residency.addresses[index] >> 4);
      chip.write(gus_driver::register_select,1,0x41);
      chip.write(gus_driver::data_high,1,0xa1);                                // Eight-bit DMA, unsigned source -> signed GF1 RAM.
      static_cast<void>(chip.advance(1));
      static_cast<void>(chip.read(gus_driver::data_high,1));
    }
    dma_source = {};
    for(unsigned int voice{0}; voice != queues.size(); ++voice) driver.volume(voice,gus_parameters::maximum_volume);
    static_cast<void>(chip.advance(1));
  }
  void next(unsigned int const voice) {
    auto &queue{queues.at(voice)};
    if(queue.started == queue.items.size()) return;
    auto const index{queue.items[queue.started++]};
    auto const begin{*residency.addresses[index]};
    // The real-mode queue stores the native MOVSX result, including high rates.
    auto const rate{static_cast<uint32_t>(static_cast<int32_t>(std::bit_cast<int16_t>(samples[index].rate)))};
    driver.play(voice,begin,begin + static_cast<uint32_t>(samples[index].pcm.size()),rate);
  }
  std::array<int16_t,2> sample() {
    auto const output{chip.sample()};
    if(chip.advance(1000.0 / chip_rate)) {
      unsigned int constexpr voice_irq_mask{0x60}, wave_inactive_bit{0x80}, voice_mask{31};
      while(chip.read(gus_driver::card_base + 6,1) & voice_irq_mask) {
        chip.write(gus_driver::register_select,1,0x8f);
        auto const status{chip.read(gus_driver::data_high,1)};
        auto const voice{status & voice_mask};
        if(!(status & wave_inactive_bit) && voice < queues.size()) next(voice);
      }
    }
    return output;
  }
};

gus_samples::gus_samples(std::span<gus_sample_data const> const samples, gus_parameters const &parameters, unsigned int const output_rate)
  : state{std::make_unique<implementation>(samples,parameters,output_rate)} {}
gus_samples::~gus_samples() = default;
void gus_samples::enable(bool const enabled) {
  std::lock_guard lock{state->mutex};
  state->scheduler.enable(enabled);
}
void gus_samples::advance(simulation::timer_ticks const ticks) {
  std::lock_guard lock{state->mutex};
  state->scheduler.advance(ticks);
}
void gus_samples::submit(sample_priority const priority, std::span<unsigned int const> const sequence) {
  auto &s{*state};
  std::lock_guard lock{s.mutex};
  std::array<bool,gus_scheduler::voice_count> stopped{};
  for(unsigned int voice{0}; voice != stopped.size(); ++voice) stopped[voice] = s.driver.stopped(voice);
  if(auto request{s.scheduler.submit(priority,sequence,stopped)}) {
    s.driver.stop(request->voice);
    s.queues[request->voice] = {
      .items{std::move(request->sequence)},
      .started{0},
    };
    s.next(request->voice);
  }
}
void gus_samples::render(std::span<float> const stereo) {
  if(stereo.size() % 2) throw std::invalid_argument{"PCM output needs complete stereo frames"};
  auto &s{*state};
  std::lock_guard lock{s.mutex};
  for(unsigned int frame{0}; frame != stereo.size() / 2; ++frame) {
    // Keep the fixed GF1 mixer clock across callbacks; adapt its sample holds
    // to the host device rate without changing native oscillator timing.
    if(!s.primed) { s.held = s.sample();s.primed = true; }
    while(s.phase >= s.output_rate) { s.phase -= s.output_rate;s.held = s.sample(); }
    stereo[frame * 2] += s.held[0] / 32768.0f;
    stereo[frame * 2 + 1] += s.held[1] / 32768.0f;
    s.phase += implementation::chip_rate;
  }
}
gus_playback_state gus_samples::value() {
  auto &s{*state};
  std::lock_guard lock{s.mutex};
  gus_playback_state result{
    .scheduling{s.scheduler.value()},
    .pending{},
    .started{},
    .cooldowns{s.scheduler.remaining().begin(),s.scheduler.remaining().end()},
  };
  for(unsigned int voice{0}; voice != s.queues.size(); ++voice) {
    result.pending[voice] = static_cast<unsigned int>(s.queues[voice].items.size()) - s.queues[voice].started;
    result.started[voice] = s.queues[voice].started;
  }
  return result;
}

} // namespace tfx::audio
