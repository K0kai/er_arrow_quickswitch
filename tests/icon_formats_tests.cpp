#include "../src/icon_formats.hpp"
#include <cassert>
int main(){
    using namespace arrow_switch::item_icons;
    const std::string xml=R"(<TextureAtlas imagePath="atlas.dds"><SubTexture name="MENU_ItemIcon_06000.png" x="32" y="64" width="128" height="128"/><SubTexture name="MENU_ItemIcon_06001d.png" x="0" y="0" width="128" height="128"/><SubTexture name="MENU_ItemIcon_06002.png" x="0" y="0" width="0" height="128"/></TextureAtlas>)";
    const auto layout=parse_layout(View(reinterpret_cast<const std::uint8_t*>(xml.data()),xml.size()));
    assert(layout.size()==1 && layout.at(6000).x==32 && layout.at(6000).atlas=="atlas");
    const std::string menuXml=R"(<TextureAtlas imagePath="menu.dds"><SubTexture name="MENU_SlotBase.png" x="1" y="2" width="314" height="318"/><SubTexture name="MENU_FL_Cursor_Wide.png" x="3" y="4" width="594" height="52"/></TextureAtlas>)";
    const auto menu=parse_layout(View(reinterpret_cast<const std::uint8_t*>(menuXml.data()),menuXml.size()));
    assert(menu.size()==2 && menu.at(0xF0000001u).atlas=="menu" && menu.at(0xF0000002u).width==594);
    assert(path_hash("menu/hi/01_common.tpf.dcx")==path_hash("/MENU\\HI\\01_common.tpf.dcx"));
    Bytes truncated(4,0);assert(tpf_texture(truncated,"atlas").empty());
    bool caught=false;try{value<std::uint64_t>(truncated,0);}catch(const std::exception&){caught=true;}assert(caught);
    assert(parse_layout(truncated).empty());
    Bytes tpf(96,0);std::memcpy(tpf.data(),"TPF\0",4);
    const auto put=[&](std::size_t offset,std::uint32_t n){std::memcpy(tpf.data()+offset,&n,4);};
    put(8,1);put(16,80);put(20,4);put(28,48);std::memcpy(tpf.data()+48,"atlas.dds",10);std::memcpy(tpf.data()+80,"DDS ",4);
    assert(tpf_texture(tpf,"atlas").size()==4);put(20,0xFFFFFFFF);assert(tpf_texture(tpf,"atlas").empty());
    // An exact-looking name without a terminator must not select a texture.
    Bytes missingNameTerminator(49,0);std::memcpy(missingNameTerminator.data(),"TPF\0",4);
    const auto putIn=[](Bytes& target,std::size_t offset,std::uint32_t n){std::memcpy(target.data()+offset,&n,4);};
    putIn(missingNameTerminator,8,1);putIn(missingNameTerminator,16,36);putIn(missingNameTerminator,20,4);
    putIn(missingNameTerminator,28,40);std::memcpy(missingNameTerminator.data()+36,"DDS ",4);
    std::memcpy(missingNameTerminator.data()+40,"atlas.dds",9);
    assert(tpf_texture(missingNameTerminator,"atlas").empty());
    missingNameTerminator.push_back(0);assert(tpf_texture(missingNameTerminator,"atlas").size()==4);
    Bytes wide=missingNameTerminator;wide.resize(60);wide[14]=1;
    for(unsigned i=0;i<9;++i){wide[40+i*2]=static_cast<std::uint8_t>("atlas.dds"[i]);wide[41+i*2]=0;}
    wide[58]=wide[59]=0;assert(tpf_texture(wide,"atlas").size()==4);
    wide.resize(58);assert(tpf_texture(wide,"atlas").empty());
    wide.resize(59);assert(tpf_texture(wide,"atlas").empty());

    Bytes dcx(80,0);std::memcpy(dcx.data(),"DCX\0",4);std::memcpy(dcx.data()+40,"DFLT",4);
    const auto be=[&](std::size_t at,std::uint32_t n){for(unsigned i=0;i<4;++i)dcx[at+i]=static_cast<std::uint8_t>(n>>(24-i*8));};
    be(28,128);be(32,4);DcxPayload payload;
    assert(dcx_payload(dcx,payload) && payload.decoded==128 && payload.compressed==4 && payload.codec==DcxCodec::deflate);
    std::memcpy(dcx.data()+40,"KRAK",4);assert(dcx_payload(dcx,payload) && payload.codec==DcxCodec::kraken);
    std::memcpy(dcx.data()+40,"NOPE",4);assert(!dcx_payload(dcx,payload));
    std::memcpy(dcx.data()+40,"DFLT",4);be(28,0);assert(!dcx_payload(dcx,payload));
    be(28,0xFFFFFFFF);assert(!dcx_payload(dcx,payload));
    be(28,128);be(32,0);assert(!dcx_payload(dcx,payload));
    be(32,5);assert(!dcx_payload(dcx,payload));
    dcx.resize(75);assert(!dcx_payload(dcx,payload));

}
