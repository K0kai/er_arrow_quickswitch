#define NOMINMAX
#include "../src/theme_config.hpp"
#include "../src/ui_layout.hpp"
#include "../src/config_file.hpp"
#include <cassert>
#include <cmath>
#include <fstream>

int main() {
    using namespace arrow_switch;
    const auto file=std::filesystem::current_path()/(L"theme-test-"+std::to_wstring(GetCurrentProcessId())+L".ini");
    assert(config_file::ensure(file));
    auto s=theme::load(file);
    assert(s.textColor==0xF4EBD6 && s.wheelOpacity==72 && s.wheelScale==100);
    assert(s.showCounts && !s.showEffectLabels && s.showIndicator);
    {
        std::ofstream output(file,std::ios::trunc);
        output<<"[Theme]\nOffsetX=-120\nWheelOpacityPercent=0\nNameFontSize=999\n"
                "SelectedTextColor=#00aAFF\nTextColor=#12345Z\nCountColor=#1234567\n"
                "WheelScalePercent=bad\nSelectionImage=my-marker.png\nFontFamily=Georgia\n"
                "[Indicator]\nScalePercent=150\nShowAmmoText=0\nFadeInMs=0\nOffsetY=-25\n"
                "[Controls]\nArrow1WheelKey=Ctrl+Q\nSettingsKey=None\n";
    }
    s=theme::load(file);
    assert(s.wheelOffsetX==-120 && s.wheelOpacity==0 && s.nameSize==24);
    assert(s.selectedTextColor==0x00AAFF && s.textColor==0xF4EBD6 && s.countColor==0xD9CFB9);
    assert(s.wheelScale==100 && s.selectionImage==L"my-marker.png" && s.fontFamily==L"Georgia");
    assert(s.scale==150 && !s.showIndicator && s.enabled && s.fadeIn==0 && s.indicatorOffsetY==-25);
    assert(config_file::ensure(file));
    wchar_t binding[64]{};
    GetPrivateProfileStringW(L"Controls",L"Arrow1WheelKey",L"",binding,64,file.c_str());
    assert(std::wstring(binding)==L"Ctrl+Q");
    s.indicatorTextColor=0xABCDEF;s.fontFamily=L"Georgia";s.wheelOffsetY=-75;s.showCounts=false;s.selectionLayout=2;
    assert(theme::save(file,s));
    const auto saved=theme::load(file);
    assert(saved.indicatorTextColor==0xABCDEF && saved.fontFamily==L"Georgia" && saved.wheelOffsetY==-75 && !saved.showCounts);
    assert(saved.selectionLayout==2);
    GetPrivateProfileStringW(L"Controls",L"SettingsKey",L"",binding,64,file.c_str());
    assert(std::wstring(binding)==L"None");
    s=theme::load(file);
    assert(s.wheelOffsetX==-120 && !s.showIndicator && s.textColor==0xF4EBD6);
    std::filesystem::remove(file);
    for(const auto [width,height]:{std::pair{640.f,360.f},std::pair{1920.f,1080.f},std::pair{3440.f,1440.f}}){
        for(unsigned ws:{50u,100u,150u})for(unsigned is:{50u,100u,200u}){
            const auto l=indicator::layout(width,height,ws,is,180,2000,-2000,-2000,2000);
            for(const auto& rect:{l.wheel,l.indicator}){
                assert(rect.x>=7.99f && rect.y>=7.99f);
                assert(rect.x+rect.width<=width-7.99f && rect.y+rect.height<=height-7.99f);
            }
            assert(std::abs(l.wheel.width-l.wheel.height)<.01f);
            assert(std::abs(l.indicator.width/l.indicator.height-420.f/88.f)<.01f);
        }
        const auto normal=indicator::layout(width,height,100,100,24,0,0,0,0);
        assert(normal.indicator.y>=normal.wheel.y+normal.wheel.height);
    }
}
