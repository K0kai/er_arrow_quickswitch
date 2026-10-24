#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace arrow_switch::item_icons {
using Logger=void(*)(const char*,...);
// Reserved IDs for exact named sprites in the installed menu atlas.
constexpr unsigned menuSlotBase=0xF0000001u;
constexpr unsigned menuSelectionWide=0xF0000002u;
constexpr unsigned menuArrowQuantity=0xF0000003u;
constexpr unsigned menuBoltQuantity=0xF0000004u;
struct Image { unsigned width=128,height=128,contentX=0,contentY=0,contentWidth=128,contentHeight=128;
    std::vector<std::uint8_t> rgba; };
// Icons are extracted in a background worker and cached as small RGBA images.
// No game textures are redistributed and no game files are written.
void initialize(const std::filesystem::path& gameDirectory,Logger logger);
void set_override(const std::filesystem::path& directory);
std::shared_ptr<const Image> request(unsigned iconId);
void prefetch(const std::vector<unsigned>& iconIds);
std::uint64_t revision();
// Same extraction path, synchronously, for the standalone verification tool.
std::shared_ptr<const Image> extract(const std::filesystem::path& gameDirectory,
    const std::filesystem::path& overrideDirectory,unsigned iconId,Logger logger=nullptr);
}
