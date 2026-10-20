#pragma once
#include <windows.h>
#include <algorithm>
#include <string>
namespace arrow_switch::indicator {
inline unsigned fit_wrapped_text(HDC dc,const std::wstring& text,RECT bounds,unsigned requested,const std::wstring& family){
    const auto minimum=std::min(10u,requested);
    for(unsigned size=requested;;--size){
        const auto font=CreateFontW(-static_cast<int>(size),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,family.c_str());
        const auto previous=SelectObject(dc,font?font:GetStockObject(DEFAULT_GUI_FONT));
        RECT measured{0,0,bounds.right-bounds.left,0};
        DrawTextW(dc,text.c_str(),-1,&measured,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX|DT_EDITCONTROL);
        SelectObject(dc,previous);if(font)DeleteObject(font);
        if((measured.bottom<=bounds.bottom-bounds.top && measured.right<=bounds.right-bounds.left) || size==minimum)return size;
    }
}
}
