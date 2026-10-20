#include "../src/arrow_names.hpp"
#include <cassert>
#include <cstdio>

int main() {
    using namespace arrow_switch;
    std::array<std::uint8_t,0x400> fmg{},patched{},repo{};
    std::array<std::uint64_t,2> offsets{0x100,0x180};
    const auto put=[](auto& buffer,std::size_t offset,const auto& value) {
        std::memcpy(buffer.data()+offset,&value,sizeof(value));
    };
    put(fmg,0xC,std::uint32_t(2));
    put(fmg,0x18,reinterpret_cast<std::uintptr_t>(offsets.data()));
    const std::uint32_t firstGroup[4]={0,50000000,50000000,0},secondGroup[4]={1,90000001,90000001,0};
    put(fmg,0x28,firstGroup); put(fmg,0x38,secondGroup);
    const wchar_t vanilla[]=L"Arrow",custom[]=L"Moonlight \u9727\u306e\u77e2";
    put(fmg,0x100,vanilla); put(fmg,0x180,custom);
    const auto fmgPtr=reinterpret_cast<std::uintptr_t>(fmg.data());
    assert(fmg_name(fmgPtr,50000000)==vanilla);
    assert(fmg_name(fmgPtr,90000001)==custom);
    assert(fmg_name(fmgPtr,50000001).empty());
    offsets[1]=0; assert(fmg_name(fmgPtr,90000001).empty()); offsets[1]=0x180;
    std::array<std::uintptr_t,512> table{};
    std::array<std::uintptr_t,1> versions{reinterpret_cast<std::uintptr_t>(table.data())};
    put(repo,8,reinterpret_cast<std::uintptr_t>(versions.data()));
    put(repo,0x10,std::uint32_t(1)); put(repo,0x14,std::uint32_t(512));
    auto repository=reinterpret_cast<std::uintptr_t>(repo.data());
    table[11]=fmgPtr;
    assert(arrow_name(reinterpret_cast<std::uintptr_t>(&repository),90000001)==custom);
    patched=fmg;
    const wchar_t modded[]=L"Custom Flame Arrow";
    put(patched,0x180,modded);
    table[115]=reinterpret_cast<std::uintptr_t>(patched.data());
    assert(arrow_name(reinterpret_cast<std::uintptr_t>(&repository),90000001)==modded);
    table[115]=0; table[11]=0; table[310]=fmgPtr;
    assert(arrow_name(reinterpret_cast<std::uintptr_t>(&repository),90000001)==custom);
    table[310]=0; table[410]=fmgPtr;
    assert(arrow_name(reinterpret_cast<std::uintptr_t>(&repository),90000001)==custom);
    table[410]=0;
    assert(arrow_name(reinterpret_cast<std::uintptr_t>(&repository),90000001).empty());
    assert(arrow_name(0,90000001).empty());
    std::puts("Runtime text: vanilla, arbitrary mod IDs, Unicode, patch priority, DLC fallbacks and missing data passed.");
}
