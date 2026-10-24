#pragma once
#include "cycle.hpp"
#include "ammo_slots.hpp"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>
#include <cwctype>

namespace arrow_switch::wheel {
constexpr std::size_t pageSize=8;
constexpr std::size_t page_size(unsigned layout){return layout==1 || layout==3?4:pageSize;}
constexpr std::size_t page_count(std::size_t items,std::size_t size=pageSize){return size?(items+size-1)/size:0;}
constexpr std::size_t page_items(std::size_t items,std::size_t page,std::size_t size=pageSize){
    const auto start=page*size;
    return start<items?std::min(size,items-start):0;
}
constexpr std::size_t turn_page(std::size_t page,std::size_t pages,int scrollSteps){
    if(!pages)return 0;
    const auto amount=static_cast<std::size_t>(scrollSteps<0?-static_cast<std::int64_t>(scrollSteps):scrollSteps)%pages;
    return scrollSteps>=0?(page+amount)%pages:(page+pages-amount)%pages;
}
inline std::vector<Arrow> eligible(const std::vector<Arrow>& arrows,std::uint32_t current,std::uint32_t reserved,
                                   AmmoSlot slot,bool sameFamily=false) {
    std::vector<Arrow> result;
    const auto at=std::find_if(arrows.begin(),arrows.end(),[=](const Arrow& arrow){return arrow.id==current;});
    const bool empty=current==empty_ammo_id;
    if(!empty && (at==arrows.end() || !slot_accepts(slot,at->type))) return result;
    for(const auto& arrow:arrows)
        if(arrow.count && slot_accepts(slot,arrow.type) && arrow.id!=reserved &&
           (!sameFamily || empty || arrow.type==at->type)) result.push_back(arrow);
    return result;
}

// Index zero is at twelve o'clock, increasing clockwise. The center is a cancel zone.
inline int selection(float dx,float dy,std::size_t count,float deadZone=78.f) {
    if(!count || dx*dx+dy*dy<deadZone*deadZone) return -1;
    constexpr double pi=3.14159265358979323846;
    double angle=std::atan2(static_cast<double>(dx),-static_cast<double>(dy));
    if(angle<0) angle+=2*pi;
    return static_cast<int>(std::floor((angle+pi/count)/(2*pi/count)))%static_cast<int>(count);
}
// Conservative visual hint from the active localized name. Unrecognized names stay neutral.
struct Affinity { wchar_t badge; std::uint32_t color; const wchar_t* label; };
inline Affinity damage_affinity(std::uint16_t physical,std::uint16_t magic,std::uint16_t fire,
                               std::uint16_t lightning,std::uint16_t holy) {
    const std::uint16_t values[]={physical,magic,fire,lightning,holy};
    std::size_t strongest=0;
    for(std::size_t i=1;i<5;++i)if(values[i]>values[strongest])strongest=i;
    if(!strongest || values[strongest]<120) return {L'\u2191',0xD1C39A,L"ARROW"};
    for(std::size_t i=0;i<5;++i)if(i!=strongest && values[strongest]<values[i]*1.3f)
        return {L'\u2191',0xD1C39A,L"ARROW"};
    constexpr Affinity result[]={{L'\u2191',0xD1C39A,L"ARROW"},{L'\u2727',0x78A8E9,L"MAGIC"},
        {L'\u2668',0xF29A52,L"FIRE"},{L'\u26a1',0xE8D16C,L"LIGHTNING"},
        {L'\u2726',0xE9D79A,L"HOLY"}};
    return result[strongest];
}
inline Affinity affinity(std::wstring name) {
    for(auto& c:name) c=static_cast<wchar_t>(std::towlower(c));
    const auto has=[&](const wchar_t* word){return name.find(word)!=std::wstring::npos;};
    if(has(L"death")||has(L"blight")) return {L'\u2620',0xA1A892,L"DEATH BLIGHT"};
    if(has(L"madness")||has(L"frenzy")||has(L"frenzied")) return {L'\u273a',0xE9BB4C,L"MADNESS"};
    if(has(L"scarlet rot")||has(L"rotbone")||has(L"rotted")) return {L'\u2623',0xB97B63,L"ROT"};
    if(has(L"poison")) return {L'\u2620',0x87B66A,L"POISON"};
    if(has(L"blood")||has(L"bleed")) return {L'\u2666',0xC94D52,L"BLEED"};
    if(has(L"sleep")) return {L'\u263e',0xB79AD2,L"SLEEP"};
    if(has(L"frost")||has(L"cold")||has(L"ice")) return {L'\u2744',0x9FD8E7,L"FROST"};
    if(has(L"fire")||has(L"flame")) return {L'\u2668',0xF29A52,L"FIRE"};
    if(has(L"lightning")||has(L"thunder")) return {L'\u26a1',0xE8D16C,L"LIGHTNING"};
    if(has(L"holy")||has(L"haligbone")) return {L'\u2726',0xE9D79A,L"HOLY"};
    if(has(L"magic")||has(L"glintstone")||has(L"spiritflame")) return {L'\u2727',0x78A8E9,L"MAGIC"};
    return {L'\u2191',0xD1C39A,L"ARROW"};
}
}
