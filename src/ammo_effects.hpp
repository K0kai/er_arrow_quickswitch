#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace arrow_switch::effects {
// Bit positions match the twelve tiles in assets/wheel-game-icons.rgba.
enum Kind : std::uint8_t {
    magic=0,fire=1,lightning=2,holy=3,rot=4,poison=5,bleed=6,sleep=7,
    frost=8,physical=9,madness=10,death=11
};
using Mask=std::uint16_t;
constexpr Mask bit(Kind kind){return static_cast<Mask>(1u<<kind);}
struct Evidence {
    std::array<std::uint16_t,5> damage{}; // physical, magic, fire, lightning, holy
    std::array<std::int32_t,7> buildup{}; // poison, rot, bleed, death, frost, sleep, madness
};
inline Mask classify(const Evidence& value){
    constexpr Kind damageKinds[]{physical,magic,fire,lightning,holy};
    constexpr Kind statusKinds[]{poison,rot,bleed,death,frost,sleep,madness};
    Mask result{};
    for(std::size_t i=0;i<value.damage.size();++i)if(value.damage[i]>0)result|=bit(damageKinds[i]);
    for(std::size_t i=0;i<value.buildup.size();++i)if(value.buildup[i]>0)result|=bit(statusKinds[i]);
    return result;
}
inline const wchar_t* name(Kind kind){
    constexpr const wchar_t* names[]={L"MAGIC",L"FIRE",L"LIGHTNING",L"HOLY",L"ROT",L"POISON",
        L"BLEED",L"SLEEP",L"FROST",L"PHYSICAL",L"MADNESS",L"DEATH BLIGHT"};
    return names[kind];
}
inline std::uint32_t color(Kind kind){
    constexpr std::uint32_t colors[]={0x78A8E9,0xF29A52,0xE8D16C,0xE9D79A,0xB97B63,0x87B66A,
        0xC94D52,0xB79AD2,0x9FD8E7,0xD1C39A,0xE9BB4C,0xA1A892};
    return colors[kind];
}
inline std::vector<Kind> ordered(Mask mask){
    std::vector<Kind> result;
    // Show nonphysical damage first, then statuses, then physical if present.
    constexpr Kind order[]{magic,fire,lightning,holy,rot,poison,bleed,sleep,frost,madness,death,physical};
    for(const auto kind:order)if(mask&bit(kind))result.push_back(kind);
    return result;
}
inline std::wstring label(Mask mask){
    std::wstring result;
    for(auto kind:ordered(mask)){
        if(!result.empty())result+=L" + ";
        result+=name(kind);
    }
    return result;
}
inline std::uint32_t accent(Mask mask){
    for(auto kind:ordered(mask))if(kind!=physical)return color(kind);
    return color(physical);
}
}
