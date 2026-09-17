/* mesh_model.h — deck-side passive 802.15.4 PAN/node model (OCP-SPEC §12). */
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "ocp/ocp_item.h"

namespace model {
struct MeshPan { std::string pan, proto, channels; uint16_t nodes = 0; int rssi = 0; uint8_t lqi = 0; };
struct MeshNode { std::string pan, short_addr, ext, role; int rssi = 0; uint8_t lqi = 0; uint32_t seen = 0; };
class MeshModel {
public:
    static constexpr size_t kMaxPans = 32, kMaxNodes = 96;
    void begin(); void stop(); void clear(); void absorb(const ocp::Item &frame);
    bool active() const { return active_; } uint16_t malformedRows() const { return malformed_; }
    const std::vector<MeshPan> &pans() const { return pans_; } const std::vector<MeshNode> &nodes() const { return nodes_; }
private:
    bool active_ = false; uint16_t malformed_ = 0; std::vector<MeshPan> pans_; std::vector<MeshNode> nodes_;
};
}  // namespace model
