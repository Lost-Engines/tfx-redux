#pragma once

#include "models/shape.h"
#include "render/camera.h"
#include "render/colour_remap.h"

namespace tfx::render {

// Restore native camera coordinates and project/near-clip an unshaded face.
std::optional<std::vector<vec2<int16_t>>> project_model_face(std::span<models::model_vertex const> vertices, camera const &view);

// Project flat/Gouraud model faces through native Q16 projection and integer screen
// clipping, including native near-plane crossings. Returns false for inspection
// cameras, out-of-range coordinates or the separate far-plane path; no pixels
// are changed in that case.
bool integer_model_polygon(framebuffer &output, std::span<models::model_vertex const> vertices, camera const &view, uint8_t colour, colour_map const *remap = nullptr);

// Original opcode-21 branch and polygon winding are distinct tests. Continuous
// cameras test the face plane before perspective division, including vertices
// behind the eye. Null means the supplied vertices cannot be evaluated.
std::optional<bool> model_facing(std::span<models::model_vertex const> vertices, camera const &view, models::facing_test purpose);

} // namespace tfx::render
