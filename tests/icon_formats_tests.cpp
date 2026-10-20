#include "../src/icon_formats.hpp"
#include <cassert>
int main(){
    using namespace arrow_switch::item_icons;
    const std::string xml=R"(<TextureAtlas imagePath="atlas.dds"><SubTexture name="MENU_ItemIcon_06000.png" x="32" y="64" width="128" height="128"/><SubTexture name="MENU_ItemIcon_06001d.png" x="0" y="0" width="128" height="128"/><SubTexture name="MENU_ItemIcon_06002.png" x="0" y="0" width="0" height="128"/></TextureAtlas>)";
    const auto layout=parse_layout(View(reinterpret_cast<const std::uint8_t*>(xml.data()),xml.size()));
    assert(layout.size()==1 && layout.at(6000).x==32 && layout.at(6000).atlas=="atlas");
    assert(path_hash("menu/hi/01_common.tpf.dcx")==path_hash("/MENU\\HI\\01_common.tpf.dcx"));
    Bytes truncated(4,0);assert(tpf_texture(truncated,"atlas").empty());
    bool caught=false;try{value<std::uint64_t>(truncated,0);}catch(const std::exception&){caught=true;}assert(caught);
    assert(parse_layout(truncated).empty());
    Bytes tpf(96,0);std::memcpy(tpf.data(),"TPF\0",4);
    const auto put=[&](std::size_t offset,std::uint32_t n){std::memcpy(tpf.data()+offset,&n,4);};
    put(8,1);put(16,80);put(20,4);put(28,48);std::memcpy(tpf.data()+48,"atlas.dds",10);std::memcpy(tpf.data()+80,"DDS ",4);
    assert(tpf_texture(tpf,"atlas").size()==4);put(20,0xFFFFFFFF);assert(tpf_texture(tpf,"atlas").empty());
}
