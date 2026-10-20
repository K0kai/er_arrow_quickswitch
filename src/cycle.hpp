#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace arrow_switch {
inline bool is_arrow_type(std::uint16_t type) { return type == 81 || type == 83; }
inline bool is_ammo_type(std::uint16_t type) { return is_arrow_type(type) || type == 85 || type == 86; }
inline bool plausible_param_name_offset(std::uint32_t offset) { return offset > 0 && offset <= 0x4000000; }
struct Arrow {
    std::uint32_t id, handle, count, index;
    std::uint16_t type;
};
// Optionally match the equipped arrow family (normal vs great arrows).
// Unknown or empty current equipment cannot establish a starting position.
inline const Arrow* next(const std::vector<Arrow>& arrows, std::uint32_t current,
                         int direction=1, std::uint32_t excluded=0xFFFFFFFFu,
                         bool sameFamily=true) {
    auto at = std::find_if(arrows.begin(), arrows.end(), [=](const Arrow& a) { return a.id == current; });
    if (at == arrows.end() || !is_arrow_type(at->type)) return nullptr;
    const auto start = static_cast<std::size_t>(at - arrows.begin());
    for (std::size_t n = 1; n < arrows.size(); ++n) {
        const auto pos = direction < 0 ? (start + arrows.size() - n) % arrows.size() : (start + n) % arrows.size();
        const auto& a = arrows[pos];
        if (a.count && is_arrow_type(a.type) && (!sameFamily || a.type == at->type) &&
            a.id != current && a.id != excluded) return &a;
    }
    return nullptr;
}
}
