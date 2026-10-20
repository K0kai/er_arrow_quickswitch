#pragma once
#include "bindings.hpp"
#include "controller.hpp"
#include <array>
#include <filesystem>

namespace arrow_switch::wheel_bindings {
inline constexpr const wchar_t* keyboardKeys[]{L"Arrow1WheelKey",L"Arrow2WheelKey",L"Bolt1WheelKey",L"Bolt2WheelKey"};
inline constexpr const wchar_t* controllerKeys[]{L"Arrow1WheelButton",L"Arrow2WheelButton",L"Bolt1WheelButton",L"Bolt2WheelButton",L"NextPageButton",L"PreviousPageButton"};
struct Values {
    std::array<std::wstring,4> keyboard{L"Key1",L"Key2",L"Key3",L"Key4"};
    std::array<std::wstring,6> pad{L"None",L"None",L"None",L"None",L"RB",L"LB"};
    std::wstring shortcut=L"F10",diagnostic=L"F8";
};
inline Values load(const std::filesystem::path& file){
    Values value;
    const auto read=[&](const wchar_t* section,const wchar_t* key,std::wstring& target){
        wchar_t buffer[256]{};GetPrivateProfileStringW(section,key,target.c_str(),buffer,256,file.c_str());target=buffer;
    };
    for(unsigned i=0;i<4;++i)read(L"Controls",keyboardKeys[i],value.keyboard[i]);
    for(unsigned i=0;i<6;++i)read(L"Controller",controllerKeys[i],value.pad[i]);
    read(L"Controls",L"SettingsKey",value.shortcut);read(L"Controls",L"DiagnosticKey",value.diagnostic);
    return value;
}
inline const char* validate(const Values& value){
    std::array<input::Binding,6> keys{};
    if(!input::parse_optional(value.shortcut,keys[4]) || !input::parse(value.diagnostic,keys[5]))return "Invalid settings or diagnostic shortcut.";
    for(unsigned i=0;i<4;++i)if(!input::parse(value.keyboard[i],keys[i]))return "Enter a valid keyboard binding for every wheel (for example Key1 or Ctrl+E).";
    for(unsigned i=0;i<6;++i)for(unsigned j=i+1;j<6;++j)
        if(keys[i].key && keys[i]==keys[j])return "Keyboard, settings and diagnostic shortcuts must be different.";
    std::array<controller::Binding,6> pad{};
    for(unsigned i=0;i<6;++i)if(!controller::parse(value.pad[i],pad[i]))return "Invalid controller binding. Use button names joined with +, or None.";
    for(unsigned i=0;i<4;++i){
        for(unsigned j=i+1;j<4;++j)if(controller::ambiguous(pad[i],pad[j]))return "Controller wheel chords must not overlap or contain one another.";
        for(unsigned j=4;j<6;++j)if(controller::down(pad[j],pad[i].buttons))return "A wheel chord must not also hold a paging button. Change the wheel or paging binding.";
    }
    if(controller::ambiguous(pad[4],pad[5]))return "Controller paging bindings must not overlap.";
    return nullptr;
}
}
