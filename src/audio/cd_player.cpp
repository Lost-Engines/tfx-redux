#include "cd_player.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <condition_variable>
#include <cstring>
#include <map>
#include <mutex>
#include <optional>
#include <thread>
#include <miniaudio.h>
#include "resources/cd_image.h"

namespace tfx::audio {
namespace {

unsigned int constexpr channels{resources::cd_channels};
unsigned int constexpr buffer_frames{16384};
unsigned int constexpr decode_frames{2048};

struct image_pcm {
  std::ifstream file;
  uint64_t offset;
  uint64_t length;
  uint64_t cursor{0};
  std::array<uint8_t,44> header{};

  explicit image_pcm(resources::cd_track const &track) :
    file{track.file,std::ios::binary}, offset{uint64_t{track.file_sector} * track.sector_bytes},
    length{uint64_t{track.sectors} * track.sector_bytes} {
    if(!file || length > UINT32_MAX - header.size()) throw std::runtime_error{"cannot stream CD audio track"};
    auto const text{[&](unsigned int at, std::string_view value){ std::copy(value.begin(),value.end(),header.begin() + at); }};
    auto const word{[&](unsigned int at, uint32_t value, unsigned int bytes){
      for(unsigned int i{0}; i != bytes; ++i) header[at + i] = static_cast<uint8_t>(value >> (i * 8));
    }};
    // A virtual WAV header lets the same streaming decoder/resampler consume
    // the bounded CD-DA extent, without converting or copying the BIN to disk.
    text(0,"RIFF");word(4,static_cast<uint32_t>(length) + 36,4);text(8,"WAVEfmt ");word(16,16,4);
    word(20,1,2);word(22,channels,2);word(24,resources::cd_sample_rate,4);
    word(28,resources::cd_sample_rate * channels * resources::cd_sample_bytes,4);
    word(32,channels * resources::cd_sample_bytes,2);word(34,resources::cd_sample_bytes * 8,2);
    text(36,"data");word(40,static_cast<uint32_t>(length),4);
  }
  static ma_result read(ma_decoder *decoder, void *output, size_t requested, size_t *received) {
    auto &self{*static_cast<image_pcm*>(decoder->pUserData)};
    *received = 0;
    auto *bytes{static_cast<uint8_t*>(output)};
    if(self.cursor < self.header.size()) {
      auto const count{static_cast<size_t>(std::min<uint64_t>(requested,self.header.size() - self.cursor))};
      std::memcpy(bytes,self.header.data() + self.cursor,count);
      self.cursor += count;*received += count;requested -= count;bytes += count;
    }
    if(requested && self.cursor < self.header.size() + self.length) {
      auto const count{static_cast<size_t>(std::min<uint64_t>(requested,self.header.size() + self.length - self.cursor))};
      self.file.clear();self.file.seekg(static_cast<std::streamoff>(self.offset + self.cursor - self.header.size()));
      self.file.read(reinterpret_cast<char*>(bytes),static_cast<std::streamsize>(count));
      auto const actual{static_cast<size_t>(self.file.gcount())};
      self.cursor += actual;*received += actual;
      if(actual != count) return MA_IO_ERROR;
    }
    return *received ? MA_SUCCESS : MA_AT_END;
  }
  static ma_result seek(ma_decoder *decoder, ma_int64 offset, ma_seek_origin origin) {
    auto &self{*static_cast<image_pcm*>(decoder->pUserData)};
    auto const base{origin == ma_seek_origin_start ? int64_t{0} : origin == ma_seek_origin_current ? static_cast<int64_t>(self.cursor) : static_cast<int64_t>(self.header.size() + self.length)};
    if(offset < -base || offset > static_cast<int64_t>(self.header.size() + self.length) - base) return MA_BAD_SEEK;
    self.cursor = static_cast<uint64_t>(base + offset);return MA_SUCCESS;
  }
};

class streamed_track {
private:
  std::unique_ptr<image_pcm> image;
  ma_decoder decoder{};
  ma_pcm_rb ring{};
  bool decoder_ready{false};
  bool ring_ready{false};
  bool ended{false};
  std::mutex mutex;
  std::condition_variable_any changed;
  std::jthread worker;

  bool fill() {
    std::array<float,decode_frames * channels> pcm{};
    ma_uint64 frames{0};
    auto const result{ma_decoder_read_pcm_frames(&decoder,pcm.data(),decode_frames,&frames)};
    if(result != MA_SUCCESS && result != MA_AT_END) throw std::runtime_error{"CD audio decoding failed"};
    std::lock_guard lock{mutex};
    ma_uint32 at{0};
    while(at < frames) {
      ma_uint32 count{static_cast<ma_uint32>(frames - at)};void *destination{nullptr};
      ma_pcm_rb_acquire_write(&ring,&count,&destination);
      if(!count) throw std::runtime_error{"CD decode buffer exceeded its capacity"};
      std::memcpy(destination,pcm.data() + at * channels,count * channels * sizeof(float));
      ma_pcm_rb_commit_write(&ring,count);at += count;
    }
    if(result == MA_AT_END || frames < decode_frames) ended = true;
    return !ended;
  }
  void start() {
    if(ma_pcm_rb_init(ma_format_f32,channels,buffer_frames,nullptr,nullptr,&ring) != MA_SUCCESS) throw std::runtime_error{"cannot allocate CD audio buffer"};
    ring_ready = true;
    // Prime before publishing the new voice. The device callback never reads
    // files, seeks, initialises codecs or waits for a decoding thread to finish.
    while(ma_pcm_rb_available_write(&ring) >= decode_frames && fill()) {}
    worker = std::jthread{[this](std::stop_token stop){
      try {
        for(;;) {
          {
            std::unique_lock lock{mutex};
            changed.wait(lock,stop,[&]{ return ended || ma_pcm_rb_available_write(&ring) >= decode_frames; });
            if(stop.stop_requested() || ended) return;
          }
          if(!fill()) return;
        }
      } catch(...) {
        std::lock_guard lock{mutex};
        ended = true;
      }
    }};
  }
  void close() {
    worker.request_stop();changed.notify_all();
    if(worker.joinable()) worker.join();
    if(ring_ready) ma_pcm_rb_uninit(&ring);
    if(decoder_ready) ma_decoder_uninit(&decoder);
  }
public:
  streamed_track(std::filesystem::path const &file, unsigned int rate) {
    auto config{ma_decoder_config_init(ma_format_f32,channels,rate)};
#ifdef _WIN32
    auto const result{ma_decoder_init_file_w(file.c_str(),&config,&decoder)};
#else
    auto const result{ma_decoder_init_file(file.c_str(),&config,&decoder)};
#endif // _WIN32
    if(result != MA_SUCCESS) throw std::runtime_error{"cannot decode CD track: " + file.string()};
    decoder_ready = true;
    try { start(); } catch(...) { close();throw; }
  }
  streamed_track(resources::cd_track const &track, unsigned int rate) : image{std::make_unique<image_pcm>(track)} {
    auto config{ma_decoder_config_init(ma_format_f32,channels,rate)};
    auto const result{ma_decoder_init(image_pcm::read,image_pcm::seek,image.get(),&config,&decoder)};
    if(result != MA_SUCCESS) throw std::runtime_error{"cannot decode CD image track"};
    decoder_ready = true;
    try { start(); } catch(...) { close();throw; }
  }
  ~streamed_track() {
    close();
  }
  bool playing() {
    std::lock_guard lock{mutex};
    return !ended || ma_pcm_rb_available_read(&ring) != 0;
  }
  void render(std::span<float> stereo) {
    {
      std::lock_guard lock{mutex};
      unsigned int at{0};
      while(at < stereo.size() / channels) {
        ma_uint32 frames{static_cast<ma_uint32>(std::min<size_t>(stereo.size() / channels - at,UINT32_MAX))};void *source{nullptr};
        ma_pcm_rb_acquire_read(&ring,&frames,&source);
        if(!frames) break;
        auto const *pcm{static_cast<float const*>(source)};
        for(unsigned int i{0}; i != frames * channels; ++i) stereo[at * channels + i] += pcm[i];
        ma_pcm_rb_commit_read(&ring,frames);at += frames;
      }
    }
    changed.notify_all();
  }
};

std::string lowercase(std::string value) {
  for(auto &c : value) if(c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return value;
}
std::optional<unsigned int> track_number(std::filesystem::path const &path) {
  auto name{lowercase(path.stem().string())};
  if(name.starts_with("track")) name.erase(0,5);
  std::erase_if(name,[](char c){ return c == ' ' || c == '-' || c == '_'; });
  unsigned int number{0};
  auto const parsed{std::from_chars(name.data(),name.data() + name.size(),number)};
  if(parsed.ec != std::errc{} || parsed.ptr != name.data() + name.size() || !number || number > 99) return {};
  return number;
}

} // anonymous namespace

struct cd_player::implementation {
  std::optional<resources::cd_image> image;
  std::map<unsigned int,std::filesystem::path> files;
  unsigned int rate;
  mutable std::mutex mutex;
  std::unique_ptr<streamed_track> active;

  implementation(std::filesystem::path const &source, unsigned int output_rate) : rate{output_rate} {
    if(!rate) throw std::invalid_argument{"CD audio output rate must be positive"};
    if(!std::filesystem::is_directory(source)) image.emplace(source);
    else {
      std::array<std::string_view,6> constexpr extensions{".flac",".wav",".wave",".ogg",".oga",".mp3"};
      for(auto const extension : extensions) for(auto const &entry : std::filesystem::directory_iterator{source}) {
        if(!entry.is_regular_file() || lowercase(entry.path().extension().string()) != extension) continue;
        if(auto number{track_number(entry.path())}) {
          auto const [found,inserted]{files.emplace(*number,entry.path())};
          if(!inserted && lowercase(found->second.extension().string()) == extension) throw std::runtime_error{"ambiguous CD track number in " + source.string()};
        }
      }
      if(files.empty()) throw std::runtime_error{"no numbered CD audio tracks in " + source.string()};
    }
  }
};
cd_player::cd_player(std::filesystem::path const &source, unsigned int const output_rate) : state{std::make_unique<implementation>(source,output_rate)} {
}
cd_player::~cd_player() = default;
bool cd_player::contains(unsigned int number) const {
  if(!state->image) return state->files.contains(number);
  auto const tracks{state->image->tracks()};
  return std::ranges::any_of(tracks,[&](auto const &track){ return track.number == number && track.audio; });
}
bool cd_player::play(unsigned int number) {
  stop();
  if(!contains(number)) return false;
  auto voice{state->image ? std::make_unique<streamed_track>(state->image->track(number),state->rate) : std::make_unique<streamed_track>(state->files.at(number),state->rate)};
  std::lock_guard lock{state->mutex};
  state->active = std::move(voice);
  return true;
}
void cd_player::stop() {
  std::unique_ptr<streamed_track> previous;
  { std::lock_guard lock{state->mutex};previous = std::move(state->active); }
  // Join the old decoding worker outside the callback's short mixing lock.
}
bool cd_player::playing() const {
  std::lock_guard lock{state->mutex};
  return state->active && state->active->playing();
}
void cd_player::render(std::span<float> stereo) {
  if(stereo.size() % channels) throw std::invalid_argument{"CD output requires complete stereo frames"};
  std::lock_guard lock{state->mutex};
  if(state->active) state->active->render(stereo);
}

} // namespace tfx::audio
