#include "../src/window_input.hpp"
#include "../src/bindings.hpp"
#include <cassert>
#include <cstdio>
using namespace arrow_switch;
int main(){
    window_input::State state;
    state.message(WM_KEYDOWN,'E',0);assert(!state.down('E'));
    state.message(WM_SETFOCUS,0,0);
    state.message(WM_KEYDOWN,VK_CONTROL,0);assert(state.down(VK_CONTROL) && state.down(VK_LCONTROL));
    state.message(WM_KEYDOWN,VK_CONTROL,1LL<<24);assert(state.down(VK_RCONTROL));
    state.message(WM_KEYUP,VK_CONTROL,0);assert(state.down(VK_CONTROL));
    input::Binding chord;assert(input::parse(L"Ctrl+E",chord));
    state.message(WM_KEYDOWN,'E',0);assert(state.down(chord.key) && input::matches(chord,input::ctrl));
    state.message(WM_KEYUP,VK_CONTROL,1LL<<24);assert(!state.down(VK_CONTROL));
    state.message(WM_KEYDOWN,VK_SHIFT,0x36LL<<16);assert(state.down(VK_SHIFT) && state.down(VK_RSHIFT));
    state.message(WM_KEYUP,VK_SHIFT,0x36LL<<16);assert(!state.down(VK_SHIFT));
    state.message(WM_SYSKEYDOWN,VK_MENU,1LL<<24);assert(state.down(VK_RMENU));
    state.message(WM_SYSKEYUP,VK_MENU,1LL<<24);assert(!state.down(VK_MENU));
    state.message(WM_KEYDOWN,VK_F10,0);assert(state.down(VK_F10));
    state.message(WM_KEYDOWN,VK_F10,1LL<<30);assert(state.down(VK_F10));
    state.message(WM_KILLFOCUS,0,0);assert(!state.down('E') && !state.down(VK_F10));
    state.message(WM_SETFOCUS,0,0);
    state.message(WM_KEYDOWN,VK_F10,1LL<<30);assert(!state.down(VK_F10));
    state.message(WM_KEYUP,VK_F10,0);state.message(WM_KEYDOWN,VK_F10,0);assert(state.down(VK_F10));
    state.message(WM_LBUTTONDOWN,0,0);assert(state.down(VK_LBUTTON));
    state.message(WM_LBUTTONUP,0,0);assert(!state.down(VK_LBUTTON));
    state.message(WM_XBUTTONDOWN,MAKEWPARAM(0,XBUTTON1),0);assert(state.down(VK_XBUTTON1));
    state.message(WM_XBUTTONUP,MAKEWPARAM(0,XBUTTON1),0);assert(!state.down(VK_XBUTTON1));
    state.message(WM_XBUTTONDBLCLK,MAKEWPARAM(0,XBUTTON2),0);assert(state.down(VK_XBUTTON2));
    state.message(WM_CAPTURECHANGED,0,0);assert(!state.down(VK_XBUTTON2));
    RAWMOUSE mouse{};mouse.usButtonFlags=RI_MOUSE_RIGHT_BUTTON_DOWN|RI_MOUSE_BUTTON_4_DOWN;
    state.raw_mouse(mouse);assert(state.down(VK_RBUTTON) && state.down(VK_XBUTTON1));
    mouse.usButtonFlags=RI_MOUSE_RIGHT_BUTTON_UP|RI_MOUSE_BUTTON_4_UP;
    state.raw_mouse(mouse);assert(!state.down(VK_RBUTTON) && !state.down(VK_XBUTTON1));
    state.message(WM_ACTIVATEAPP,FALSE,0);mouse.usButtonFlags=RI_MOUSE_RIGHT_BUTTON_DOWN;
    state.raw_mouse(mouse);assert(!state.down(VK_RBUTTON));
    state.message(WM_SETFOCUS,0,0);state.message(WM_NCDESTROY,0,0);assert(!state.focused);
    std::puts("Window input: chords, sided modifiers, F10, repeats, focus reset, mouse buttons and raw-button fallback passed.");
}
