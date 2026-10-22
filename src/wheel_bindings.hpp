#pragma once
#include "bindings.hpp"
#include "controller.hpp"
#include <array>
#include <filesystem>

namespace arrow_switch::wheel_bindings {
inline constexpr const wchar_t* keyboardKeys[]{L"Arrow1WheelKey",L"Arrow2WheelKey",L"Bolt1WheelKey",L"Bolt2WheelKey"};
inline constexpr const wchar_t* controllerKeys[]{L"Arrow1WheelButton",L"Arrow2WheelButton",L"Bolt1WheelButton",L"Bolt2WheelButton",L"NextPageButton",L"PreviousPageButton",L"SettingsButton"};
inline constexpr std::size_t wheelCount=std::size(keyboardKeys);
inline constexpr std::size_t pagingCount=2;
inline constexpr std::size_t settingsIndex=wheelCount+pagingCount;
inline constexpr std::size_t controllerCount=std::size(controllerKeys);
inline constexpr std::size_t nextPageIndex=wheelCount;
inline constexpr std::size_t previousPageIndex=wheelCount+1;
inline constexpr std::size_t keyboardSettingsIndex=wheelCount;
inline constexpr std::size_t diagnosticIndex=wheelCount+1;
inline constexpr std::size_t keyboardBindingCount=wheelCount+2;
struct Values {
    std::array<std::wstring,wheelCount> keyboard{L"Key1",L"Key2",L"Key3",L"Key4"};
    std::array<std::wstring,controllerCount> pad{L"None",L"None",L"None",L"None",L"RB",L"LB",L"Back+Start"};
    std::wstring shortcut=L"F10",diagnostic=L"F8";
};
inline Values load(const std::filesystem::path& file){
    Values value;
    const auto read=[&](const wchar_t* section,const wchar_t* key,std::wstring& target){
        wchar_t buffer[256]{};GetPrivateProfileStringW(section,key,target.c_str(),buffer,256,file.c_str());target=buffer;
    };
    for(unsigned i=0;i<wheelCount;++i)read(L"Controls",keyboardKeys[i],value.keyboard[i]);
    for(unsigned i=0;i<controllerCount;++i)read(L"Controller",controllerKeys[i],value.pad[i]);
    read(L"Controls",L"SettingsKey",value.shortcut);read(L"Controls",L"DiagnosticKey",value.diagnostic);
    return value;
}
inline const char* validate(const Values& value){
    std::array<input::Binding,keyboardBindingCount> keys{};
    if(!input::parse_optional(value.shortcut,keys[keyboardSettingsIndex]) || !input::parse(value.diagnostic,keys[diagnosticIndex]))return "Invalid settings or diagnostic shortcut.";
    for(unsigned i=0;i<wheelCount;++i)if(!input::parse(value.keyboard[i],keys[i]))return "Enter a valid keyboard binding for every wheel (for example Key1 or Ctrl+E).";
    for(unsigned i=0;i<keyboardBindingCount;++i)for(unsigned j=i+1;j<keyboardBindingCount;++j)
        if(keys[i].key && keys[i]==keys[j])return "Keyboard, settings and diagnostic shortcuts must be different.";
    std::array<controller::Binding,controllerCount> pad{};
    for(unsigned i=0;i<controllerCount;++i)if(!controller::parse(value.pad[i],pad[i]))return "Invalid controller binding. Use button names joined with +, or None.";
    for(unsigned i=0;i<wheelCount;++i){
        for(unsigned j=i+1;j<wheelCount;++j)if(controller::ambiguous(pad[i],pad[j]))return "Controller wheel chords must not overlap or contain one another.";
        for(unsigned j=wheelCount;j<settingsIndex;++j)if(controller::down(pad[j],pad[i].buttons))return "A wheel chord must not also hold a paging button. Change the wheel or paging binding.";
    }
    if(controller::ambiguous(pad[nextPageIndex],pad[previousPageIndex]))return "Controller paging bindings must not overlap.";
    for(unsigned i=0;i<settingsIndex;++i)if(controller::ambiguous(pad[i],pad[settingsIndex]))
        return "The controller settings shortcut must not overlap a wheel or paging binding.";
    return nullptr;
}
}
