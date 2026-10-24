#pragma once
#include <windows.h>
#include <string>
namespace arrow_switch::indicator {
inline HFONT create_text_font(unsigned size,const std::wstring& family){
    return CreateFontW(-static_cast<int>(size),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,family.c_str());
}
}
