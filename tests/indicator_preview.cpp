#define NOMINMAX
#include "../src/indicator.hpp"
#include "../src/theme_config.hpp"
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
    std::puts("Indicator PNG and D3D12/WARP shader/upload/draw/readback/resize checks passed.");
    return 0;
}
