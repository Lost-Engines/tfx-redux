#include "cursor.h"
#include <algorithm>
#include <stdexcept>
#include "render/sprites.h"

namespace tfx::presentation {
std::array<pointer_style,2> load_pointers(std::span<uint8_t const> const executable) {
  resources::binary_view const view{executable};
  unsigned int constexpr data{973476}, idle_table{0xdf08}, active_table{0xdf30}, record_bytes{20};
  auto const shape{[&](unsigned int const record){
    unsigned int const pointer{data + view.le32(record)};
    if(view.byte(pointer) != 1) throw std::runtime_error{"unexpected native menu pointer frame count"};
    unsigned int const frame{view.le16(pointer + 1)};
    unsigned int const pixels{view.byte(pointer + frame) * 2u * view.byte(pointer + frame + 1)};
    auto decoded{resources::decode_sprites(view.slice(pointer,frame + 2 + pixels))};
    return pointer_shape{
      .image{std::move(decoded.front())},
      .adjustment{view.signed_le16(record + 8),view.signed_le16(record + 10)},
    };
  }};
  std::array<pointer_style,2> result;
  for(unsigned int index{0}; index != result.size(); ++index) result[index] = {
    .idle{shape(data + idle_table + index * record_bytes)},
    .active{shape(data + active_table + index * record_bytes)},
  };
  return result;
}
void draw_pointer(render::framebuffer &target, pointer_shape const &shape, vec2<int16_t> const &position) {
  render::draw_sprite(target,shape.image,vec2<int>{position} + vec2<int>{shape.adjustment});
}
} // namespace tfx::presentation
