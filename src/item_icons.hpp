#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace arrow_switch::item_icons {
using Logger=void(*)(const char*,...);
struct Image { unsigned width=128,height=128;std::vector<std::uint8_t> rgba; };
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
