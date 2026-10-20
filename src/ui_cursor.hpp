#pragma once
#include <algorithm>
namespace arrow_switch::settings_ui {
struct Cursor {
    float x=0,y=0;bool initialized=false;
    void move(float dx,float dy,float width,float height){
        if(!initialized){x=width*.5f;y=height*.5f;initialized=true;}
        x=std::clamp(x+dx,0.f,std::max(0.f,width-1));
        y=std::clamp(y+dy,0.f,std::max(0.f,height-1));
    }
};
}
