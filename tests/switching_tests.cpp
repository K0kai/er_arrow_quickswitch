#include "../src/game_access.hpp"
#include "../src/request.hpp"
#include <cassert>
#include <cstdio>

int main() {
    using namespace arrow_switch;
    const std::vector<Arrow> arrows={{10,1,10,0,81},{20,2,8,1,81},{30,3,1,2,83},{40,4,9,3,81}};
    assert(next(arrows,10,1,20)->id==40); // secondary slot excluded
    assert(next(arrows,10,-1,20)->id==40); // backwards wraps
    assert(next(arrows,40,-1,20)->id==10); // backwards excludes incompatible family
    assert(next(arrows,20,1,40)->id==10);
    assert(next({{10,1,1,0,81},{20,2,1,1,81}},10,1,20)==nullptr);

    Request q;
    q.submit(1,1000);
    assert(!q.ready(1000,true,true,150,3000,200));
    assert(!q.ready(1200,true,false,150,3000,200));
    assert(!q.ready(1349,true,false,150,3000,200));
    assert(q.ready(1350,true,false,150,3000,200));
    assert(q.take(1350)==1);
    q.submit(-1,1400);
    assert(!q.ready(1400,true,false,150,3000,200));
    assert(q.ready(1550,true,false,150,3000,200));
    assert(q.take(1550)==-1);
    q.submit(1,1600);
    assert(!q.ready(1700,false,false,150,3000,200)); // menu/focus cancels
    assert(!q.ready(2000,true,false,150,3000,200)); // cannot leak into gameplay
    q.submit(1,3000);
    assert(!q.ready(6000,true,false,150,3000,200)); // expires
    assert(q.direction==0);

    game::Snapshot s;
    s.arrows=arrows; s.first=10; s.handle=1; s.index=384; s.tail=384;
    assert(game::current_matches(s));
    s.handle=2; assert(!game::current_matches(s));
    s.handle=1; s.index=385; assert(!game::current_matches(s));
    s.index=384; s.tail=0xFFFFFFFFu; assert(!game::current_matches(s));

    // Exercise revalidation with real readable memory and mutations between selection/call.
    std::array<std::uint8_t,0x700> player{};
    std::array<std::uint8_t,0x80> inventory{};
    std::array<game::Entry,4> entries{};
    const auto put=[](auto& buffer,std::size_t offset,const auto& value) {
        std::memcpy(buffer.data()+offset,&value,sizeof(value));
    };
    const auto playerPtr=reinterpret_cast<std::uintptr_t>(player.data());
    const auto invPtr=reinterpret_cast<std::uintptr_t>(inventory.data());
    const auto listPtr=reinterpret_cast<std::uintptr_t>(entries.data());
    std::array<std::uintptr_t,2> manager={0,playerPtr};
    auto managerPtr=reinterpret_cast<std::uintptr_t>(manager.data());
    game::Addresses a; a.gdm=reinterpret_cast<std::uintptr_t>(&managerPtr);
    s.manager=managerPtr; s.player=playerPtr; s.inventory=invPtr; s.list=listPtr;
    s.first=10; s.second=20; s.handle=1; s.index=384; s.tail=384;
    put(player,0x5D0,invPtr); put(player,0x3B0,s.first); put(player,0x3B8,s.second);
    put(player,0x358,s.handle); put(player,0x2D0,s.index);
    put(inventory,0x10,listPtr); put(inventory,0x1C,s.tail);
    entries[3]={4,40,9,0,0,0};
    assert(game::revalidate(a,s,arrows[3]));
    entries[3].count=0; assert(!game::revalidate(a,s,arrows[3]));
    entries[3].count=9; entries[3].handle=9; assert(!game::revalidate(a,s,arrows[3]));
    entries[3].handle=4; put(player,0x3B8,std::uint32_t(40)); assert(!game::revalidate(a,s,arrows[3]));
    put(player,0x3B8,s.second); manager[1]=0; assert(!game::revalidate(a,s,arrows[3]));

    // All four ammo slots use distinct native indices and equipment fields.
    manager[1]=playerPtr;
    const std::vector<Arrow> ammo={{10,1,10,0,81},{20,2,8,1,81},{30,3,1,2,81},
        {40,4,9,3,81},{50,5,7,4,85},{60,6,7,5,85},{70,7,6,6,85},{80,8,5,7,85},
        {90,9,5,8,83},{100,10,5,9,86}};
    std::array<game::Entry,10> ammoEntries{};
    for(const auto& item:ammo)ammoEntries[item.index]={item.handle,item.id,item.count,0,0,0};
    s.list=reinterpret_cast<std::uintptr_t>(ammoEntries.data());
    put(inventory,0x10,s.list);
    s.tail=100;put(inventory,0x1C,s.tail);
    s.arrows=ammo;
    s.slotIds={10,20,50,60};s.slotHandles={1,2,5,6};s.slotIndices={100,101,104,105};
    for(std::size_t i=0;i<4;++i){
        const auto slot=static_cast<AmmoSlot>(i);
        put(player,id_offset(slot),s.slotIds[i]);
        put(player,handle_offset(slot),s.slotHandles[i]);
        put(player,index_offset(slot),s.slotIndices[i]);
    }
    for(std::size_t i=0;i<4;++i){
        const auto slot=static_cast<AmmoSlot>(i);
        constexpr int expectedNative[]{6,8,7,9};
        assert(native_slot(slot)==expectedNative[i]);
        assert(game::current_matches_slot(s,slot));
        const auto target=i<2?ammo[3]:ammo[6];
        assert(game::revalidate_slot(a,s,target,slot));
        const auto otherFamily=i<2?ammo[8]:ammo[9];
        assert(game::revalidate_slot(a,s,otherFamily,slot));
        const auto wrong=i<2?ammo[6]:ammo[3];
        assert(!game::revalidate_slot(a,s,wrong,slot));
        const auto forbidden=ammo[i<2?(i^1u):(i^1u)+2u];
        assert(!game::revalidate_slot(a,s,forbidden,slot));
        put(player,id_offset(slot),std::uint32_t(0));
        assert(!game::revalidate_slot(a,s,target,slot));
        put(player,id_offset(slot),s.slotIds[i]);
    }
    // Empty slots accept either family but still require stable slot fields,
    // a live owned candidate, and an unchanged paired slot.
    for(std::size_t i=0;i<4;++i){
        const auto slot=static_cast<AmmoSlot>(i);
        const auto oldId=s.slotIds[i],oldHandle=s.slotHandles[i],oldIndex=s.slotIndices[i];
        s.slotIds[i]=s.slotHandles[i]=s.slotIndices[i]=0xFFFFFFFFu;
        put(player,id_offset(slot),s.slotIds[i]);
        put(player,handle_offset(slot),s.slotHandles[i]);
        put(player,index_offset(slot),s.slotIndices[i]);
        assert(game::empty_slot(s,slot) && game::current_matches_slot(s,slot));
        const auto target=i<2?ammo[8]:ammo[9];
        assert(game::revalidate_slot(a,s,target,slot));
        ammoEntries[target.index].count=0;
        assert(!game::revalidate_slot(a,s,target,slot));
        ammoEntries[target.index].count=target.count;
        put(player,index_offset(slot),oldIndex);
        assert(!game::revalidate_slot(a,s,target,slot));
        s.slotIndices[i]=oldIndex;
        assert(!game::current_matches_slot(s,slot));
        s.slotIds[i]=oldId;s.slotHandles[i]=oldHandle;
        put(player,id_offset(slot),oldId);put(player,handle_offset(slot),oldHandle);
        assert(game::current_matches_slot(s,slot));
    }
    // Reforged may retain a stale equipped handle in the two secondary slots,
    // while the ID and index still identify the correct live inventory entry.
    for(const auto slot:{AmmoSlot::arrow2,AmmoSlot::bolt2}){
        const auto i=slot_index(slot);
        const auto original=s.slotHandles[i];
        s.slotHandles[i]=original+1000;
        put(player,handle_offset(slot),s.slotHandles[i]);
        assert(game::current_matches_slot(s,slot));
        assert(game::revalidate_slot(a,s,i==1?ammo[8]:ammo[9],slot));
        put(player,index_offset(slot),s.slotIndices[i]+1);
        assert(!game::revalidate_slot(a,s,i==1?ammo[8]:ammo[9],slot));
        put(player,index_offset(slot),s.slotIndices[i]);
        s.slotHandles[i]=original;
        put(player,handle_offset(slot),original);
    }
    s.slotHandles[0]+=1000;
    assert(!game::current_matches_slot(s,AmmoSlot::arrow1));
    s.slotHandles[0]-=1000;

    // A valid HUD object and living player are both required. Menus, death and
    // mismatched HUD ownership must reject a request before the native call.
    std::array<std::uint8_t,0x4E80> fe{};
    std::array<std::uint8_t,0x1E510> world{};
    std::array<std::uint8_t,0x198> character{};
    std::array<std::uint8_t,0x140> stats{};
    auto statsPtr=reinterpret_cast<std::uintptr_t>(stats.data());
    auto bag=statsPtr;
    auto worldPtr=reinterpret_cast<std::uintptr_t>(world.data());
    put(character,0x190,reinterpret_cast<std::uintptr_t>(&bag));
    put(world,0x1E508,reinterpret_cast<std::uintptr_t>(character.data()));
    put(fe,0,game::base()+0x2AA0A08); put(fe,0x78,std::uint8_t(3));
    put(stats,0x138,std::int32_t(100));
    a.world=reinterpret_cast<std::uintptr_t>(&worldPtr);
    void* hud=fe.data()+0x4E60;
    assert(game::normal_gameplay(a,hud));
    put(fe,0x78,std::uint8_t(6)); assert(!game::normal_gameplay(a,hud));
    put(fe,0x78,std::uint8_t(3)); put(stats,0x138,std::int32_t(0)); assert(!game::normal_gameplay(a,hud));
    put(stats,0x138,std::int32_t(100)); put(fe,0,std::uintptr_t(0)); assert(!game::normal_gameplay(a,hud));
    std::puts("Switch policy, input deferral/cancellation, inventory mapping, and stale-request checks passed.");
}
