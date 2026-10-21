#pragma once
#include "bindings.hpp"
#include "controller.hpp"
#include "window_input.hpp"
#include <bit>
#include <cwchar>

namespace arrow_switch::binding_capture {
enum class Device {keyboard,controller};
inline unsigned modifiers(const window_input::State& keys){
    return (keys.down(VK_CONTROL)?input::ctrl:0u)|(keys.down(VK_SHIFT)?input::shift:0u)|
        (keys.down(VK_MENU)?input::alt:0u)|((keys.down(VK_LWIN)||keys.down(VK_RWIN))?input::win:0u);
}
inline bool held(const window_input::State& keys){for(int key=1;key<255;++key)if(keys.down(key))return true;return false;}
inline std::wstring key_name(int key){
    if(key>='A' && key<='Z')return std::wstring(1,static_cast<wchar_t>(key));
    if(key>='0' && key<='9')return L"Key"+std::wstring(1,static_cast<wchar_t>(key));
    if(key>=VK_F1 && key<=VK_F24)return L"F"+std::to_wstring(key-VK_F1+1);
    switch(key){
    case VK_LBUTTON:return L"Mouse1";case VK_RBUTTON:return L"Mouse2";case VK_MBUTTON:return L"Mouse3";
    case VK_XBUTTON1:return L"Mouse4";case VK_XBUTTON2:return L"Mouse5";
    case VK_CONTROL:case VK_LCONTROL:case VK_RCONTROL:return L"Ctrl";
    case VK_SHIFT:case VK_LSHIFT:case VK_RSHIFT:return L"Shift";
    case VK_MENU:case VK_LMENU:case VK_RMENU:return L"Alt";
    case VK_LWIN:return L"Win";
    case VK_SPACE:return L"Space";case VK_TAB:return L"Tab";case VK_RETURN:return L"Enter";
    case VK_ESCAPE:return L"Escape";case VK_BACK:return L"Backspace";case VK_DELETE:return L"Delete";
    case VK_INSERT:return L"Insert";case VK_HOME:return L"Home";case VK_END:return L"End";
    case VK_PRIOR:return L"PageUp";case VK_NEXT:return L"PageDown";
    case VK_UP:return L"Up";case VK_DOWN:return L"Down";case VK_LEFT:return L"Left";case VK_RIGHT:return L"Right";
    }
    wchar_t value[16]{};std::swprintf(value,16,L"0x%02X",static_cast<unsigned>(key));return value;
}
inline std::wstring keyboard_name(int key,unsigned mask){
    std::wstring text;mask&=~input::modifier_family(key);
    if(mask&input::ctrl)text+=L"Ctrl+";if(mask&input::shift)text+=L"Shift+";
    if(mask&input::alt)text+=L"Alt+";if(mask&input::win)text+=L"Win+";
    return text+key_name(key);
}
inline std::wstring controller_name(std::uint32_t mask){
    constexpr const wchar_t* names[]{L"LT",L"RT",L"LB",L"RB",L"LS",L"RS",L"Back",L"Start",L"DPadUp",L"DPadDown",L"DPadLeft",L"DPadRight",L"A",L"B",L"X",L"Y"};
    std::wstring text;
    for(const auto* name:names)if(mask&controller::button(controller::normalize(name))){if(!text.empty())text+=L"+";text+=name;}
    return text;
}
struct Capture {
    Device device=Device::keyboard;
    bool active=false,waiting=false,completed=false,cancelled=false;
    window_input::State previous;
    int modifierKey=0;unsigned modifierMask=0;
    std::uint32_t peak=0;
    std::wstring result;
    void begin(Device kind,const window_input::State& keys,std::uint32_t pad){
        *this=Capture{};device=kind;previous=keys;active=true;
        waiting=kind==Device::keyboard?held(keys):pad!=0;
    }
    void cancel(){active=false;cancelled=true;}
    void finish(std::wstring text){result=std::move(text);active=false;completed=true;}
    void keyboard(const window_input::State& keys){
        if(!active)return;
        if(!keys.focused){cancel();return;}
        if(keys.down(VK_ESCAPE) && !previous.down(VK_ESCAPE)){cancel();return;}
        if(device==Device::keyboard){
            if(waiting){if(!held(keys))waiting=false;previous=keys;return;}
            for(int key=1;key<255;++key){
                if(!keys.down(key) || previous.down(key))continue;
                if(input::modifier_family(key)){modifierKey=key;modifierMask=modifiers(keys);}
                else{finish(keyboard_name(key,modifiers(keys)));break;}
            }
            if(active && modifierKey && !keys.down(modifierKey))finish(keyboard_name(modifierKey,modifierMask));
        }
        previous=keys;
    }
    void controller_buttons(std::uint32_t buttons,bool connected){
        if(!active || device!=Device::controller)return;
        if(!connected){if(peak)cancel();return;}
        constexpr std::uint32_t supported=controller::leftTrigger|controller::rightTrigger|
            XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_B|XINPUT_GAMEPAD_X|XINPUT_GAMEPAD_Y|
            XINPUT_GAMEPAD_LEFT_SHOULDER|XINPUT_GAMEPAD_RIGHT_SHOULDER|
            XINPUT_GAMEPAD_LEFT_THUMB|XINPUT_GAMEPAD_RIGHT_THUMB|XINPUT_GAMEPAD_BACK|XINPUT_GAMEPAD_START|
            XINPUT_GAMEPAD_DPAD_UP|XINPUT_GAMEPAD_DPAD_DOWN|XINPUT_GAMEPAD_DPAD_LEFT|XINPUT_GAMEPAD_DPAD_RIGHT;
        buttons&=supported;
        if(waiting){if(!buttons)waiting=false;return;}
        // Save the largest simultaneous chord, never a union of sequential keys.
        if(std::popcount(buttons)>std::popcount(peak))peak=buttons;
        if(!buttons && peak)finish(controller_name(peak));
    }
};
}
