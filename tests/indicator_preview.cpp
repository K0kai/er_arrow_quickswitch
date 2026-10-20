#define NOMINMAX
#include "../src/indicator.hpp"
#include "../src/theme_config.hpp"
#include "../src/settings_ui.hpp"
#include "../src/window_input.hpp"
#include <cassert>
#include <cstdio>
#include <cstdarg>
void report(const char* text,...) {
    va_list args; va_start(args,text); std::vprintf(text,args); va_end(args); std::putchar('\n');
}
int wmain(int argc,wchar_t** argv) {
    if (argc<2 || argc>3) return 2;
    const auto settings=argc==3?arrow_switch::theme::load(argv[2]):arrow_switch::indicator::Settings{};
    if (!arrow_switch::indicator::preview(argv[1],report,&settings)) {
        std::puts("Indicator raster or D3D12/WARP pipeline test failed."); return 1;
    }
    // Keyboard tracking must work before ImGui is initialized, and survive
    // graphics shutdown without losing the window subclass.
    const auto instance=GetModuleHandleW(nullptr);
    WNDCLASSW windowClass{};windowClass.lpfnWndProc=DefWindowProcW;
    windowClass.hInstance=instance;windowClass.lpszClassName=L"ArrowWheel_InputTest";
    assert(RegisterClassW(&windowClass));
    const auto window=CreateWindowW(windowClass.lpszClassName,L"Input test",0,0,0,100,100,nullptr,nullptr,instance,nullptr);
    assert(window && arrow_switch::settings_ui::attach_input_window(window));
    SendMessageW(window,WM_SETFOCUS,0,0);
    SendMessageW(window,WM_KEYDOWN,VK_F10,0);
    assert(arrow_switch::window_input::snapshot().down(VK_F10));
    arrow_switch::settings_ui::shutdown();
    SendMessageW(window,WM_KEYUP,VK_F10,0);
    assert(!arrow_switch::window_input::snapshot().down(VK_F10));
    SendMessageW(window,WM_KEYDOWN,'1',0);
    assert(arrow_switch::window_input::snapshot().down('1'));
    SendMessageW(window,WM_KILLFOCUS,0,0);
    assert(!arrow_switch::window_input::snapshot().down('1'));
    DestroyWindow(window);UnregisterClassW(windowClass.lpszClassName,instance);
    assert(!arrow_switch::window_input::snapshot().focused);
    std::puts("Window subclass: input before panel creation, graphics shutdown and focus loss passed.");
    std::puts("Indicator PNG and D3D12/WARP shader/upload/draw/readback/resize checks passed.");
    return 0;
}
