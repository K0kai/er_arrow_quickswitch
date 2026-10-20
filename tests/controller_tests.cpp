#include "../src/controller.hpp"
#include "../src/wheel.hpp"
#include <cassert>
#include <cstdio>

int main() {
    using namespace arrow_switch;
    controller::Binding binding;
    assert(controller::parse(L" None ",binding) && !binding.buttons);
    assert(controller::parse(L"LB+DPadUp",binding));
    assert(binding.buttons==(XINPUT_GAMEPAD_LEFT_SHOULDER|XINPUT_GAMEPAD_DPAD_UP));
    assert(controller::parse(L"PadLT + PadDPadLeft",binding));
    assert(binding.buttons==(controller::leftTrigger|XINPUT_GAMEPAD_DPAD_LEFT));
    assert(controller::parse(L"L3+Menu",binding));
    assert(binding.buttons==(XINPUT_GAMEPAD_LEFT_THUMB|XINPUT_GAMEPAD_START));
    const auto before=binding;
    for(const auto* invalid:{L"",L"LB+",L"LB+LB",L"PadGuide",L"LB+Ctrl",L"A++B"}) {
        assert(!controller::parse(invalid,binding));
        assert(binding==before);
    }
    controller::Binding up,left,shoulder;
    assert(controller::parse(L"LB+DPadUp",up));
    assert(controller::parse(L"LB+DPadLeft",left));
    assert(controller::parse(L"LB",shoulder));
    assert(!controller::ambiguous(up,left));
    assert(controller::ambiguous(up,shoulder));
    assert(!controller::down(up,XINPUT_GAMEPAD_LEFT_SHOULDER));
    assert(controller::down(up,XINPUT_GAMEPAD_LEFT_SHOULDER|XINPUT_GAMEPAD_DPAD_UP));
    XINPUT_GAMEPAD pad{};
    pad.bLeftTrigger=179;
    assert(!(controller::held_buttons(pad,180)&controller::leftTrigger));
    pad.bLeftTrigger=180;
    assert(controller::held_buttons(pad,180)&controller::leftTrigger);
    float x=1,y=1;
    assert(!controller::right_stick(pad,25,x,y) && x==0 && y==0);
    pad.sThumbRY=30000;
    assert(controller::right_stick(pad,25,x,y) && y<0);
    assert(wheel::selection(x,y,8)==0);
    pad.sThumbRY=0;pad.sThumbRX=30000;
    assert(controller::right_stick(pad,25,x,y) && x>0);
    assert(wheel::selection(x,y,8)==2);
    pad.sThumbRX=2000;
    assert(!controller::right_stick(pad,25,x,y));
    std::puts("Controller chords, trigger threshold, deadzone and right-stick wheel selection passed.");
}
