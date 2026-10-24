#define NOMINMAX
#include "../src/ui_cursor.hpp"
#include "../src/text_layout.hpp"
#include <cassert>
int main(){
    arrow_switch::settings_ui::Cursor cursor;
    cursor.move(0,0,1920,1080);assert(cursor.x==960 && cursor.y==540);
    cursor.move(120,-40,1920,1080);assert(cursor.x==1080 && cursor.y==500);
    // OS cursor positions do not enter this state: successive raw deltas persist.
    cursor.move(20,10,1920,1080);assert(cursor.x==1100 && cursor.y==510);
    cursor.move(10000,-10000,1920,1080);assert(cursor.x==1919 && cursor.y==0);
    cursor.move(0,0,640,360);assert(cursor.x==639 && cursor.y==0);
    HDC dc=CreateCompatibleDC(nullptr);assert(dc);
    RECT area{0,0,116,52};
    assert(arrow_switch::indicator::fit_wrapped_text(dc,L"Arrow",area,18,L"Palatino Linotype")==18);
    for(const auto& name:{L"Bloodbone Arrow (Fletched)",L"Poisonbone Arrow (Fletched)",L"Haligbone Arrow (Fletched)"}){
        const auto size=arrow_switch::indicator::fit_wrapped_text(dc,name,area,18,L"Palatino Linotype");
        assert(size>=10 && size<18);
        const auto font=CreateFontW(-static_cast<int>(size),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Palatino Linotype");
        const auto previous=SelectObject(dc,font);RECT measured{0,0,116,0};
        DrawTextW(dc,name,-1,&measured,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX|DT_EDITCONTROL);
        assert(measured.bottom<=52 && measured.right<=116);
        SelectObject(dc,previous);DeleteObject(font);
    }
    DeleteDC(dc);
}
