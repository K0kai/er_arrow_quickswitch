#include "../src/wheel.hpp"
#include <cassert>
int main(){
    using namespace arrow_switch;
    assert(wheel::page_count(0)==0 && wheel::page_count(8)==1 && wheel::page_count(9)==2);
    assert(wheel::page_items(17,0)==8 && wheel::page_items(17,1)==8 && wheel::page_items(17,2)==1);
    assert(wheel::turn_page(0,3,1)==1 && wheel::turn_page(0,3,-1)==2);
    assert(wheel::turn_page(2,3,1)==0 && wheel::turn_page(1,3,-1)==0);
    std::vector<Arrow> arrows{{100,1,5,0,81},{200,2,0,1,81},{300,3,4,2,81},{400,4,9,3,83}};
    auto items=wheel::eligible(arrows,100,300,AmmoSlot::arrow1);
    assert(items.size()==2 && items[0].id==100 && items[1].id==400);
    items=wheel::eligible(arrows,100,300,AmmoSlot::arrow1,true);
    assert(items.size()==1 && items[0].id==100);
    items=wheel::eligible(arrows,400,100,AmmoSlot::arrow1,true);
    assert(items.size()==1 && items[0].id==400);
    items=wheel::eligible(arrows,100,0,AmmoSlot::arrow2);
    assert(items.size()==3 && items[0].id==100 && items[1].id==300 && items[2].id==400);
    assert(wheel::eligible(arrows,999,0,AmmoSlot::arrow1).empty());
    assert(wheel::selection(0,0,4)==-1);
    assert(wheel::selection(60,0,4)==-1);
    assert(wheel::selection(90,0,4)==1);
    assert(wheel::selection(90,0,4,100)==-1);
    assert(wheel::selection(0,-100,4)==0);
    assert(wheel::selection(100,0,4)==1);
    assert(wheel::selection(0,100,4)==2);
    assert(wheel::selection(-100,0,4)==3);
    assert(wheel::selection(100,0,0)==-1);
    assert(wheel::affinity(L"Lightningbone Arrow").badge==L'\u26a1');
    assert(wheel::affinity(L"Bloodbone Arrow").badge==L'\u2666');
    assert(wheel::affinity(L"Scarlet Rot Arrow").badge==L'\u2623');
    assert(wheel::affinity(L"Haligbone Arrow").badge==L'\u2726');
    assert(std::wstring(wheel::affinity(L"Haligbone Arrow").label)==L"HOLY");
    assert(wheel::affinity(L"Plain Modded Arrow").badge==L'\u2191');
    assert(std::wstring(wheel::affinity(L"Deathbone Arrow").label)==L"DEATH BLIGHT");
    assert(std::wstring(wheel::affinity(L"Death Blight Bolt").label)==L"DEATH BLIGHT");
    assert(std::wstring(wheel::affinity(L"Frenzied Flame Arrow").label)==L"MADNESS");
    assert(std::wstring(wheel::affinity(L"Madness Bolt").label)==L"MADNESS");
    const std::vector<Arrow> bolts{{500,5,5,4,85},{600,6,4,5,85},{700,7,3,6,81},{800,8,3,7,86}};
    auto boltItems=wheel::eligible(bolts,500,600,AmmoSlot::bolt1);
    assert(boltItems.size()==2 && boltItems[0].id==500 && boltItems[1].id==800);
    boltItems=wheel::eligible(bolts,500,600,AmmoSlot::bolt1,true);
    assert(boltItems.size()==1 && boltItems[0].id==500);
    boltItems=wheel::eligible(bolts,800,500,AmmoSlot::bolt1,true);
    assert(boltItems.size()==1 && boltItems[0].id==800);
    assert(wheel::damage_affinity(100,100,175,100,0).badge==L'\u2668');
    assert(wheel::damage_affinity(100,100,100,100,0).badge==L'\u2191');
}
