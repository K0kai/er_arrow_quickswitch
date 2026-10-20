#pragma once
#include <algorithm>
#include <cwctype>
#include <string>
#include <string_view>

namespace arrow_switch::input {
enum Modifier : unsigned { ctrl=1,shift=2,alt=4,win=8 };
struct Binding {
    int key{};
    unsigned modifiers{};
    bool operator==(const Binding&) const = default;
};
inline unsigned modifier_family(int key) {
    if (key==0x11 || key==0xA2 || key==0xA3) return ctrl;
    if (key==0x10 || key==0xA0 || key==0xA1) return shift;
    if (key==0x12 || key==0xA4 || key==0xA5) return alt;
    if (key==0x5B || key==0x5C) return win;
    return 0;
}
inline std::wstring normalize(std::wstring_view value) {
    while (!value.empty() && std::iswspace(value.front())) value.remove_prefix(1);
    while (!value.empty() && std::iswspace(value.back())) value.remove_suffix(1);
    std::wstring result(value);
    std::transform(result.begin(),result.end(),result.begin(),[](wchar_t ch){return static_cast<wchar_t>(std::towupper(ch));});
    return result;
}
inline unsigned modifier(std::wstring_view name) {
    if (name==L"CTRL" || name==L"CONTROL") return ctrl;
    if (name==L"SHIFT") return shift;
    if (name==L"ALT") return alt;
    if (name==L"WIN" || name==L"WINDOWS") return win;
    return 0;
}
inline int number(std::wstring_view value,unsigned base=10) {
    if (value.empty()) return -1;
    unsigned result=0;
    for (auto ch:value) {
        unsigned digit=ch>=L'0' && ch<=L'9' ? static_cast<unsigned>(ch-L'0') :
            ch>=L'A' && ch<=L'F' ? static_cast<unsigned>(ch-L'A'+10) : 99;
        if (digit>=base || result>254/base) return -1;
        result=result*base+digit;
        if (result>254) return -1;
    }
    return static_cast<int>(result);
}
inline int key(std::wstring_view name) {
    if (name.size()==1 && name[0]>=L'A' && name[0]<=L'Z') return name[0];
    if (name.starts_with(L"VK_")) name.remove_prefix(3);
    if (name.starts_with(L"0X")) return number(name.substr(2),16);
    if (!name.empty() && name.front()>=L'0' && name.front()<=L'9') return number(name);
    if (name.starts_with(L"KEY_") || name.starts_with(L"KEY")) {
        name.remove_prefix(name.starts_with(L"KEY_")?4:3);
        if (name.size()==1 && name[0]>=L'0' && name[0]<=L'9') return name[0];
        if (name.size()==1 && name[0]>=L'A' && name[0]<=L'Z') return name[0];
    }
    if (name.starts_with(L"DIGIT") && name.size()==6 && name[5]>=L'0' && name[5]<=L'9') return name[5];
    if (name.starts_with(L"F")) { auto n=number(name.substr(1)); if (n>=1 && n<=24) return 0x6F+n; }
    if (name.starts_with(L"MOUSE")) {
        auto n=number(name.substr(5)); constexpr int values[]={0,1,2,4,5,6};
        if (n>=1 && n<=5) return values[n];
    }
    if (name.starts_with(L"NUMPAD") && name.size()==7 && name[6]>=L'0' && name[6]<=L'9') return 0x60+name[6]-L'0';
    struct Alias { std::wstring_view name; int value; };
    constexpr Alias aliases[]={
        {L"SPACE",0x20},{L"TAB",9},{L"ENTER",13},{L"RETURN",13},{L"ESC",27},{L"ESCAPE",27},
        {L"BACKSPACE",8},{L"DELETE",0x2E},{L"INSERT",0x2D},{L"HOME",0x24},{L"END",0x23},
        {L"PAGEUP",0x21},{L"PAGEDOWN",0x22},{L"UP",0x26},{L"DOWN",0x28},{L"LEFT",0x25},{L"RIGHT",0x27},
        {L"CAPSLOCK",0x14},{L"PLUS",0xBB},{L"MINUS",0xBD},{L"COMMA",0xBC},{L"PERIOD",0xBE},
        {L"NUMPADADD",0x6B},{L"NUMPADSUBTRACT",0x6D},{L"NUMPADMULTIPLY",0x6A},{L"NUMPADDIVIDE",0x6F},
        {L"NUMPADDECIMAL",0x6E},{L"CTRL",0x11},{L"CONTROL",0x11},{L"SHIFT",0x10},{L"ALT",0x12},
        {L"WIN",0x5B},{L"WINDOWS",0x5B}
    };
    for (const auto& alias:aliases) if (name==alias.name) return alias.value;
    return -1;
}
inline bool parse(std::wstring_view text,Binding& result) {
    Binding parsed;
    while (true) {
        const auto plus=text.find(L'+');
        const auto token=normalize(text.substr(0,plus));
        if (token.empty()) return false;
        if (plus==std::wstring_view::npos) {
            parsed.key=key(token);
            if (parsed.key<1 || parsed.key>254 || (modifier_family(parsed.key)&parsed.modifiers)) return false;
            result=parsed; return true;
        }
        const auto bit=modifier(token);
        if (!bit || (parsed.modifiers&bit)) return false;
        parsed.modifiers|=bit;
        text.remove_prefix(plus+1);
    }
}
inline bool parse_optional(std::wstring_view text,Binding& result) {
    const auto token=normalize(text);
    if(token.empty() || token==L"NONE" || token==L"DISABLED" || token==L"0"){
        result={};return true;
    }
    return parse(text,result);
}
inline bool matches(const Binding& binding,unsigned heldModifiers) {
    // Extra modifiers must not accidentally trigger the unmodified binding.
    // A standalone modifier key is still supported for legacy numeric configs.
    return binding.key>0 && (heldModifiers & ~modifier_family(binding.key))==binding.modifiers;
}
struct Edge {
    bool held=true;
    bool sample(bool primaryDown,bool modifiersMatch,bool allowed=true) {
        const bool pressed=primaryDown && !held;
        held=primaryDown;
        return pressed && modifiersMatch && allowed;
    }
};
}
