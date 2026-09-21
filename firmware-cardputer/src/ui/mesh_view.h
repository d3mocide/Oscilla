/* mesh_view.h — passive 802.15.4 PAN/node renderer. */
#pragma once
#include <cstddef>
#include "model/mesh_model.h"
#include "ui/chrome.h"
namespace ui { void drawMeshView(const model::MeshModel &mesh, size_t cursor, const ChromeState &chrome); }
