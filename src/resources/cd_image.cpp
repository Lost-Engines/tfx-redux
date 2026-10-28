#include "cd_image.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include "archive.h"

namespace tfx::resources {
namespace {

std::string uppercase(std::string value) {
  for(auto &c : value) if(c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  return value;
}
uint32_t timestamp(std::string const &text) {
  std::array<unsigned int,3> parts{};
  size_t at{0};
  for(unsigned int index{0}; index != parts.size(); ++index) {
    auto const end{index == 2 ? text.size() : text.find(':',at)};
    if(end == std::string::npos || end == at) throw std::runtime_error{"invalid CUE timestamp: " + text};
    auto const parsed{std::from_chars(text.data() + at,text.data() + end,parts[index])};
    if(parsed.ec != std::errc{} || parsed.ptr != text.data() + end) throw std::runtime_error{"invalid CUE timestamp: " + text};
    at = end + 1;
  }
  uint64_t const sectors{(uint64_t{parts[0]} * 60 + parts[1]) * cd_sectors_per_second + parts[2]};
  if(parts[1] >= 60 || parts[2] >= cd_sectors_per_second || sectors > std::numeric_limits<uint32_t>::max()) throw std::runtime_error{"CUE timestamp exceeds its range"};
  return static_cast<uint32_t>(sectors);
}
struct cue_entry {
  cd_track track;
  std::optional<uint32_t> index0{};
  std::optional<uint32_t> index1{};
  uint32_t pregap{0};
  uint32_t postgap{0};
};

} // namespace

cd_image::cd_image(std::filesystem::path const &cue) {
  std::ifstream input{cue};
  if(!input) throw std::runtime_error{"cannot open CUE: " + cue.string()};
  std::vector<cue_entry> entries;
  std::filesystem::path file;
  for(std::string line; std::getline(input,line);) {
    std::istringstream fields{line};
    std::string command;fields >> command;command = uppercase(command);
    if(command == "FILE") {
      std::string filename,type;
      fields >> std::quoted(filename,'"','\0') >> type;
      if(filename.empty() || uppercase(type) != "BINARY") throw std::runtime_error{"CUE FILE must name a BINARY image"};
      std::ranges::replace(filename,'\\','/');
      file = cue.parent_path() / std::filesystem::path{filename};
    } else if(command == "TRACK") {
      unsigned int number{0};std::string type;fields >> number >> type;type = uppercase(type);
      if(file.empty() || !number || number > 99 || (!entries.empty() && number <= entries.back().track.number)) throw std::runtime_error{"invalid CUE track number/order"};
      if(type != "AUDIO" && type != "MODE1/2352" && type != "MODE1/2048") throw std::runtime_error{"unsupported CUE track mode: " + type};
      entries.push_back({
        .track{
          .number{number},
          .audio{type == "AUDIO"},
          .file{file},
          .sector_bytes{type == "MODE1/2048" ? cd_data_bytes : cd_sector_bytes},
          .file_sector{0},
          .sectors{0},
          .disc_sector{0},
        },
      });
    } else if(command == "INDEX" || command == "PREGAP" || command == "POSTGAP") {
      if(entries.empty()) throw std::runtime_error{"CUE index/gap precedes its track"};
      unsigned int index{0};std::string time;
      if(command == "INDEX") fields >> index;
      fields >> time;
      auto const value{timestamp(time)};
      auto &entry{entries.back()};
      if(command == "PREGAP") entry.pregap = value;
      else if(command == "POSTGAP") entry.postgap = value;
      else if(index == 0) entry.index0 = value;
      else if(index == 1) {
        if(entry.index1) throw std::runtime_error{"duplicate CUE INDEX 01"};
        entry.index1 = value;
      }
    } else if(!command.empty() && command != "REM" && command != "CATALOG" && command != "TITLE" && command != "PERFORMER" && command != "SONGWRITER" && command != "ISRC" && command != "FLAGS" && command != "CDTEXTFILE") {
      throw std::runtime_error{"unsupported CUE directive: " + command};
    }
  }
  if(entries.empty()) throw std::runtime_error{"CUE contains no tracks"};
  uint64_t file_base{0}, inserted{0}, previous_file_sectors{0};
  std::filesystem::path previous;
  unsigned int previous_sector_size{0};
  for(unsigned int index{0}; index != entries.size(); ++index) {
    auto &entry{entries[index]};auto &track{entry.track};
    if(!entry.index1) throw std::runtime_error{"CUE track has no INDEX 01"};
    auto const length{std::filesystem::file_size(track.file)};
    if(length % track.sector_bytes || length / track.sector_bytes > std::numeric_limits<uint32_t>::max()) throw std::runtime_error{"CD image has a partial sector or exceeds its range: " + track.file.string()};
    auto const file_sectors{length / track.sector_bytes};
    if(previous != track.file) {
      file_base += previous_file_sectors;
      previous = track.file;previous_file_sectors = file_sectors;previous_sector_size = track.sector_bytes;
    } else if(previous_sector_size != track.sector_bytes) throw std::runtime_error{"mixed CUE sector sizes in one FILE"};
    uint64_t end{file_sectors};
    if(index + 1 != entries.size() && entries[index + 1].track.file == track.file) {
      auto const &next{entries[index + 1]};
      if(!next.index1) throw std::runtime_error{"CUE track has no INDEX 01"};
      end = next.index0.value_or(*next.index1);
    }
    inserted += entry.pregap;
    uint64_t const position{file_base + inserted + *entry.index1};
    if(*entry.index1 >= end || end > file_sectors || position > std::numeric_limits<uint32_t>::max() || (entry.index0 && *entry.index0 > *entry.index1)) throw std::runtime_error{"CUE track/index exceeds its image"};
    track.file_sector = *entry.index1;
    track.sectors = static_cast<uint32_t>(end - *entry.index1);
    track.disc_sector = static_cast<uint32_t>(position);
    tracks_.push_back(std::move(track));
    inserted += entry.postgap;
  }
}
std::span<cd_track const> cd_image::tracks() const {
  return tracks_;
}
cd_track const &cd_image::track(unsigned int const number) const {
  auto const found{std::ranges::find(tracks_,number,&cd_track::number)};
  if(found == tracks_.end()) throw std::runtime_error{"CD track not present: " + std::to_string(number)};
  return *found;
}
bytes cd_image::read_file(std::string_view const name) const {
  auto const &data{track(1)};
  if(data.audio) throw std::runtime_error{"CD track 1 has no ISO data"};
  std::ifstream input{data.file,std::ios::binary};
  auto const read{[&](uint32_t const sector, uint32_t const count) {
    if(sector > data.sectors || (uint64_t{count} + cd_data_bytes - 1) / cd_data_bytes > data.sectors - sector) throw std::runtime_error{"ISO extent exceeds CD data track"};
    bytes result(count);std::array<uint8_t,cd_sector_bytes> buffer{};
    for(uint32_t at{0}; at < count;) {
      input.seekg(static_cast<std::streamoff>((uint64_t{data.file_sector} + sector + at / cd_data_bytes) * data.sector_bytes));
      if(!input.read(reinterpret_cast<char*>(buffer.data()),data.sector_bytes)) throw std::runtime_error{"cannot read CD data sector"};
      if(data.sector_bytes == cd_sector_bytes && (buffer[0] != 0 || buffer[11] != 0 || buffer[15] != 1 || !std::ranges::all_of(std::span{buffer}.subspan(1,10),[](auto v) { return v == 255; }))) throw std::runtime_error{"invalid Mode 1 CD sector"};
      auto const length{std::min(count - at,cd_data_bytes)};
      std::copy_n(buffer.begin() + (data.sector_bytes == cd_sector_bytes ? 16 : 0),length,result.begin() + at);
      at += length;
    }
    return result;
  }};
  auto const descriptor{read(16,cd_data_bytes)};
  if(descriptor[0] != 1 || !std::equal(descriptor.begin() + 1,descriptor.begin() + 7,"CD001\1")) throw std::runtime_error{"CD has no primary ISO 9660 descriptor"};
  bytes directory(descriptor.begin() + 156,descriptor.begin() + 190);
  std::istringstream components{std::string{name}};
  std::vector<std::string> names;
  for(std::string component; std::getline(components,component,'/');) {
    if(component.empty() || component == "." || component == "..") throw std::runtime_error{"invalid ISO resource name"};
    names.push_back(uppercase(component));
  }
  if(names.empty()) throw std::runtime_error{"empty ISO resource name"};
  for(unsigned int part{0}; part != names.size(); ++part) {
    binary_view const entry{directory};
    auto const contents{read(entry.le32(2),entry.le32(10))};
    bool found{false};
    for(size_t at{0}; at < contents.size();) {
      auto const length{contents[at]};
      if(!length) { at = (at / cd_data_bytes + 1) * cd_data_bytes;continue; }
      binary_view const child{std::span{contents}.subspan(at)};
      if(length < 34 || length > contents.size() - at || child.byte(32) > length - 33) throw std::runtime_error{"invalid ISO directory record"};
      auto const raw{child.slice(33,child.byte(32))};
      std::string filename{raw.begin(),raw.end()};
      filename.resize(filename.find(';') == std::string::npos ? filename.size() : filename.find(';'));
      if(uppercase(filename) == names[part]) {
        if(child.byte(25) & 128) throw std::runtime_error{"multi-extent ISO files are unsupported"};
        if(part + 1 == names.size()) {
          if(child.byte(25) & 2) throw std::runtime_error{"ISO resource names a directory"};
          return read(child.le32(2),child.le32(10));
        }
        if(!(child.byte(25) & 2)) throw std::runtime_error{"ISO path component is not a directory"};
        directory.assign(contents.begin() + static_cast<ptrdiff_t>(at),contents.begin() + static_cast<ptrdiff_t>(at + length));
        found = true;break;
      }
      at += length;
    }
    if(!found) throw std::runtime_error{"file not present on CD: " + std::string{name}};
  }
  throw std::runtime_error{"file not present on CD: " + std::string{name}};
}

void install_reference_cd(std::filesystem::path const &cue, std::filesystem::path const &destination) {
  cd_image const disc{cue};
  auto archive_bytes{disc.read_file("DID.DAT")};
  auto const installer{disc.read_file("DISK1.DAT")};
  unsigned int constexpr reference_archive_size{11509643}, reference_installer_size{1422373}, reference_resources{3488};
  if(archive_bytes.size() != reference_archive_size || installer.size() != reference_installer_size || archive{archive_bytes}.entries().size() != reference_resources) throw std::runtime_error{"automatic installation requires the TFX 2.05 reference CD"};
  struct member {
    std::string_view name;
    unsigned int offset;
    unsigned int bytes;
  };
  // The 2.05 DISK1.DAT is a concatenated installer payload, not a 7z/RA archive.
  std::array constexpr members{
    member{
      .name{"TFX.DAT"},
      .offset{31610},
      .bytes{1032503},
    },
    member{
      .name{"START.EXE"},
      .offset{1102239},
      .bytes{36288},
    },
    member{
      .name{"GUS.COM"},
      .offset{1078863},
      .bytes{23376},
    },
    member{
      .name{"TFX.ADV"},
      .offset{1420861},
      .bytes{258},
    },
  };
  binary_view const payload{installer};
  std::filesystem::create_directories(destination);
  auto const write{[&](std::string_view const name, std::span<uint8_t const> const bytes) {
    auto const path{destination / name};
    if(std::filesystem::is_regular_file(path) && resources::read_file(path) == resources::bytes{bytes.begin(),bytes.end()}) return;
    auto temporary{path};temporary += ".installing";
    {
      std::ofstream output{temporary,std::ios::binary | std::ios::trunc};
      if(!output || !output.write(reinterpret_cast<char const*>(bytes.data()),static_cast<std::streamsize>(bytes.size())) || !output.flush()) throw std::runtime_error{"cannot install " + path.string()};
    }
    std::filesystem::copy_file(temporary,path,std::filesystem::copy_options::overwrite_existing);
    std::filesystem::remove(temporary);
  }};
  for(auto const &file : members) write(file.name,payload.slice(file.offset,file.bytes));
  for(auto const *name : {"TUNES/INTRO.ADL","TUNES/INTRO.RLD","TUNES/INTRO.SCC"}) {
    auto const bytes{disc.read_file(name)};
    std::filesystem::create_directories(destination / "TUNES");write(name,bytes);
  }
  if(!std::filesystem::exists(destination / "TFX.CFG")) {
    std::string_view constexpr defaults{"SOUNDFX=1\r\nMUSIC=1\r\nLANG=1\r\nCONTROL=0\r\nMODEL=3\r\nDETAIL=2\r\n"};
    write("TFX.CFG",{reinterpret_cast<uint8_t const*>(defaults.data()),defaults.size()});
  }
  write("DID.DAT",archive_bytes); // Publish the archive only after its dependencies.
}

} // namespace tfx::resources
