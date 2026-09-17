/* mesh_view.h — passive 802.15.4 PAN/node renderer. */
#pragma once
#include <cstddef>
#include <string>
#include "model/mesh_model.h"
namespace ui { void drawMeshView(const model::MeshModel &mesh, size_t cursor, const std::string &notice); }
