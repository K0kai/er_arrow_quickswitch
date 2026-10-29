#pragma once
#include "indicator.hpp"
#include <algorithm>
#include <cwchar>
#include <filesystem>

namespace arrow_switch::theme {
// Read appearance independently so reloading cannot change held key bindings.
inline indicator::Settings load(const std::filesystem::path& path) {
    indicator::Settings s;
    const auto file=path.wstring();
    const auto number=[&](const wchar_t* section,const wchar_t* key,int fallback,int low,int high) {
        wchar_t value[64]{},*end{};
        GetPrivateProfileStringW(section,key,L"",value,64,file.c_str());
        if(!*value)return fallback;
        const auto parsed=std::wcstol(value,&end,10);
        return end==value || *end?fallback:static_cast<int>(std::clamp(parsed,static_cast<long>(low),static_cast<long>(high)));
    };
    const auto color=[&](const wchar_t* section,const wchar_t* key,unsigned fallback) {
        wchar_t value[64]{};
        GetPrivateProfileStringW(section,key,L"",value,64,file.c_str());
        if(std::wcslen(value)!=7 || value[0]!=L'#')return fallback;
        unsigned result=0;
        for(int i=1;i<7;++i){
            const auto c=value[i];
            const int digit=c>=L'0'&&c<=L'9'?c-L'0':c>=L'a'&&c<=L'f'?c-L'a'+10:c>=L'A'&&c<=L'F'?c-L'A'+10:-1;
            if(digit<0)return fallback;
            result=(result<<4)|static_cast<unsigned>(digit);
        }
        return result;
    };
    const auto text=[&](const wchar_t* section,const wchar_t* key,const std::wstring& fallback) {
        wchar_t value[1024]{};
        GetPrivateProfileStringW(section,key,fallback.c_str(),value,1024,file.c_str());
        return std::wstring(value);
    };
    s.selectionLayout=static_cast<unsigned>(number(L"Switching",L"SelectionLayout",0,0,3));
    s.blockCameraWhileSelecting=number(L"Switching",L"BlockCameraWhileSelecting",1,0,1)!=0;
#define THEME_NUM(member,key,low,high) s.member=static_cast<unsigned>(number(L"Theme",L##key,static_cast<int>(s.member),low,high))
#define THEME_COLOR(member,key) s.member=color(L"Theme",L##key,s.member)
    THEME_NUM(wheelOpacity,"WheelOpacityPercent",0,100);
    THEME_NUM(wheelScale,"WheelScalePercent",50,150);
    THEME_NUM(cancelRadius,"CancelRadius",30,140);
    THEME_NUM(nameSize,"NameFontSize",12,24);
    THEME_NUM(countSize,"CountFontSize",10,22);
    THEME_NUM(effectSize,"EffectFontSize",9,15);
    THEME_NUM(centerSize,"CenterFontSize",14,28);
    THEME_NUM(iconScale,"IconScalePercent",50,125);
    THEME_NUM(itemIconScale,"ItemIconScalePercent",50,150);
    THEME_NUM(markerScale,"MarkerScalePercent",50,200);
    THEME_NUM(borderScale,"BorderScalePercent",50,200);
    THEME_NUM(brightness,"BrightnessPercent",50,200);
    THEME_NUM(contentBrightness,"ContentBrightnessPercent",75,150);
    THEME_NUM(selectionGlow,"SelectionGlowPercent",0,100);
    THEME_NUM(iconBackdrop,"IconBackdropOpacityPercent",0,80);
    THEME_NUM(paperWhite,"HdrPaperWhiteNits",80,400);
    THEME_COLOR(selectionColor,"SelectionColor");
    THEME_COLOR(accentColor,"AccentColor");
    THEME_COLOR(backgroundColor,"BackgroundColor");
    THEME_COLOR(selectedFillColor,"SelectedFillColor");
    THEME_COLOR(textColor,"TextColor");
    THEME_COLOR(mutedColor,"MutedTextColor");
    THEME_COLOR(countColor,"CountColor");
    THEME_COLOR(selectedTextColor,"SelectedTextColor");
#undef THEME_NUM
#undef THEME_COLOR
    s.wheelOffsetX=number(L"Theme",L"OffsetX",0,-2000,2000);
    s.wheelOffsetY=number(L"Theme",L"OffsetY",0,-2000,2000);
    s.showEffectIcons=number(L"Theme",L"ShowEffectIcons",1,0,1)!=0;
    s.showIcons=number(L"Theme",L"ShowIcons",1,0,1)!=0;
    s.showItemIcons=number(L"Theme",L"ShowItemIcons",1,0,1)!=0;
    s.showMarker=number(L"Theme",L"ShowSelectionMarker",1,0,1)!=0;
    s.decorativeRings=number(L"Theme",L"DecorativeRings",1,0,1)!=0;
    s.animateSelection=number(L"Theme",L"AnimateSelection",1,0,1)!=0;
    s.showEffectLabels=number(L"Theme",L"ShowEffectLabels",0,0,1)!=0;
    s.showCounts=number(L"Theme",L"ShowCounts",1,0,1)!=0;
    s.textShadow=number(L"Theme",L"TextShadow",1,0,1)!=0;
    s.fontFamily=text(L"Theme",L"FontFamily",s.fontFamily);
    s.detailFont=text(L"Theme",L"DetailFontFamily",s.detailFont);
    s.selectionImage=text(L"Theme",L"SelectionImage",L"");
    s.iconSourceDirectory=text(L"Theme",L"IconSourceDirectory",L"");
    // Enabled retains its legacy meaning: enable the overlay, including wheels.
    s.enabled=number(L"Indicator",L"Enabled",1,0,1)!=0;
    s.showIndicator=number(L"Indicator",L"ShowAmmoText",1,0,1)!=0;
    s.alwaysShow=number(L"Indicator",L"AlwaysShow",0,0,1)!=0;
    s.duration=static_cast<unsigned>(number(L"Indicator",L"DurationMs",3500,500,30000));
    s.scale=static_cast<unsigned>(number(L"Indicator",L"ScalePercent",100,50,200));
    s.indicatorNameSize=static_cast<unsigned>(number(L"Indicator",L"NameFontSize",22,12,30));
    s.indicatorGap=static_cast<unsigned>(number(L"Indicator",L"Gap",24,0,180));
    s.indicatorOffsetX=number(L"Indicator",L"OffsetX",0,-2000,2000);
    s.indicatorOffsetY=number(L"Indicator",L"OffsetY",0,-2000,2000);
    s.fadeIn=static_cast<unsigned>(number(L"Indicator",L"FadeInMs",180,0,3000));
    s.fadeOut=static_cast<unsigned>(number(L"Indicator",L"FadeOutMs",600,0,5000));
    s.indicatorTextColor=color(L"Indicator",L"TextColor",s.indicatorTextColor);
    s.indicatorFont=text(L"Indicator",L"FontFamily",s.indicatorFont);
    return s;
}
inline bool save(const std::filesystem::path& path,const indicator::Settings& s) {
    bool ok=true;const auto file=path.wstring();
    const auto write=[&](const wchar_t* section,const wchar_t* key,const std::wstring& value){
        if(!WritePrivateProfileStringW(section,key,value.c_str(),file.c_str()))ok=false;
    };
    const auto color=[&](const wchar_t* section,const wchar_t* key,unsigned value){
        wchar_t text[8]{};swprintf_s(text,L"#%06X",value&0xFFFFFF);write(section,key,text);
    };
    write(L"Switching",L"SelectionLayout",std::to_wstring(s.selectionLayout));
    write(L"Switching",L"BlockCameraWhileSelecting",s.blockCameraWhileSelecting?L"1":L"0");
#define SAVE_NUM(section,member,key) write(L##section,L##key,std::to_wstring(s.member))
#define SAVE_COLOR(section,member,key) color(L##section,L##key,s.member)
    SAVE_NUM("Theme",wheelOpacity,"WheelOpacityPercent");SAVE_NUM("Theme",wheelScale,"WheelScalePercent");
    SAVE_NUM("Theme",wheelOffsetX,"OffsetX");SAVE_NUM("Theme",wheelOffsetY,"OffsetY");SAVE_NUM("Theme",cancelRadius,"CancelRadius");
    SAVE_NUM("Theme",nameSize,"NameFontSize");SAVE_NUM("Theme",countSize,"CountFontSize");SAVE_NUM("Theme",effectSize,"EffectFontSize");SAVE_NUM("Theme",centerSize,"CenterFontSize");
    SAVE_NUM("Theme",iconScale,"IconScalePercent");SAVE_NUM("Theme",itemIconScale,"ItemIconScalePercent");
    SAVE_NUM("Theme",markerScale,"MarkerScalePercent");SAVE_NUM("Theme",borderScale,"BorderScalePercent");
    SAVE_NUM("Theme",brightness,"BrightnessPercent");SAVE_NUM("Theme",paperWhite,"HdrPaperWhiteNits");
    SAVE_NUM("Theme",contentBrightness,"ContentBrightnessPercent");SAVE_NUM("Theme",selectionGlow,"SelectionGlowPercent");SAVE_NUM("Theme",iconBackdrop,"IconBackdropOpacityPercent");
    SAVE_NUM("Theme",decorativeRings,"DecorativeRings");SAVE_NUM("Theme",animateSelection,"AnimateSelection");
    SAVE_NUM("Theme",showIcons,"ShowIcons");SAVE_NUM("Theme",showItemIcons,"ShowItemIcons");SAVE_NUM("Theme",showEffectIcons,"ShowEffectIcons");
    SAVE_NUM("Theme",showEffectLabels,"ShowEffectLabels");SAVE_NUM("Theme",showCounts,"ShowCounts");SAVE_NUM("Theme",textShadow,"TextShadow");SAVE_NUM("Theme",showMarker,"ShowSelectionMarker");
    SAVE_COLOR("Theme",selectionColor,"SelectionColor");SAVE_COLOR("Theme",accentColor,"AccentColor");
    SAVE_COLOR("Theme",backgroundColor,"BackgroundColor");SAVE_COLOR("Theme",selectedFillColor,"SelectedFillColor");
    SAVE_COLOR("Theme",textColor,"TextColor");SAVE_COLOR("Theme",mutedColor,"MutedTextColor");SAVE_COLOR("Theme",countColor,"CountColor");SAVE_COLOR("Theme",selectedTextColor,"SelectedTextColor");
    write(L"Theme",L"FontFamily",s.fontFamily);write(L"Theme",L"DetailFontFamily",s.detailFont);
    write(L"Theme",L"SelectionImage",s.selectionImage);write(L"Theme",L"IconSourceDirectory",s.iconSourceDirectory);
    SAVE_NUM("Indicator",enabled,"Enabled");SAVE_NUM("Indicator",showIndicator,"ShowAmmoText");SAVE_NUM("Indicator",alwaysShow,"AlwaysShow");
    SAVE_NUM("Indicator",duration,"DurationMs");SAVE_NUM("Indicator",scale,"ScalePercent");
    SAVE_NUM("Indicator",indicatorNameSize,"NameFontSize");
    SAVE_NUM("Indicator",indicatorGap,"Gap");SAVE_NUM("Indicator",indicatorOffsetX,"OffsetX");SAVE_NUM("Indicator",indicatorOffsetY,"OffsetY");
    SAVE_NUM("Indicator",fadeIn,"FadeInMs");SAVE_NUM("Indicator",fadeOut,"FadeOutMs");
    SAVE_COLOR("Indicator",indicatorTextColor,"TextColor");
    write(L"Indicator",L"FontFamily",s.indicatorFont);
#undef SAVE_NUM
#undef SAVE_COLOR
    return ok;
}
}
