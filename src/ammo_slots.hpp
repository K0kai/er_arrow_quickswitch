#pragma once
#include <cstddef>
#include <cstdint>

namespace arrow_switch {
constexpr std::uint32_t empty_ammo_id=0xFFFFFFFFu;
enum class AmmoSlot : std::uint8_t { arrow1,arrow2,bolt1,bolt2 };
constexpr std::size_t slot_index(AmmoSlot slot){return static_cast<std::size_t>(slot);}
// Confirmed from live 1.17.1 equipment data: arrow1, bolt1, arrow2, bolt2.
constexpr int native_slot(AmmoSlot slot){
    constexpr int values[]{6,8,7,9};
    return values[slot_index(slot)];
}
constexpr bool slot_accepts(AmmoSlot slot,std::uint16_t type){
    return slot_index(slot)<2 ? (type==81||type==83) : (type==85||type==86);
}
constexpr AmmoSlot peer_slot(AmmoSlot slot){return static_cast<AmmoSlot>(slot_index(slot)^1u);}
constexpr std::uintptr_t id_offset(AmmoSlot slot){return 0x3B0+(native_slot(slot)-6)*4;}
constexpr std::uintptr_t handle_offset(AmmoSlot slot){return 0x358+(native_slot(slot)-6)*4;}
constexpr std::uintptr_t index_offset(AmmoSlot slot){return 0x2D0+(native_slot(slot)-6)*4;}
constexpr const wchar_t* slot_label(AmmoSlot slot){
    switch(slot){
        case AmmoSlot::arrow1:return L"ARROW SLOT 1";
        case AmmoSlot::arrow2:return L"ARROW SLOT 2";
        case AmmoSlot::bolt1:return L"BOLT SLOT 1";
        case AmmoSlot::bolt2:return L"BOLT SLOT 2";
    }
    return L"AMMO";
}
}
