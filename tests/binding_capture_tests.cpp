#include "../src/binding_capture.hpp"
#include <cassert>
#include <cstdio>
using namespace arrow_switch;
int main(){
    using binding_capture::Capture;using binding_capture::Device;
    window_input::State keys;keys.focus(true);Capture capture;
    keys.button(VK_LBUTTON,true);capture.begin(Device::keyboard,keys,0);
    capture.keyboard(keys);assert(capture.active && capture.waiting);
    keys.button(VK_LBUTTON,false);capture.keyboard(keys);assert(!capture.waiting);
    keys.button(VK_LCONTROL,true);capture.keyboard(keys);assert(capture.active);
    keys.button('E',true);capture.keyboard(keys);
    assert(capture.completed && capture.result==L"Ctrl+E");
    input::Binding parsed;assert(input::parse(capture.result,parsed) && parsed.key=='E' && parsed.modifiers==input::ctrl);
    keys.focus(true);capture.begin(Device::keyboard,keys,0);keys.button(VK_XBUTTON1,true);capture.keyboard(keys);
    assert(capture.result==L"Mouse4");
    keys.focus(true);capture.begin(Device::keyboard,keys,0);keys.button(VK_F10,true);capture.keyboard(keys);
    assert(capture.result==L"F10");
    keys.focus(true);capture.begin(Device::keyboard,keys,0);keys.button(VK_LSHIFT,true);capture.keyboard(keys);
    keys.button(VK_LSHIFT,false);capture.keyboard(keys);assert(capture.result==L"Shift");
    keys.focus(true);capture.begin(Device::keyboard,keys,0);keys.button(VK_ESCAPE,true);capture.keyboard(keys);
    assert(capture.cancelled && !capture.completed && capture.result.empty());
    keys.focus(true);capture.begin(Device::keyboard,keys,0);keys.focus(false);capture.keyboard(keys);assert(capture.cancelled);

    // Every stored keyboard token must round-trip through the INI parser.
    for(int key=1;key<255;++key){
        assert(input::parse(binding_capture::keyboard_name(key,input::ctrl|input::shift|input::alt|input::win),parsed));
        if(!input::modifier_family(key))assert(parsed.key==key);
    }
    keys.focus(true);capture.begin(Device::controller,keys,XINPUT_GAMEPAD_Y);
    capture.controller_buttons(XINPUT_GAMEPAD_Y,true);assert(capture.waiting);
    capture.controller_buttons(0,true);assert(!capture.waiting);
    capture.controller_buttons(controller::leftTrigger,true);
    capture.controller_buttons(controller::leftTrigger|XINPUT_GAMEPAD_Y,true);
    capture.controller_buttons(XINPUT_GAMEPAD_Y,true);assert(capture.active);
    capture.controller_buttons(0,true);assert(capture.completed && capture.result==L"LT+Y");
    controller::Binding pad;assert(controller::parse(capture.result,pad) && pad.buttons==(controller::leftTrigger|XINPUT_GAMEPAD_Y));
    capture.begin(Device::controller,keys,0);
    capture.controller_buttons(XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_B,true);
    capture.controller_buttons(XINPUT_GAMEPAD_B|XINPUT_GAMEPAD_X,true);
    capture.controller_buttons(0,true);assert(capture.result==L"A+B");
    capture.begin(Device::controller,keys,0);capture.controller_buttons(XINPUT_GAMEPAD_A,true);
    capture.controller_buttons(0,false);assert(capture.cancelled && !capture.completed);
    capture.begin(Device::controller,keys,0);capture.controller_buttons(1u<<31,true);
    capture.controller_buttons(0,true);assert(capture.active && !capture.completed);
    keys.button(VK_ESCAPE,true);capture.keyboard(keys);assert(capture.cancelled);
    std::puts("Rebinding: activation input ignored, keyboard/mouse/modifier capture, token round-trips, controller chords, cancellation and disconnect passed.");
}
