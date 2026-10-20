#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include "icon_archive.hpp"
#include "../third_party/miniz/miniz_tinfl.h"
#include <array>
#include <fstream>

namespace arrow_switch::item_icons {
namespace {
constexpr std::size_t maxAsset=512ull*1024*1024;
// Data0's public RSA modulus and exponent are archive format constants.
constexpr char modulus[]=
    "F518EEDB086BB970D5419A5FCA55443DE3719BB5E0307703C9A691508A57404D"
    "2A128FBA637E8B3F4C6916F2F41F490AC47C29B884AC760AE730727FCA3E12EB"
    "5316CE13BCCB80506F0FF325E8610D244279C149985073DC8D890B91428E82BE"
    "F36C5DF713395D775FB1D0734624A086E2072F8AA4D9667FD1FFE72B957281E5"
    "979FFA0B79804B8D7D52F30492012CDAEB82578EDD1B3FF8BA9A957648B66A29"
    "1C68EF1D9B210AD1D721CD7A4485DA306164881C6ED301C630B2C27FBBA7DE78"
    "C2371F8F1079556699E2119B2A18B56E160F71C86DE179C685C51358EDCD9895"
    "AC97071C8D0E24B2EB4BAB7B918CB30CD087939103BA3EFF3F47B50D16D7425B";
struct Crypto {
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_KEY_HANDLE key{};
    ~Crypto(){if(key)BCryptDestroyKey(key);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);}
};
bool disk(const std::filesystem::path& path,Bytes& bytes,std::size_t limit=maxAsset) {
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input)return false;
    const auto length=input.tellg();if(length<=0 || static_cast<std::uint64_t>(length)>limit)return false;
    bytes.resize(static_cast<std::size_t>(length));input.seekg(0);input.read(reinterpret_cast<char*>(bytes.data()),length);
    return input.good();
}
bool decrypt_ranges(View header,std::size_t keyOffset,Bytes& bytes) {
    if(!keyOffset)return true;
    // The archive supplies the AES key and its encrypted byte ranges per file.
    const auto ranges=value<std::int32_t>(header,keyOffset+16);
    if(ranges<0 || ranges>4096 || keyOffset>header.size() || 20ull+static_cast<std::size_t>(ranges)*16>header.size()-keyOffset)return false;
    Crypto aes;
    if(BCryptOpenAlgorithmProvider(&aes.algorithm,BCRYPT_AES_ALGORITHM,nullptr,0)!=0 ||
       BCryptSetProperty(aes.algorithm,BCRYPT_CHAINING_MODE,reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_ECB)),sizeof(BCRYPT_CHAIN_MODE_ECB),0)!=0 ||
       BCryptGenerateSymmetricKey(aes.algorithm,&aes.key,nullptr,0,const_cast<PUCHAR>(header.data()+keyOffset),16,0)!=0)return false;
    for(int i=0;i<ranges;++i){
        const auto start=value<std::int64_t>(header,keyOffset+20+static_cast<std::size_t>(i)*16);
        const auto end=value<std::int64_t>(header,keyOffset+28+static_cast<std::size_t>(i)*16);
        if(start==-1 || end==-1 || start==end)continue;
        if(start<0 || end<start || static_cast<std::uint64_t>(end)>bytes.size() || (end-start)%16)return false;
        ULONG actual{};const auto count=static_cast<ULONG>(end-start);
        if(BCryptDecrypt(aes.key,bytes.data()+start,count,nullptr,nullptr,0,bytes.data()+start,count,&actual,0)!=0 || actual!=count)return false;
    }
    return true;
}
}
bool Archive::index() {
    if(!header_.empty())return true;
    Bytes raw;if(!disk(gameRoot_/L"Data0.bhd",raw,8*1024*1024))return false;
    if(magic(raw,0,"BHD5")){header_=std::move(raw);return true;}
    if(raw.size()%256)return false;
    Crypto rsa;
    if(BCryptOpenAlgorithmProvider(&rsa.algorithm,BCRYPT_RSA_ALGORITHM,nullptr,0)!=0)return false;
    BCRYPT_RSAKEY_BLOB layout{};layout.Magic=BCRYPT_RSAPUBLIC_MAGIC;layout.BitLength=2048;layout.cbPublicExp=4;layout.cbModulus=256;
    Bytes blob(sizeof(layout)+260);std::memcpy(blob.data(),&layout,sizeof(layout));
    const std::uint8_t exponent[]{0x47,0x31,0xFC,0x21};std::memcpy(blob.data()+sizeof(layout),exponent,4);
    const auto nibble=[](char c){return c<='9'?c-'0':c-'A'+10;};
    for(unsigned i=0;i<256;++i)blob[sizeof(layout)+4+i]=static_cast<std::uint8_t>((nibble(modulus[2*i])<<4)|nibble(modulus[2*i+1]));
    if(BCryptImportKeyPair(rsa.algorithm,nullptr,BCRYPT_RSAPUBLIC_BLOB,&rsa.key,blob.data(),static_cast<ULONG>(blob.size()),0)!=0)return false;
    Bytes decoded;decoded.reserve(raw.size()/256*255);
    for(std::size_t i=0;i<raw.size();i+=256){
        std::array<std::uint8_t,256> block{};ULONG actual{};
        if(BCryptEncrypt(rsa.key,raw.data()+i,256,nullptr,nullptr,0,block.data(),256,&actual,BCRYPT_PAD_NONE)!=0 || actual!=256)return false;
        decoded.insert(decoded.end(),block.begin()+1,block.end());
    }
    if(!magic(decoded,0,"BHD5"))return false;
    header_=std::move(decoded);return true;
}
bool Archive::packed(const std::string& relative,Bytes& bytes) {
    if(!index())return false;
    const auto buckets=value<std::uint32_t>(header_,16),table=value<std::uint32_t>(header_,20);
    if(!buckets || buckets>100000 || table>header_.size() || static_cast<std::size_t>(buckets)*8>header_.size()-table)return false;
    const auto wanted=path_hash(relative);
    // A BHD bucket is selected by the path hash. Check the index rather than
    // scanning or decrypting the game data archive.
    const auto bucket=table+static_cast<std::size_t>(wanted%buckets)*8;
    const auto count=value<std::uint32_t>(header_,bucket),offset=value<std::uint32_t>(header_,bucket+4);
    if(count>100000 || offset>header_.size() || static_cast<std::size_t>(count)*40>header_.size()-offset)return false;
    for(unsigned i=0;i<count;++i){
        const auto entry=offset+static_cast<std::size_t>(i)*40;
        if(value<std::uint64_t>(header_,entry)!=wanted)continue;
        const auto padded=value<std::uint32_t>(header_,entry+8),unpadded=value<std::uint32_t>(header_,entry+12);
        const auto fileOffset=value<std::uint64_t>(header_,entry+16),aes=value<std::uint64_t>(header_,entry+32);
        if(!padded || padded>maxAsset)return false;
        std::ifstream data(gameRoot_/L"Data0.bdt",std::ios::binary|std::ios::ate);if(!data)return false;
        const auto length=data.tellg();if(length<0 || fileOffset>static_cast<std::uint64_t>(length) || padded>static_cast<std::uint64_t>(length)-fileOffset)return false;
        bytes.resize(padded);data.seekg(static_cast<std::streamoff>(fileOffset));data.read(reinterpret_cast<char*>(bytes.data()),padded);
        if(!data.good() || !decrypt_ranges(header_,static_cast<std::size_t>(aes),bytes))return false;
        if(unpadded && unpadded<bytes.size())bytes.resize(unpadded);
        return true;
    }
    return false;
}
bool Archive::unpack(Bytes& bytes) {
    if(!magic(bytes,0,"DCX\0"))return !bytes.empty();
    if(bytes.size()<76)return false;
    const auto size=big32(bytes,28),compressed=big32(bytes,32);
    if(!size || size>maxAsset || compressed>bytes.size()-76)return false;
    Bytes decoded(size);bool ok=false;
    if(magic(bytes,40,"DFLT")){
        ok=tinfl_decompress_mem_to_mem(decoded.data(),decoded.size(),bytes.data()+76,compressed,TINFL_FLAG_PARSE_ZLIB_HEADER)==size;
    }else if(magic(bytes,40,"KRAK")){
        using Decompress=long long(__stdcall*)(const void*,long long,void*,long long,int,int,int,void*,long long,void*,void*,void*,long long,int);
        const auto library=LoadLibraryExW((gameRoot_/L"oo2core_6_win64.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(library){
            const auto decode=reinterpret_cast<Decompress>(GetProcAddress(library,"OodleLZ_Decompress"));
            if(decode)ok=decode(bytes.data()+76,compressed,decoded.data(),size,1,0,0,nullptr,0,nullptr,nullptr,nullptr,0,3)==size;
            FreeLibrary(library);
        }
    }
    if(ok)bytes=std::move(decoded);return ok;
}
bool Archive::read(const std::string& relative,Bytes& bytes) {
    bytes.clear();
    try{
        if(relative!="menu/low/01_common.tpf.dcx" && relative!="menu/low/01_common.sblytbnd.dcx" &&
           relative!="menu/hi/01_common.tpf.dcx" && relative!="menu/hi/01_common.sblytbnd.dcx")return false;
        const auto path=std::filesystem::path(relative);
        // Explicit mod root first, then ME3's filesystem redirection, loose
        // game assets, and finally the packed vanilla archive.
        if((!overrideRoot_.empty() && disk(overrideRoot_/path,bytes)) || disk(path,bytes) || disk(gameRoot_/path,bytes) || packed(relative,bytes))
            return unpack(bytes);
    }catch(const std::exception&){bytes.clear();}
    return false;
}
}
