#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include "item_icons.hpp"
#include "icon_archive.hpp"
#include <array>
#include <atomic>
#include <mutex>
#include <unordered_set>

namespace arrow_switch::item_icons {
using Microsoft::WRL::ComPtr;
namespace {
// DDS blocks stay compressed until sampled by a private WARP device. This
// supports BC7 without adding a decoder or using the game's graphics context.
class Decoder {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11Buffer> constants;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11RasterizerState> raster;
    ComPtr<ID3D11Texture2D> target,readback;
    ComPtr<ID3D11RenderTargetView> rtv;
public:
    bool create() {
        if(device)return true;
        const D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_11_0;
        if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,&level,1,D3D11_SDK_VERSION,&device,nullptr,&context)))return false;
        constexpr char source[]=R"(
cbuffer Crop : register(b0) {float4 rect;};
Texture2D atlas : register(t0);SamplerState filter : register(s0);
struct V {float4 position:SV_POSITION;float2 uv:TEXCOORD0;};
V vs(uint id:SV_VertexID){V v;float2 p=float2((id==1)?2:0,(id==2)?2:0);v.position=float4(p.x*2-1,1-p.y*2,0,1);v.uv=rect.xy+p*rect.zw;return v;}
float4 ps(V v):SV_TARGET{return atlas.Sample(filter,v.uv);}
)";
        ComPtr<ID3DBlob> v,p;
        if(FAILED(D3DCompile(source,sizeof(source),nullptr,nullptr,nullptr,"vs","vs_5_0",0,0,&v,nullptr)) ||
           FAILED(D3DCompile(source,sizeof(source),nullptr,nullptr,nullptr,"ps","ps_5_0",0,0,&p,nullptr)) ||
           FAILED(device->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,&vs)) ||
           FAILED(device->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,&ps)))return false;
        D3D11_TEXTURE2D_DESC t{};t.Width=t.Height=128;t.ArraySize=t.MipLevels=1;t.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        t.SampleDesc.Count=1;t.BindFlags=D3D11_BIND_RENDER_TARGET;
        if(FAILED(device->CreateTexture2D(&t,nullptr,&target)) || FAILED(device->CreateRenderTargetView(target.Get(),nullptr,&rtv)))return false;
        t.BindFlags=0;t.Usage=D3D11_USAGE_STAGING;t.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        if(FAILED(device->CreateTexture2D(&t,nullptr,&readback)))return false;
        D3D11_BUFFER_DESC b{};b.ByteWidth=16;b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        if(FAILED(device->CreateBuffer(&b,nullptr,&constants)))return false;
        D3D11_SAMPLER_DESC f{};f.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        f.AddressU=f.AddressV=f.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;f.MaxLOD=D3D11_FLOAT32_MAX;
        D3D11_RASTERIZER_DESC r{};r.FillMode=D3D11_FILL_SOLID;r.CullMode=D3D11_CULL_NONE;r.DepthClipEnable=TRUE;
        return SUCCEEDED(device->CreateSamplerState(&f,&sampler)) && SUCCEEDED(device->CreateRasterizerState(&r,&raster));
    }
    ComPtr<ID3D11ShaderResourceView> texture(View dds,unsigned& width,unsigned& height) {
        ComPtr<ID3D11ShaderResourceView> result;
        if(!create() || !magic(dds,0,"DDS ") || dds.size()<128)return result;
        width=value<unsigned>(dds,16);height=value<unsigned>(dds,12);
        if(!width||!height||width>16384||height>16384)return result;
        unsigned data=128,block=16;DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;
        const auto four=value<unsigned>(dds,84);
        if(four==0x30315844){
            if(dds.size()<148 || value<unsigned>(dds,132)!=3 || value<unsigned>(dds,140)!=1)return result;
            data=148;format=static_cast<DXGI_FORMAT>(value<unsigned>(dds,128));
        }else if(four==0x31545844)format=DXGI_FORMAT_BC1_UNORM;
        else if(four==0x33545844)format=DXGI_FORMAT_BC2_UNORM;
        else if(four==0x35545844)format=DXGI_FORMAT_BC3_UNORM;
        if(format==DXGI_FORMAT_BC1_UNORM_SRGB || format==DXGI_FORMAT_BC1_TYPELESS)format=DXGI_FORMAT_BC1_UNORM;
        if(format==DXGI_FORMAT_BC2_UNORM_SRGB || format==DXGI_FORMAT_BC2_TYPELESS)format=DXGI_FORMAT_BC2_UNORM;
        if(format==DXGI_FORMAT_BC3_UNORM_SRGB || format==DXGI_FORMAT_BC3_TYPELESS)format=DXGI_FORMAT_BC3_UNORM;
        if(format==DXGI_FORMAT_BC7_UNORM_SRGB || format==DXGI_FORMAT_BC7_TYPELESS)format=DXGI_FORMAT_BC7_UNORM;
        if(format==DXGI_FORMAT_BC1_UNORM)block=8;
        else if(format!=DXGI_FORMAT_BC2_UNORM && format!=DXGI_FORMAT_BC3_UNORM && format!=DXGI_FORMAT_BC7_UNORM)return result;
        const unsigned pitch=((width+3)/4)*block,length=pitch*((height+3)/4);
        if(data>dds.size()||length>dds.size()-data)return result;
        D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=1;
        desc.Format=format;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA pixels{dds.data()+data,pitch,length};ComPtr<ID3D11Texture2D> texture;
        if(SUCCEEDED(device->CreateTexture2D(&desc,&pixels,&texture)))device->CreateShaderResourceView(texture.Get(),nullptr,&result);
        return result;
    }
    std::shared_ptr<const Image> crop(ID3D11ShaderResourceView* texture,unsigned width,unsigned height,const IconRect& rect) {
        if(!texture || rect.x>width || rect.y>height || rect.width>width-rect.x || rect.height>height-rect.y)return {};
        // Preserve the source aspect ratio, centering it in a transparent square.
        const float aspect=static_cast<float>(rect.width)/rect.height;
        const float w=aspect>=1?128.f:128.f*aspect,h=aspect>=1?128.f/aspect:128.f;
        const float uv[]{rect.x/static_cast<float>(width),rect.y/static_cast<float>(height),
            rect.width/static_cast<float>(width),rect.height/static_cast<float>(height)};
        context->UpdateSubresource(constants.Get(),0,nullptr,uv,0,0);
        ID3D11RenderTargetView* renderTarget=rtv.Get();context->OMSetRenderTargets(1,&renderTarget,nullptr);
        const float clear[4]{};context->ClearRenderTargetView(rtv.Get(),clear);
        D3D11_VIEWPORT viewport{(128-w)*.5f,(128-h)*.5f,w,h,0,1};context->RSSetViewports(1,&viewport);context->RSSetState(raster.Get());
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);
        ID3D11Buffer* buffer=constants.Get();context->VSSetConstantBuffers(0,1,&buffer);
        context->PSSetShaderResources(0,1,&texture);ID3D11SamplerState* sample=sampler.Get();context->PSSetSamplers(0,1,&sample);
        context->Draw(3,0);context->CopyResource(readback.Get(),target.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};if(FAILED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped)))return {};
        auto image=std::make_shared<Image>();image->rgba.resize(128*128*4);
        for(unsigned y=0;y<128;++y)std::memcpy(image->rgba.data()+y*128*4,static_cast<const std::uint8_t*>(mapped.pData)+y*mapped.RowPitch,128*4);
        context->Unmap(readback.Get(),0);return image;
    }
};
class Extractor {
    Archive archive;
    Decoder decoder;
    Logger log;
    struct Source {std::string quality;bool tried=false;IconLayout layout;Bytes tpf;
        std::unordered_map<std::string,ComPtr<ID3D11ShaderResourceView>> textures;
        std::unordered_map<std::string,std::pair<unsigned,unsigned>> sizes;};
    std::array<Source,2> sources{{{"low",false,{},{},{},{}},{"hi",false,{},{},{},{}}}};
public:
    Extractor(const std::filesystem::path& game,const std::filesystem::path& overrides,Logger logger):archive(game,overrides),log(logger){}
    std::shared_ptr<const Image> get(unsigned id) {
        for(auto& source:sources){
            const std::string prefix="menu/"+source.quality+"/01_common";
            if(!source.tried){
                source.tried=true;Bytes layout;
                if(archive.read(prefix+".sblytbnd.dcx",layout))source.layout=parse_layout(layout);
                if(log)log("Item icons: %s layout contains %zu entries.",source.quality.c_str(),source.layout.size());
            }
            const auto found=source.layout.find(id);if(found==source.layout.end())continue;
            const auto& rect=found->second;
            if(!source.textures.contains(rect.atlas)){
                if(source.tpf.empty() && !archive.read(prefix+".tpf.dcx",source.tpf))continue;
                const auto dds=tpf_texture(source.tpf,rect.atlas);unsigned width{},height{};
                auto texture=decoder.texture(dds,width,height);
                source.sizes[rect.atlas]={width,height};source.textures[rect.atlas]=std::move(texture);
            }
            const auto [width,height]=source.sizes[rect.atlas];
            if(auto image=decoder.crop(source.textures[rect.atlas].Get(),width,height,rect))return image;
        }
        return {};
    }
};
struct State {
    std::mutex mutex;
    std::filesystem::path game,overrideRoot;
    std::filesystem::path automaticRoot;
    std::unordered_map<unsigned,std::shared_ptr<const Image>> cache;
    std::vector<unsigned> pending;
    Logger logger{};bool running=false,ready=false;
    unsigned generation=0;
    std::atomic<std::uint64_t> revision{0};
} state;
DWORD WINAPI worker(void*) {
    for(;;){
        std::vector<unsigned> batch;std::filesystem::path game,overrideRoot;Logger log{};unsigned generation{};
        {std::lock_guard<std::mutex> guard(state.mutex);
            if(state.pending.empty()){state.running=false;return 0;}
            batch.swap(state.pending);game=state.game;overrideRoot=state.overrideRoot;log=state.logger;generation=state.generation;
        }
        unsigned loaded=0;
        try{
            Extractor extractor(game,overrideRoot,log);
            for(const auto id:batch){
                const auto image=extractor.get(id);
                std::lock_guard<std::mutex> guard(state.mutex);
                if(generation!=state.generation)break;
                if(image){state.cache[id]=image;++state.revision;++loaded;}
            }
        }catch(const std::exception& e){if(log)log("Item icon extraction stopped: %s",e.what());}
        if(log)log("Item icons: extracted %u of %zu requested icons (missing icons use effect badges).",loaded,batch.size());
    }
}
}
void initialize(const std::filesystem::path& gameDirectory,Logger logger) {
    HMODULE module{};GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&worker),&module);
    std::lock_guard<std::mutex> guard(state.mutex);
    state.game=gameDirectory;state.logger=logger;state.ready=true;
    // Known supported overhaul loaders expose their mod root through a loaded
    // native module. Probe only the four known icon paths in nearby parents.
    for(const auto name:{L"reforged.dll",L"NightreignMovement.Convergence.dll",L"ErdTools.dll"}){
        wchar_t filename[32768]{};const auto loaded=GetModuleHandleW(name);
        if(!loaded || !GetModuleFileNameW(loaded,filename,32768))continue;
        auto parent=std::filesystem::path(filename).parent_path();
        for(unsigned depth=0;depth<4 && !parent.empty();++depth,parent=parent.parent_path()){
            for(const auto& root:{parent,parent/L"mod"}){
                std::error_code error;
                if(std::filesystem::is_regular_file(root/L"menu/low/01_common.tpf.dcx",error) ||
                   std::filesystem::is_regular_file(root/L"menu/hi/01_common.tpf.dcx",error)){
                    state.automaticRoot=root;state.overrideRoot=root;
                    if(logger)logger("Item icons: detected mod asset root %ls.",root.c_str());return;
                }
            }
        }
    }
}
void set_override(const std::filesystem::path& directory) {
    std::lock_guard<std::mutex> guard(state.mutex);
    const auto root=directory.empty()?state.automaticRoot:directory;
    if(state.overrideRoot==root)return;
    state.overrideRoot=root;state.cache.clear();state.pending.clear();++state.generation;++state.revision;
}
std::shared_ptr<const Image> request(unsigned id) {
    if(!id || id==65535)return {};
    std::lock_guard<std::mutex> guard(state.mutex);
    if(!state.ready)return {};
    if(const auto it=state.cache.find(id);it!=state.cache.end())return it->second;
    if(state.cache.size()>=256)return {};
    state.cache[id]={};state.pending.push_back(id);
    if(!state.running){
        // Keep the module resident while the short-lived extraction worker runs.
        HMODULE module{};GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&worker),&module);
        const auto thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);
        if(thread){state.running=true;CloseHandle(thread);}else{state.pending.clear();state.cache.erase(id);}
    }
    return {};
}
void prefetch(const std::vector<unsigned>& ids) {
    // Reserve the whole visible page before starting extraction, avoiding a
    // separate archive decompression for each first-frame icon request.
    {
        std::lock_guard<std::mutex> guard(state.mutex);
        if(!state.ready)return;
        for(const auto id:ids)if(id && id!=65535 && !state.cache.contains(id) && state.cache.size()<256){
            state.cache[id]={};state.pending.push_back(id);
        }
        if(state.running || state.pending.empty())return;
        const auto thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);
        if(thread){state.running=true;CloseHandle(thread);}else state.pending.clear();
    }
}
std::uint64_t revision(){return state.revision.load();}
std::shared_ptr<const Image> extract(const std::filesystem::path& gameDirectory,
    const std::filesystem::path& overrideDirectory,unsigned id,Logger log) {
    try{Extractor extractor(gameDirectory,overrideDirectory,log);return extractor.get(id);}
    catch(const std::exception& e){if(log)log("Item icon preview: %s",e.what());return {};}
}
}
