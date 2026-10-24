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
    const auto font=arrow_switch::indicator::create_text_font(18,L"Palatino Linotype");assert(font);
    const auto previous=SelectObject(dc,font);
    // The reported name needs more than the old 140-pixel column. The wider
    // list name row accommodates it without changing the requested font size.
    RECT measured{};
    DrawTextW(dc,L"Lightningbone Bolt",-1,&measured,DT_CALCRECT|DT_SINGLELINE|DT_NOPREFIX);
    assert(measured.right>140 && measured.right<=198);
    SelectObject(dc,previous);DeleteObject(font);
    DeleteDC(dc);
}
