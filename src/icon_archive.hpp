#pragma once
#include "icon_formats.hpp"
#include <filesystem>

namespace arrow_switch::item_icons {
// Reads only named menu assets. Archive indexes and encryption metadata are
// taken from the installed Data0.bhd, never from fixed BDT offsets.
class Archive {
    std::filesystem::path gameRoot_,overrideRoot_;
    Bytes header_;
    const char* source_="none";
    bool index();
    bool packed(const std::string& relative,Bytes& result);
public:
    Archive(std::filesystem::path gameRoot,std::filesystem::path overrideRoot={}):
        gameRoot_(std::move(gameRoot)),overrideRoot_(std::move(overrideRoot)){}
    bool read(const std::string& relative,Bytes& result);
    const char* source() const { return source_; }
    bool unpack(Bytes& bytes);
};
}
