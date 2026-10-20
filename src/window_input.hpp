#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <array>
#include <mutex>

namespace arrow_switch::window_input {
// Current button states only: no character messages, text buffers or event log.
struct State {
    std::array<bool,256> held{};
    bool focused=false;
    bool down(int key) const {
        if(!focused || key<=0 || key>=256)return false;
        if(key==VK_CONTROL)return held[VK_LCONTROL] || held[VK_RCONTROL];
        if(key==VK_SHIFT)return held[VK_LSHIFT] || held[VK_RSHIFT];
        if(key==VK_MENU)return held[VK_LMENU] || held[VK_RMENU];
        return held[static_cast<unsigned>(key)];
    }
    void focus(bool value){held.fill(false);focused=value;}
    void button(unsigned key,bool value){if(focused && key<held.size())held[key]=value;}
    void message(UINT message,WPARAM wParam,LPARAM lParam){
        if(message==WM_SETFOCUS){focus(true);return;}
        if(message==WM_KILLFOCUS || (message==WM_ACTIVATEAPP && !wParam) ||
           (message==WM_ACTIVATE && LOWORD(wParam)==WA_INACTIVE) || message==WM_NCDESTROY){focus(false);return;}
        if(!focused)return;
        if(message==WM_KEYDOWN || message==WM_SYSKEYDOWN || message==WM_KEYUP || message==WM_SYSKEYUP){
            unsigned key=static_cast<unsigned>(wParam);
            if(key==VK_SHIFT)key=((static_cast<ULONG_PTR>(lParam)>>16)&255)==0x36?VK_RSHIFT:VK_LSHIFT;
            if(key==VK_CONTROL)key=(lParam&(1LL<<24))?VK_RCONTROL:VK_LCONTROL;
            if(key==VK_MENU)key=(lParam&(1LL<<24))?VK_RMENU:VK_LMENU;
            const bool pressed=message==WM_KEYDOWN || message==WM_SYSKEYDOWN;
            // A held key returning from another window must be released first.
            if(pressed && (lParam&(1LL<<30)) && !down(static_cast<int>(key)))return;
            button(key,pressed);return;
        }
        switch(message){
        case WM_LBUTTONDOWN:case WM_LBUTTONDBLCLK:button(VK_LBUTTON,true);break;
        case WM_LBUTTONUP:button(VK_LBUTTON,false);break;
        case WM_RBUTTONDOWN:case WM_RBUTTONDBLCLK:button(VK_RBUTTON,true);break;
        case WM_RBUTTONUP:button(VK_RBUTTON,false);break;
        case WM_MBUTTONDOWN:case WM_MBUTTONDBLCLK:button(VK_MBUTTON,true);break;
        case WM_MBUTTONUP:button(VK_MBUTTON,false);break;
        case WM_XBUTTONDOWN:case WM_XBUTTONDBLCLK:button(HIWORD(wParam)==XBUTTON1?VK_XBUTTON1:VK_XBUTTON2,true);break;
        case WM_XBUTTONUP:button(HIWORD(wParam)==XBUTTON1?VK_XBUTTON1:VK_XBUTTON2,false);break;
        case WM_CAPTURECHANGED:
            for(unsigned key:{VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2})held[key]=false;
            break;
        }
    }
    void raw_mouse(const RAWMOUSE& mouse){
        constexpr USHORT downFlags[]{RI_MOUSE_LEFT_BUTTON_DOWN,RI_MOUSE_RIGHT_BUTTON_DOWN,RI_MOUSE_MIDDLE_BUTTON_DOWN,RI_MOUSE_BUTTON_4_DOWN,RI_MOUSE_BUTTON_5_DOWN};
        constexpr USHORT upFlags[]{RI_MOUSE_LEFT_BUTTON_UP,RI_MOUSE_RIGHT_BUTTON_UP,RI_MOUSE_MIDDLE_BUTTON_UP,RI_MOUSE_BUTTON_4_UP,RI_MOUSE_BUTTON_5_UP};
        constexpr unsigned keys[]{VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2};
        for(unsigned i=0;i<5;++i){if(mouse.usButtonFlags&downFlags[i])button(keys[i],true);if(mouse.usButtonFlags&upFlags[i])button(keys[i],false);}
    }
};
inline std::mutex mutex;
inline State state;
inline State snapshot(){std::lock_guard<std::mutex> guard(mutex);return state;}
inline void focus(bool value){std::lock_guard<std::mutex> guard(mutex);state.focus(value);}
inline void message(UINT value,WPARAM wParam,LPARAM lParam){std::lock_guard<std::mutex> guard(mutex);state.message(value,wParam,lParam);}
inline void raw_mouse(const RAWMOUSE& value){std::lock_guard<std::mutex> guard(mutex);state.raw_mouse(value);}
}
