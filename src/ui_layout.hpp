#pragma once
#include <algorithm>

namespace arrow_switch::indicator {
struct Rect { float x,y,width,height; };
struct Layout { Rect wheel,indicator; };
// Coordinates and spacing are reference pixels at 1080p. Fit uniformly to the
// screen so a small window never stretches the circle into an ellipse.
inline Layout layout(float width,float height,unsigned wheelScale,unsigned indicatorScale,
                     unsigned gap,int wheelX,int wheelY,int indicatorX,int indicatorY) {
    const float base=height/1080.f;
    float ws=base*wheelScale/100.f,is=base*indicatorScale/100.f;
    const float spacing=base*gap;
    const float fit=std::min({1.f,(width-16.f)/std::max(640.f*ws,420.f*is),
        (height-16.f)/(640.f*ws+spacing+88.f*is)});
    ws*=fit;is*=fit;
    const float total=640.f*ws+spacing*fit+88.f*is;
    Rect wheel{(width-640.f*ws)*.5f+wheelX*base,(height-total)*.5f+wheelY*base,640.f*ws,640.f*ws};
    wheel.x=std::clamp(wheel.x,8.f,std::max(8.f,width-wheel.width-8.f));
    wheel.y=std::clamp(wheel.y,8.f,std::max(8.f,height-wheel.height-8.f));
    Rect card{wheel.x+wheel.width*.5f-210.f*is+indicatorX*base,
        wheel.y+wheel.height+spacing*fit+indicatorY*base,420.f*is,88.f*is};
    card.x=std::clamp(card.x,8.f,std::max(8.f,width-card.width-8.f));
    card.y=std::clamp(card.y,8.f,std::max(8.f,height-card.height-8.f));
    return {wheel,card};
}
}
