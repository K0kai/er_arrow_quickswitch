#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Xinput.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cwctype>
#include <string>
#include <string_view>
#include <utility>

namespace arrow_switch::controller {
constexpr std::uint32_t leftTrigger=1u<<16, rightTrigger=1u<<17;
struct Binding {
    std::uint32_t buttons{}; // zero disables this controller binding
    bool operator==(const Binding&) const = default;
};
inline std::wstring normalize(std::wstring_view value) {
    while (!value.empty() && std::iswspace(value.front())) value.remove_prefix(1);
    while (!value.empty() && std::iswspace(value.back())) value.remove_suffix(1);
    std::wstring result(value);
    std::transform(result.begin(),result.end(),result.begin(),[](wchar_t ch){return static_cast<wchar_t>(std::towupper(ch));});
    return result;
}
inline std::uint32_t button(std::wstring_view name) {
    if (name.starts_with(L"PAD")) name.remove_prefix(3);
    struct Alias { std::wstring_view name; std::uint32_t mask; };
    constexpr Alias aliases[]={
        {L"A",XINPUT_GAMEPAD_A},{L"B",XINPUT_GAMEPAD_B},{L"X",XINPUT_GAMEPAD_X},{L"Y",XINPUT_GAMEPAD_Y},
        {L"LB",XINPUT_GAMEPAD_LEFT_SHOULDER},{L"RB",XINPUT_GAMEPAD_RIGHT_SHOULDER},
        {L"LT",leftTrigger},{L"RT",rightTrigger},
        {L"LS",XINPUT_GAMEPAD_LEFT_THUMB},{L"L3",XINPUT_GAMEPAD_LEFT_THUMB},
        {L"RS",XINPUT_GAMEPAD_RIGHT_THUMB},{L"R3",XINPUT_GAMEPAD_RIGHT_THUMB},
        {L"BACK",XINPUT_GAMEPAD_BACK},{L"VIEW",XINPUT_GAMEPAD_BACK},
        {L"START",XINPUT_GAMEPAD_START},{L"MENU",XINPUT_GAMEPAD_START},
        {L"DPADUP",XINPUT_GAMEPAD_DPAD_UP},{L"DPADDOWN",XINPUT_GAMEPAD_DPAD_DOWN},
        {L"DPADLEFT",XINPUT_GAMEPAD_DPAD_LEFT},{L"DPADRIGHT",XINPUT_GAMEPAD_DPAD_RIGHT}
    };
    for(const auto& alias:aliases) if(name==alias.name)return alias.mask;
    return 0;
}
inline bool parse(std::wstring_view text,Binding& result) {
    if (normalize(text)==L"NONE") {result={};return true;}
    Binding parsed;
    while(true) {
        const auto plus=text.find(L'+');
        const auto token=normalize(text.substr(0,plus));
        const auto bit=button(token);
        if(!bit || (parsed.buttons&bit))return false;
        parsed.buttons|=bit;
        if(plus==std::wstring_view::npos) {result=parsed;return true;}
        text.remove_prefix(plus+1);
    }
}
inline bool down(Binding binding,std::uint32_t held) {
    return binding.buttons && (held&binding.buttons)==binding.buttons;
}
inline bool ambiguous(Binding a,Binding b) {
    return a.buttons && b.buttons &&
        ((a.buttons&b.buttons)==a.buttons || (a.buttons&b.buttons)==b.buttons);
}
inline std::uint32_t held_buttons(const XINPUT_GAMEPAD& pad,unsigned triggerThreshold) {
    return static_cast<std::uint32_t>(pad.wButtons) |
        (pad.bLeftTrigger>=triggerThreshold?leftTrigger:0u) |
        (pad.bRightTrigger>=triggerThreshold?rightTrigger:0u);
}
// XInput's Y axis is positive up; screen coordinates are positive down.
// A returned false is the wheel's cancel zone.
inline bool right_stick(const XINPUT_GAMEPAD& pad,unsigned deadzonePercent,float& x,float& y) {
    const auto rawX=static_cast<float>(pad.sThumbRX)/32767.f;
    const auto rawY=static_cast<float>(pad.sThumbRY)/32767.f;
    const float magnitude=std::hypot(rawX,rawY);
    if(magnitude<static_cast<float>(deadzonePercent)/100.f) {x=0;y=0;return false;}
    x=std::clamp(rawX*200.f,-200.f,200.f);
    y=std::clamp(-rawY*200.f,-200.f,200.f);
    return true;
}
}
