#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../src/icon_archive.hpp"
#include <cassert>
#include <fstream>
#include <cstdio>
int main(){
    using namespace arrow_switch::item_icons;
    const auto root=std::filesystem::current_path()/(L"icon-archive-test-"+std::to_wstring(GetCurrentProcessId()));
    assert(!std::filesystem::exists(root));
    const auto file=root/L"menu/low/01_common.tpf.dcx";
    std::filesystem::create_directories(file.parent_path());
    const auto write=[&](const Bytes& bytes){std::ofstream out(file,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());assert(out.good());};
    Archive archive(root,root);Bytes output{99};
    Bytes malformed(80,0);std::memcpy(malformed.data(),"DCX\0",4);malformed[31]=4;malformed[35]=4;
    std::memcpy(malformed.data()+40,"NOPE",4);write(malformed);
    assert(!archive.read("menu/low/01_common.tpf.dcx",output) && output.empty());
    std::memcpy(malformed.data()+40,"DFLT",4);write(malformed);
    assert(!archive.read("menu/low/01_common.tpf.dcx",output) && output.empty());
    const Bytes compressed{120,156,11,9,112,99,0,0,2,208,0,235};
    malformed.resize(76+compressed.size());malformed[35]=static_cast<std::uint8_t>(compressed.size());
    std::copy(compressed.begin(),compressed.end(),malformed.begin()+76);write(malformed);
    assert(archive.read("menu/low/01_common.tpf.dcx",output) && output==Bytes({'T','P','F',0}));
    std::filesystem::remove(file);std::filesystem::remove(file.parent_path());
    std::filesystem::remove(root/L"menu");std::filesystem::remove(root);
    std::puts("Icon archive: unknown codec and corrupt compression clear output; valid compressed data loads.");
}
