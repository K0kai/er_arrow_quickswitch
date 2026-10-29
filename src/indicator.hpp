#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>

namespace arrow_switch::indicator {
using Logger=void(*)(const char*,...);
struct Settings {
    unsigned selectionLayout=0; // 0 wheel, 1 strip, 2 list, 3 quick tap with strip on hold
    bool enabled=true,alwaysShow=false;
    unsigned duration=3500,scale=100,left=40,bottom=240;
    unsigned wheelOpacity=84, cancelRadius=78, selectionColor=0xE1C484, accentColor=0xA18C61;
    unsigned wheelScale=100, backgroundColor=0x141815, selectedFillColor=0x49402D;
    unsigned textColor=0xF4EBD6, mutedColor=0xBDB49F, countColor=0xD9CFB9, selectedTextColor=0xFFE6A6;
    unsigned nameSize=18, countSize=16, effectSize=11, centerSize=22;
    unsigned iconScale=90, markerScale=100, borderScale=100, brightness=100, paperWhite=200;
    unsigned indicatorNameSize=22, indicatorGap=24;
    unsigned indicatorTextColor=0xF4EBD6, fadeIn=180, fadeOut=600;
    int wheelOffsetX=0,wheelOffsetY=0,indicatorOffsetX=0,indicatorOffsetY=0;
    bool showIcons=true,showEffectIcons=true,showEffectLabels=false,showCounts=true,showIndicator=true,textShadow=true,showMarker=true;
    bool blockCameraWhileSelecting=true;
    bool showItemIcons=true;
    unsigned itemIconScale=100;
    unsigned contentBrightness=115,selectionGlow=70,iconBackdrop=45;
    bool decorativeRings=true,animateSelection=true;
    std::wstring iconSourceDirectory;
    std::wstring fontFamily=L"Palatino Linotype",detailFont=L"Segoe UI",indicatorFont=L"Palatino Linotype";
    std::wstring selectionImage;
};
bool install(HMODULE module,const Settings& settings,Logger logger);
// Applied by the rendering thread between frames.
void configure(const Settings& settings);
void publish(const std::wstring& name,std::uint32_t id,std::uint32_t count,const wchar_t* slotLabel=L"ARROW SLOT 1");
struct WheelItem { std::wstring name; std::uint32_t count{}; wchar_t badge=L'\u2191';
    std::uint32_t color=0xD1C39A; std::wstring effect=L"ARROW"; std::uint16_t iconsMask{}; std::uint32_t iconId{}; };
void publish_wheel(const std::vector<WheelItem>& items,int selected,bool open,const wchar_t* slotLabel=L"ARROW SLOT 1");
void set_gameplay(bool value);
// Standalone raster and D3D12/WARP verification, never used by the game.
bool preview(const wchar_t* filename,Logger logger=nullptr,const Settings* options=nullptr);
}
