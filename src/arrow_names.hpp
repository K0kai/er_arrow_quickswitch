#pragma once
#include "game_access.hpp"
#include <string>

namespace arrow_switch {
// FMG layout follows the checked message-repository lookup in game 1.17.1.
// This uses checked reads only; no extra game function or message hook is needed.
inline std::wstring fmg_name(std::uintptr_t fmg,std::uint32_t id) {
    std::uint32_t groups{};
    if (!fmg || !game::read(fmg+0xC,groups) || !groups || groups>100000) return {};
    auto strings=game::pointer(fmg+0x18);
    if (!strings) return {};
    std::uint32_t lo=0,hi=groups;
    while (lo<hi) {
        auto mid=lo+(hi-lo)/2;
        std::uint32_t data[4]{};
        if (!game::read(fmg+0x28+mid*0x10,data)) return {};
        const auto offsetIndex=data[0],first=data[1],last=data[2];
        if (first>last) return {};
        if (id<first) hi=mid;
        else if (id>last) lo=mid+1;
        else {
            const std::uint64_t index=std::uint64_t(offsetIndex)+id-first;
            if (index>1000000) return {};
            std::uint64_t offset{};
            if (!game::read(strings+index*8,offset) || !offset || offset>0x4000000) return {};
            std::wstring text;
            for (std::size_t i=0;i<160;++i) {
                wchar_t ch{};
                if (!game::read(fmg+offset+i*2,ch)) return {};
                if (!ch) return text;
                if (ch>=32) text.push_back(ch);
            }
            return text;
        }
    }
    return {};
}

inline std::wstring arrow_name(std::uintptr_t repositoryStatic,std::uint32_t id) {
    auto repo=game::pointer(repositoryStatic);
    std::uint32_t versions{},categories{};
    if (!repo || !game::read(repo+0x10,versions) || !versions || versions>16 ||
        !game::read(repo+0x14,categories) || !categories || categories>4096) return {};
    auto versionTables=game::pointer(repo+8);
    if (!versionTables) return {};
    // WeaponName patch, base, Shadow of the Erdtree, second DLC table.
    constexpr unsigned tableIds[]={115,11,310,410};
    for (auto category:tableIds) {
        if (category>=categories) continue;
        for (std::uint32_t version=0;version<versions;++version) {
            auto table=game::pointer(versionTables+version*8);
            auto fmg=table ? game::pointer(table+category*8) : 0;
            auto text=fmg_name(fmg,id);
            while (!text.empty() && text.back()==L' ') text.pop_back();
            if (!text.empty()) return text;
        }
    }
    return {};
}
}
