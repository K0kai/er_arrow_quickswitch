#include "../src/wheel_bindings.hpp"
#include <cassert>
#include <cstdio>

int main() {
    using namespace arrow_switch::input;
    const auto binding=[](std::wstring_view text,int expected,unsigned modifiers=0) {
        Binding result; assert(parse(text,result));
        assert(result.key==expected && result.modifiers==modifiers);
        return result;
    };
    binding(L"117",0x75); binding(L"F6",0x75); binding(L"119",0x77);
    binding(L"Ctrl+E",69,ctrl); binding(L" control + e ",69,ctrl);
    binding(L"Shift+Mouse4",5,shift); binding(L"Alt+Q",81,alt);
    binding(L"Ctrl+Shift+E",69,ctrl|shift);
    binding(L"Win+Alt+Mouse5",6,win|alt);
    binding(L"Ctrl+69",69,ctrl); binding(L"Ctrl+0x45",69,ctrl);
    binding(L"VK_0x45",69); binding(L"VK_69",69);
    binding(L"Key5",53); binding(L"Digit5",53); binding(L"5",5);
    binding(L"KEY_E",69); binding(L"Ctrl+Enter",13,ctrl);
    binding(L"F24",0x87); binding(L"Alt+Numpad4",0x64,alt);
    binding(L"Ctrl+Plus",0xBB,ctrl); binding(L"Shift+Space",32,shift);
    binding(L"Mouse1",1); binding(L"Mouse2",2); binding(L"Mouse3",4);
    binding(L"17",0x11); binding(L"Ctrl",0x11);
    const wchar_t* invalid[]={L"",L" ",L"0",L"255",L"-1",L"9999999999999999999999999999",
        L"Ctrl+",L"+E",L"Ctrl++E",L"E+Ctrl",L"Ctr+E",L"Ctrl+Control+E",L"Ctrl+Ctrl",
        L"Mouse6",L"F25",L"Ctrl+Unrecognized",L"0xFF",L"Ctrl+0x",L"E+Q"};
    for (const auto* text:invalid) {
        Binding result{117,0}; assert(!parse(text,result));
        assert((result==Binding{117,0})); // rejected input cannot partially change a binding
    }
    const auto plain=binding(L"E",69),combo=binding(L"Ctrl+E",69,ctrl);
    assert(matches(plain,0)); assert(!matches(plain,ctrl));
    assert(matches(combo,ctrl)); assert(!matches(combo,0)); assert(!matches(combo,ctrl|shift));
    assert(matches(binding(L"17",17),ctrl)); assert(!matches(binding(L"17",17),ctrl|alt));
    assert(binding(L"Ctrl+Shift+E",69,ctrl|shift)==binding(L"shift+control+69",69,ctrl|shift));
    assert(plain!=combo);
    for(const auto* text:{L"",L" ",L"None",L" none ",L"Disabled",L"0"}){
        Binding optional{0x79,0};assert(parse_optional(text,optional));
        assert(optional.key==0 && optional.modifiers==0 && !matches(optional,0));
    }
    Binding optional;
    assert(parse_optional(L"Ctrl+F10",optional) && optional.key==0x79 && optional.modifiers==ctrl);
    assert(!parse_optional(L"Ctrl+None",optional) && optional.key==0x79);

    {
        using namespace arrow_switch;
        wheel_bindings::Values values;
        assert(!wheel_bindings::validate(values));
        values.keyboard[0]=L"F10";assert(wheel_bindings::validate(values));
        values.keyboard[0]=L"Ctrl+E";assert(!wheel_bindings::validate(values));
        values.keyboard[1]=L"Ctrl+E";assert(wheel_bindings::validate(values));
        values.keyboard[1]=L"Key2";
        values.pad[0]=L"Y+DPadUp";assert(!wheel_bindings::validate(values));
        values.pad[1]=L"Y";assert(wheel_bindings::validate(values));
        values.pad[1]=L"None";values.pad[0]=L"LB+Y";assert(wheel_bindings::validate(values));
        values.pad[5]=L"DPadLeft";assert(!wheel_bindings::validate(values));
        values.pad[0]=L"bad button";assert(wheel_bindings::validate(values));
        values.pad[0]=L"None";values.shortcut=L"None";assert(!wheel_bindings::validate(values));
        values.pad[6]=L"RB";assert(wheel_bindings::validate(values));
        values.pad[6]=L"None";assert(!wheel_bindings::validate(values));
        values.pad[6]=L"Back+Start";values.pad[0]=L"Back";assert(wheel_bindings::validate(values));
    }
    Edge edge;
    assert(!edge.sample(true,true)); // do not trigger a key held during initialization
    assert(!edge.sample(false,true));
    assert(!edge.sample(true,false)); // E before Ctrl does not trigger Ctrl+E
    assert(!edge.sample(true,true)); // adding Ctrl while E stays held does not trigger
    assert(!edge.sample(false,true));
    assert(edge.sample(true,true)); // hold Ctrl, then press E
    assert(!edge.sample(true,true)); // no repeat while held
    assert(!edge.sample(true,false));
    assert(!edge.sample(true,true)); // releasing/repressing Ctrl is not another E press
    assert(!edge.sample(false,true));
    assert(!edge.sample(true,true,false)); // background keypress consumed
    assert(!edge.sample(true,true,true)); // focus return must not leak a held press
    assert(!edge.sample(false,true)); assert(edge.sample(true,true));
    std::puts("Bindings: legacy VKs, names, modifier chords, invalid input, exact modifiers and press-edge/focus tests passed.");
}
