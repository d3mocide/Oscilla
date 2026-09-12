/*
 * ocp_item.h — one parsed OCP item. Framework-agnostic (DESIGN §7.1).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ocp {

enum class ItemKind : uint8_t { Frame, Event, Error, Pong, Noise };

struct KeyValue {
    std::string key;
    std::string value;   /* decoded bytes */
};

struct Item {
    ItemKind kind = ItemKind::Noise;

    std::string tag;                 /* Frame */
    bool compact = false;            /* Frame */
    std::vector<KeyValue> kv;        /* Frame/Event/Error; one per key, last wins */
    std::vector<std::string> rows;   /* Frame (block): raw row text, undecoded */

    std::string text;                /* Noise: the offending line */
    const char *reason = "";         /* Noise */

    /* nullptr if absent. */
    const std::string *get(const char *key) const;
};

}  // namespace ocp
