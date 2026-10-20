#pragma once
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace arrow_switch::item_icons {
using Bytes=std::vector<std::uint8_t>;
using View=std::span<const std::uint8_t>;
template<class T> T value(View bytes,std::size_t at) {
    if(at>bytes.size() || sizeof(T)>bytes.size()-at)throw std::runtime_error("truncated icon asset");
    T result{};std::memcpy(&result,bytes.data()+at,sizeof(T));return result;
}
inline bool magic(View bytes,std::size_t at,const char* word) {
    return at<=bytes.size() && 4<=bytes.size()-at && std::memcmp(bytes.data()+at,word,4)==0;
}
inline unsigned big32(View bytes,std::size_t at) {
    const auto v=value<std::uint32_t>(bytes,at);
    return (v>>24)|((v>>8)&0xFF00)|((v<<8)&0xFF0000)|(v<<24);
}
inline std::string basename(std::string_view path) {
    const auto slash=path.find_last_of("/\\");if(slash!=path.npos)path.remove_prefix(slash+1);
    const auto dot=path.find_last_of('.');if(dot!=path.npos)path=path.substr(0,dot);
    return std::string(path);
}
struct IconRect { std::string atlas;unsigned x{},y{},width{},height{}; };
using IconLayout=std::unordered_map<unsigned,IconRect>;
inline std::string_view attribute(std::string_view tag,std::string_view key) {
    const auto needle=std::string(key)+"=\"";
    const auto start=tag.find(needle);if(start==tag.npos)return {};
    tag.remove_prefix(start+needle.size());const auto end=tag.find('"');
    return end==tag.npos?std::string_view{}:tag.substr(0,end);
}
inline unsigned integer(std::string_view text) {
    unsigned n=0;const auto r=std::from_chars(text.data(),text.data()+text.size(),n);
    return r.ec==std::errc{} && r.ptr==text.data()+text.size()?n:0;
}
inline IconLayout parse_layout(View bytes) {
    IconLayout result;
    const std::string_view text(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    std::size_t at=0;
    while((at=text.find("<TextureAtlas ",at))!=text.npos){
        const auto head=text.find('>',at),end=text.find("</TextureAtlas>",at);
        if(head==text.npos||end==text.npos)break;
        const auto atlas=basename(attribute(text.substr(at,head-at),"imagePath"));
        std::size_t sub=head;
        while((sub=text.find("<SubTexture ",sub))!=text.npos && sub<end){
            const auto close=text.find('>',sub);if(close==text.npos||close>end)break;
            const auto tag=text.substr(sub,close-sub);
            auto name=attribute(tag,"name");
            constexpr std::string_view prefix="MENU_ItemIcon_";
            if(name.starts_with(prefix)){
                name.remove_prefix(prefix.size());const auto dot=name.find('.');
                if(dot!=name.npos){
                    const auto id=integer(name.substr(0,dot));
                    IconRect rect{atlas,integer(attribute(tag,"x")),integer(attribute(tag,"y")),
                        integer(attribute(tag,"width")),integer(attribute(tag,"height"))};
                    if(id && rect.width && rect.height && rect.width<=4096 && rect.height<=4096 && !atlas.empty())result.try_emplace(id,std::move(rect));
                }
            }
            sub=close+1;
        }
        at=end+15;
    }
    return result;
}
inline View tpf_texture(View bytes,const std::string& wanted) {
    if(!magic(bytes,0,"TPF\0") || bytes.size()<16 || bytes[12]!=0)return {};
    const auto count=value<std::uint32_t>(bytes,8);
    if(count>(bytes.size()-16)/20)return {};
    for(unsigned i=0;i<count;++i){
        const std::size_t entry=16+static_cast<std::size_t>(i)*20;
        const auto offset=value<std::uint32_t>(bytes,entry),size=value<std::uint32_t>(bytes,entry+4);
        const auto nameOffset=value<std::uint32_t>(bytes,entry+12);
        if(offset>bytes.size()||size>bytes.size()-offset||nameOffset>=bytes.size())continue;
        std::string name;const unsigned stride=bytes[14]==1?2:1;
        for(std::size_t p=nameOffset;p<bytes.size() && name.size()<256;p+=stride){
            if(bytes[p]==0 && (stride==1 || (p+1<bytes.size() && bytes[p+1]==0)))break;
            name.push_back(static_cast<char>(bytes[p]));
        }
        if(basename(name)==wanted)return bytes.subspan(offset,size);
    }
    return {};
}
inline std::uint64_t path_hash(std::string_view path) {
    std::uint64_t hash=0;if(path.empty() || path.front()!='/')hash='/';
    for(auto c:path){if(c=='\\')c='/';if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');hash=hash*0x85+static_cast<unsigned char>(c);}
    return hash;
}
}
