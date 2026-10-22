#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>
#include "cycle.hpp"
#include "ammo_slots.hpp"

namespace arrow_switch::game {
inline std::uintptr_t base() { return reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)); }
inline bool read_bytes(std::uintptr_t address, void* value, std::size_t size) {
    SIZE_T got{};
    return address>=0x10000 && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),value,size,&got) && got==size;
}
template<class T> bool read(std::uintptr_t address,T& value) { return read_bytes(address,&value,sizeof(value)); }
inline std::uintptr_t pointer(std::uintptr_t address) { std::uintptr_t value{}; read(address,value); return value; }
inline bool equal(std::uintptr_t address,const std::vector<std::uint8_t>& bytes) {
    std::vector<std::uint8_t> actual(bytes.size());
    return read_bytes(address,actual.data(),actual.size()) && actual==bytes;
}
inline std::uintptr_t find(const std::vector<int>& pattern) {
    auto image=base();
    auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    auto nt=reinterpret_cast<const IMAGE_NT_HEADERS*>(image+dos->e_lfanew);
    auto sections=IMAGE_FIRST_SECTION(nt);
    std::uintptr_t result{};
    for (unsigned s=0;s<nt->FileHeader.NumberOfSections;++s) {
        if (!(sections[s].Characteristics&IMAGE_SCN_MEM_EXECUTE)) continue;
        const auto size=sections[s].Misc.VirtualSize;
        std::vector<std::uint8_t> bytes(size);
        if (size<pattern.size() || !read_bytes(image+sections[s].VirtualAddress,bytes.data(),bytes.size())) continue;
        for (std::size_t i=0;i<=size-pattern.size();++i) {
            if (pattern[0]>=0 && bytes[i]!=pattern[0]) continue;
            std::size_t j=1;
            for (;j<pattern.size();++j) if (pattern[j]>=0 && bytes[i+j]!=pattern[j]) break;
            if (j!=pattern.size()) continue;
            if (result) return 0;
            result=image+sections[s].VirtualAddress+i;
        }
    }
    return result;
}
inline std::uintptr_t rip(std::uintptr_t instruction) {
    std::int32_t offset{};
    return instruction && read(instruction+3,offset) ? instruction+7+offset : 0;
}

struct Addresses {
    std::uintptr_t gdm{},params{},world{},equip{},hud{};
    bool resolve() {
        gdm=rip(find({0x48,0x8B,0x05,-1,-1,-1,-1,0x48,0x85,0xC0,0x74,0x05,0x48,0x8B,0x40,0x58,0xC3,0xC3}));
        params=rip(find({0x48,0x8B,0x0D,-1,-1,-1,-1,0x48,0x85,0xC9,0x0F,0x84,-1,-1,-1,-1,0x45,0x33,0xC0,0xBA,0x8E,0,0,0}));
        world=rip(find({0x48,0x8B,0x05,-1,-1,-1,-1,0x48,0x85,0xC0,0x74,0x0F,0x48,0x39,0x88}));
        const auto actual=find({0x41,0x8B,0xF1,0x49,0x8B,0xD8,0x48,0x63,0xEA,0x48,0x8B,0xF9});
        equip=actual ? actual-0x17 : 0;
        hud=base()+0x776530;
        return gdm && params && world && equip && equal(equip,{0x40,0x53,0x55,0x56,0x57,0x41,0x56,0x48,0x81,0xEC,0x90,0,0,0}) &&
            equal(hud,{0x48,0x89,0x5C,0x24,0x10,0x48,0x89,0x6C,0x24,0x18,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x57}) &&
            equal(base()+0x76B584,{0x48,0x8D,0x8F,0x60,0x4E,0,0,0x48,0x8D,0x97,0x70,0x4E,0,0}) &&
            equal(base()+0x76B573,{0x49,0x89,0x1E,0xC6,0x41,0x78,0x03});
    }
};

inline std::uintptr_t named_param(std::uintptr_t paramStatic,const char* expected) {
    auto repo=pointer(paramStatic);
    if (!repo) return 0;
    const auto length=std::strlen(expected)+1;
    if(length>64)return 0;
    for (std::uintptr_t off=0;off<0x1000;off+=8) {
        auto holder=pointer(repo+off);
        auto container=holder ? pointer(holder+0x80) : 0;
        auto param=container ? pointer(container+0x80) : 0;
        std::uint32_t offset{};
        if (!param || !read(param+0x10,offset) || !plausible_param_name_offset(offset)) continue;
        char name[64]{};
        if (read_bytes(param+offset,name,length) && std::memcmp(name,expected,length)==0) return param;
    }
    return 0;
}
inline std::uintptr_t weapon_param(std::uintptr_t paramStatic) {
    return named_param(paramStatic,"EQUIP_PARAM_WEAPON_ST");
}
inline std::uintptr_t effect_param(std::uintptr_t paramStatic) {
    return named_param(paramStatic,"SP_EFFECT_PARAM_ST");
}
inline std::uintptr_t row(std::uintptr_t param,std::uint32_t id) {
    std::uint16_t count{};
    if (!param || !read(param+0xA,count) || !count) return 0;
    unsigned first=0,last=count;
    while (first<last) {
        auto mid=first+(last-first)/2;
        auto desc=param+0x40+mid*0x18;
        std::uint32_t found{};
        if (!read(desc,found)) return 0;
        if (found<id) first=mid+1;
        else if (found>id) last=mid;
        else {
            std::uint64_t offset{};
            return read(desc+8,offset) && offset && offset<0x10000000 ? param+offset : 0;
        }
    }
    return 0;
}
inline bool type(std::uintptr_t param,std::uint32_t id,std::uint16_t& value) {
    auto data=row(param,id);
    return data && read(data+0x1A6,value);
}
struct Entry { std::uint32_t handle,id,count,sort,extra,unknown; };
static_assert(sizeof(Entry)==0x18);

struct Snapshot {
    std::uintptr_t manager{},player{},inventory{},list{},param{};
    std::uint32_t tail{},first{},second{},handle{},index{};
    std::array<std::uint32_t,4> slotIds{},slotHandles{},slotIndices{};
    std::array<std::uint32_t,22> indices{};
    std::vector<Arrow> arrows;
};

inline bool snapshot(const Addresses& a,Snapshot& s) {
    s.manager=pointer(a.gdm); s.player=s.manager ? pointer(s.manager+8) : 0;
    s.inventory=s.player ? pointer(s.player+0x5D0) : 0;
    s.list=s.inventory ? pointer(s.inventory+0x10) : 0;
    s.param=weapon_param(a.params);
    std::uint32_t cap{},used{},inlineTail{};
    if (!s.player || !s.list || !s.param || !read(s.inventory+0xC,cap) || !cap || cap>10000 ||
        !read(s.inventory+0x18,used) || used>cap || !read(s.inventory+0x1C,s.tail) ||
        !read(s.player+0x424,inlineTail) || s.tail!=inlineTail ||
        !read(s.player+0x2B8,s.indices)) return false;
    for(std::size_t i=0;i<4;++i){
        const auto slot=static_cast<AmmoSlot>(i);
        if(!read(s.player+id_offset(slot),s.slotIds[i]) ||
           !read(s.player+handle_offset(slot),s.slotHandles[i]) ||
           !read(s.player+index_offset(slot),s.slotIndices[i])) return false;
    }
    s.first=s.slotIds[0];s.second=s.slotIds[1];s.handle=s.slotHandles[0];s.index=s.slotIndices[0];
    std::vector<Entry> entries(cap);
    if (!read_bytes(s.list,entries.data(),entries.size()*sizeof(Entry))) return false;
    s.arrows.clear();
    for (std::uint32_t i=0;i<cap;++i) {
        const auto& e=entries[i];
        if (e.handle==0xFFFFFFFF || e.id>=0x10000000) continue;
        std::uint16_t weaponType{};
        if (type(s.param,e.id,weaponType) && is_ammo_type(weaponType))
            s.arrows.push_back({e.id,e.handle,e.count,i,weaponType});
    }
    std::sort(s.arrows.begin(),s.arrows.end(),[](const auto& l,const auto& r){return l.id<r.id;});
    return pointer(a.gdm)==s.manager && pointer(s.manager+8)==s.player &&
        pointer(s.player+0x5D0)==s.inventory && pointer(s.inventory+0x10)==s.list;
}

inline const Arrow* equipped_entry(const Snapshot& s,AmmoSlot slot){
    const auto i=slot_index(slot);
    if(s.tail>static_cast<std::uint32_t>(INT32_MAX) || s.slotIndices[i]<s.tail)return nullptr;
    const auto inventoryIndex=s.slotIndices[i]-s.tail;
    const auto at=std::find_if(s.arrows.begin(),s.arrows.end(),[&](const auto& e){
        return e.id==s.slotIds[i] && e.index==inventoryIndex && slot_accepts(slot,e.type) &&
            (i==1 || i==3 || e.handle==s.slotHandles[i]);
    });
    return at==s.arrows.end()?nullptr:&*at;
}
// Only accept the explicit unequipped sentinel in all three equipment fields.
// Unknown IDs or partially cleared equipment remain inconsistent and blocked.
inline bool empty_slot(const Snapshot& s,AmmoSlot slot){
    const auto i=slot_index(slot);
    return s.slotIds[i]==empty_ammo_id && s.slotHandles[i]==0xFFFFFFFFu &&
        s.slotIndices[i]==0xFFFFFFFFu;
}
inline bool current_matches_slot(const Snapshot& s,AmmoSlot slot){
    return empty_slot(s,slot) || equipped_entry(s,slot)!=nullptr;
}

inline bool revalidate_slot(const Addresses& a,const Snapshot& s,const Arrow& candidate,AmmoSlot slot){
    if(!slot_accepts(slot,candidate.type))return false;
    const auto* current=equipped_entry(s,slot);
    if(!current && !empty_slot(s,slot))return false;
    if(pointer(a.gdm)!=s.manager || pointer(s.manager+8)!=s.player ||
       pointer(s.player+0x5D0)!=s.inventory || pointer(s.inventory+0x10)!=s.list) return false;
    const auto i=slot_index(slot),peer=slot_index(peer_slot(slot));
    std::uint32_t id{},handle{},index{},peerId{},tail{};
    Entry entry{},equipped{};
    return read(s.player+id_offset(slot),id) && id==s.slotIds[i] &&
        read(s.player+handle_offset(slot),handle) && handle==s.slotHandles[i] &&
        read(s.player+index_offset(slot),index) && index==s.slotIndices[i] &&
        (empty_slot(s,slot) || (read(s.list+current->index*sizeof(Entry),equipped) &&
        equipped.id==current->id && equipped.handle==current->handle)) &&
        read(s.player+id_offset(peer_slot(slot)),peerId) && peerId==s.slotIds[peer] &&
        read(s.inventory+0x1C,tail) && tail==s.tail &&
        read(s.list+candidate.index*sizeof(Entry),entry) && entry.handle==candidate.handle &&
        entry.id==candidate.id && entry.count>0 && candidate.id!=peerId &&
        candidate.index<=static_cast<std::uint32_t>(INT32_MAX)-tail;
}

inline bool current_matches(const Snapshot& s) {
    auto at=std::find_if(s.arrows.begin(),s.arrows.end(),[&](const auto& e){return e.id==s.first && e.handle==s.handle;});
    return s.tail<=static_cast<std::uint32_t>(INT32_MAX) && at!=s.arrows.end() &&
        at->index<=static_cast<std::uint32_t>(INT32_MAX)-s.tail && at->index+s.tail==s.index;
}
// Revalidate the candidate and current equipment immediately before the native call.
inline bool revalidate(const Addresses& a,const Snapshot& s,const Arrow& candidate) {
    if (pointer(a.gdm)!=s.manager || pointer(s.manager+8)!=s.player ||
        pointer(s.player+0x5D0)!=s.inventory || pointer(s.inventory+0x10)!=s.list) return false;
    std::uint32_t first{},second{},tail{},handle{},index{};
    Entry entry{};
    return read(s.player+0x3B0,first) && first==s.first && read(s.player+0x3B8,second) && second==s.second &&
        read(s.player+0x358,handle) && handle==s.handle && read(s.player+0x2D0,index) && index==s.index &&
        read(s.inventory+0x1C,tail) && tail==s.tail &&
        read(s.list+candidate.index*sizeof(Entry),entry) && entry.handle==candidate.handle &&
        entry.id==candidate.id && entry.count>0 && candidate.id!=second &&
        candidate.index<=static_cast<std::uint32_t>(INT32_MAX)-tail;
}

inline bool normal_gameplay(const Addresses& a,void* hudContext) {
    const auto context=reinterpret_cast<std::uintptr_t>(hudContext);
    if (context<0x10000+0x4E60) return false;
    const auto fe=context-0x4E60;
    std::uintptr_t vtable{};
    std::uint8_t state{};
    // CSFeManImp embeds this HUD object at +0x4E60, confirmed in its constructor.
    if (!read(fe,vtable) || vtable!=base()+0x2AA0A08 || !read(fe+0x78,state) || state!=3) return false;
    auto world=pointer(a.world);
    auto chr=world ? pointer(world+0x1E508) : 0;
    auto modules=chr ? pointer(chr+0x190) : 0;
    auto stats=modules ? pointer(modules) : 0;
    std::int32_t hp{};
    return stats && read(stats+0x138,hp) && hp>0;
}
}
