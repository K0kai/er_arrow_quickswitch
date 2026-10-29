#include "../src/wheel.hpp"
#include "../src/list_navigation.hpp"
#include <cassert>
int main(){
    using namespace arrow_switch;
    assert(wheel::page_count(0)==0 && wheel::page_count(8)==1 && wheel::page_count(9)==2);
    assert(wheel::page_items(17,0)==8 && wheel::page_items(17,1)==8 && wheel::page_items(17,2)==1);
    assert(wheel::page_count(9,4)==3 && wheel::page_items(9,2,4)==1);
    assert(wheel::page_size(0)==8 && wheel::page_size(1)==4 && wheel::page_size(2)==8 && wheel::page_size(3)==4);
    assert(wheel::turn_page(0,3,1)==1 && wheel::turn_page(0,3,-1)==2);
    assert(wheel::turn_page(2,3,1)==0 && wheel::turn_page(1,3,-1)==0);
    std::vector<Arrow> arrows{{100,1,5,0,81},{200,2,0,1,81},{300,3,4,2,81},{400,4,9,3,83}};
    auto items=wheel::eligible(arrows,100,300,AmmoSlot::arrow1);
    assert(items.size()==2 && items[0].id==100 && items[1].id==400);
    assert(wheel::next_eligible_id(items,100)==400);
    assert(wheel::next_eligible_id(items,400)==100);
    assert(wheel::next_eligible_id(items,999)==0xFFFFFFFFu);
    assert(wheel::release_target(0,false,500,false,1,0,items,100)==400);
    assert(wheel::release_target(1,false,500,false,1,0,items,100)==0xFFFFFFFFu);
    assert(wheel::release_target(1,false,500,true,1,0,items,100)==400);
    assert(wheel::release_target(2,false,500,true,1,0,items,100)==400);
    assert(wheel::release_target(3,false,100,false,0,0,items,400)==400);
    assert(wheel::release_target(3,true,100,false,0,0,items,400)==0xFFFFFFFFu);
    assert(wheel::release_target(3,true,300,true,1,0,items,100)==400);
    assert(wheel::release_target(3,false,100,false,0,0,items,999)==0xFFFFFFFFu);
    items=wheel::eligible(arrows,100,300,AmmoSlot::arrow1,true);
    assert(items.size()==1 && items[0].id==100);
    assert(wheel::next_eligible_id(items,100)==0xFFFFFFFFu);
    items=wheel::eligible(arrows,400,100,AmmoSlot::arrow1,true);
    assert(items.size()==1 && items[0].id==400);
    items=wheel::eligible(arrows,100,0,AmmoSlot::arrow2);
    assert(items.size()==3 && items[0].id==100 && items[1].id==300 && items[2].id==400);
    assert(wheel::eligible(arrows,999,0,AmmoSlot::arrow1).empty());
    for(const auto slot:{AmmoSlot::arrow1,AmmoSlot::arrow2}){
        const auto emptyItems=wheel::eligible(arrows,empty_ammo_id,300,slot,true);
        assert(emptyItems.size()==2 && emptyItems[0].id==100 && emptyItems[1].id==400);
        assert(wheel::eligible(arrows,empty_ammo_id,300,slot,false).size()==emptyItems.size());
    }
    assert(wheel::selection(0,0,4)==-1);
    assert(wheel::selection(60,0,4)==-1);
    assert(wheel::selection(90,0,4)==1);
    assert(wheel::selection(90,0,4,100)==-1);
    assert(wheel::selection(0,-100,4)==0);
    assert(wheel::selection(100,0,4)==1);
    assert(wheel::selection(0,100,4)==2);
    assert(wheel::selection(-100,0,4)==3);
    assert(wheel::selection(100,0,0)==-1);
    list_navigation::Cursor list;
    list.open(7);assert(!list.navigated);
    list.move(1,17);assert(list.index==8 && list.index/wheel::pageSize==1 && list.navigated);
    list.move(-1,17);assert(list.index==7 && list.index/wheel::pageSize==0);
    list.open(16);list.move(1,17);assert(list.index==0);
    list.move(-1,17);assert(list.index==16 && list.index/wheel::pageSize==2);
    list.open();list.move(1,17);assert(list.index==0);
    list.open();list.move(-1,17);assert(list.index==16);
    list.open(6);list.move(10,17);assert(list.index==16);
    list.move(-10,17);assert(list.index==6);
    list.open(0);list.move(0,1);assert(list.index==0 && !list.navigated);
    list.move(1,1);assert(list.index==0 && list.navigated);
    list.move(1,0);assert(list.index==-1 && !list.navigated);
    list.open(3);list.move(1,9);
    const auto stripSize=wheel::page_size(1);
    assert(list.index/static_cast<int>(stripSize)==1 && list.index%static_cast<int>(stripSize)==0);
    list.move(-1,9);assert(list.index==3 && list.index/static_cast<int>(stripSize)==0);
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
    for(const auto slot:{AmmoSlot::bolt1,AmmoSlot::bolt2}){
        const auto emptyItems=wheel::eligible(bolts,empty_ammo_id,600,slot,true);
        assert(emptyItems.size()==2 && emptyItems[0].id==500 && emptyItems[1].id==800);
    }
    auto boltItems=wheel::eligible(bolts,500,600,AmmoSlot::bolt1);
    assert(boltItems.size()==2 && boltItems[0].id==500 && boltItems[1].id==800);
    boltItems=wheel::eligible(bolts,500,600,AmmoSlot::bolt1,true);
    assert(boltItems.size()==1 && boltItems[0].id==500);
    boltItems=wheel::eligible(bolts,800,500,AmmoSlot::bolt1,true);
    assert(boltItems.size()==1 && boltItems[0].id==800);
    assert(wheel::damage_affinity(100,100,175,100,0).badge==L'\u2668');
    assert(wheel::damage_affinity(100,100,100,100,0).badge==L'\u2191');
}
