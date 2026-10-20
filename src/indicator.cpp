#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "indicator.hpp"
#include "ammo_effects.hpp"
#include "ui_layout.hpp"
#include "item_icons.hpp"
#include "settings_ui.hpp"
#include "theme_config.hpp"
#include "text_layout.hpp"
#include <MinHook.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <vector>

namespace arrow_switch::indicator {
using Microsoft::WRL::ComPtr;
namespace {
constexpr UINT cardWidth=420,cardHeight=88,imageWidth=640,imageHeight=768;
constexpr UINT indicatorRow=656,wheelCanvas=640;
Settings settings;
Settings pendingSettings;
bool settingsPending=false;
bool previewBackdrop=false;
Logger logger{};
HMODULE owner{};
std::atomic<bool> gameplay{};
std::atomic<ULONGLONG> gameplayUpdated{};
struct Status {
    std::wstring name;
    std::wstring slotLabel=L"ARROW SLOT 1";
    std::wstring indicatorLabel=L"ARROW SLOT 1";
    std::uint32_t id=0xFFFFFFFFu,count{};
    std::uint64_t changed{},serial{};
    std::vector<WheelItem> wheel;
    int selected=-1;
    bool wheelOpen=false;
} status;
std::mutex statusMutex,gpuMutex;
std::atomic<ID3D12CommandQueue*> capturedQueue{};

using Present=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT);
using Resize=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT,UINT,DXGI_FORMAT,UINT);
using Resize1=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*,UINT,UINT,UINT,DXGI_FORMAT,UINT,const UINT*,IUnknown* const*);
using Execute=void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*,UINT,ID3D12CommandList* const*);
Present originalPresent{};
Resize originalResize{};
Resize1 originalResize1{};
Execute originalExecute{};

struct Frame {
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12Resource> backbuffer;
    ComPtr<ID3D12Resource> wheelVertices;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
    UINT64 fence{};
};
struct Renderer {
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12DescriptorHeap> rtvHeap,srvHeap;
    ComPtr<ID3D12GraphicsCommandList> commands;
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> pipeline;
    ComPtr<ID3D12PipelineState> wheelPipeline;
    ComPtr<ID3D12Resource> texture,upload;
    ComPtr<ID3D12Fence> fence;
    std::vector<Frame> frames;
    IDXGISwapChain* chain{}; // Identity only; never dereferenced outside a callback.
    HANDLE event{};
    UINT64 lastFence{},textureSerial{},iconRevision{};
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    bool uploaded=false,failed=false;

    void wait() {
        if (!fence || !lastFence || fence->GetCompletedValue()>=lastFence) return;
        if (SUCCEEDED(fence->SetEventOnCompletion(lastFence,event))) WaitForSingleObject(event,INFINITE);
    }
    void clear() {
        wait();settings_ui::shutdown();frames.clear(); commands.Reset(); texture.Reset(); upload.Reset();
        pipeline.Reset(); wheelPipeline.Reset(); root.Reset(); srvHeap.Reset(); rtvHeap.Reset(); fence.Reset();
        device.Reset(); queue.Reset(); chain=nullptr; lastFence=0; textureSerial=0; uploaded=false;
        if (event) { CloseHandle(event); event=nullptr; }
    }
} renderer;

struct Pixels { std::vector<std::uint8_t> bytes; };
struct WheelVertex { float x,y,r,g,b,a; };
std::vector<WheelVertex> wheel_geometry(const Status& current);
void blend_pixel(Pixels& image,int x,int y,unsigned color,float alpha) {
    if(x<0 || y<0 || x>=static_cast<int>(imageWidth) || y>=static_cast<int>(imageHeight))return;
    const auto p=(static_cast<std::size_t>(y)*imageWidth+x)*4;
    for(unsigned c=0;c<3;++c){
        const auto value=static_cast<float>((color>>(16-8*c))&255);
        image.bytes[p+c]=static_cast<std::uint8_t>(value*alpha+image.bytes[p+c]*(1-alpha));
    }
    image.bytes[p+3]=static_cast<std::uint8_t>(alpha*255+image.bytes[p+3]*(1-alpha));
}
// GDI supplies a grayscale glyph mask; tint and shadow are composed explicitly
// into premultiplied RGBA. Text opacity is independent of the wheel surfaces.
struct TextCanvas {
    HDC dc=CreateCompatibleDC(nullptr);
    HBITMAP bitmap{};
    HGDIOBJ oldBitmap{};
    void* memory{};
    TextCanvas() {
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=imageWidth;info.bmiHeader.biHeight=-static_cast<LONG>(imageHeight);
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        if(dc)bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&memory,nullptr,0);
        if(bitmap){oldBitmap=SelectObject(dc,bitmap);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));}
    }
    ~TextCanvas(){if(oldBitmap)SelectObject(dc,oldBitmap);if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);}
    void draw(Pixels& image,const std::wstring& text,RECT rect,unsigned size,const std::wstring& family,
              unsigned color,UINT flags=DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX) {
        if(!memory || text.empty())return;
        if(flags&DT_WORDBREAK){size=fit_wrapped_text(dc,text,rect,size,family);flags|=DT_EDITCONTROL;}
        rect.left=std::clamp(rect.left,0L,static_cast<LONG>(imageWidth));
        rect.right=std::clamp(rect.right,rect.left,static_cast<LONG>(imageWidth));
        rect.top=std::clamp(rect.top,0L,static_cast<LONG>(imageHeight));
        rect.bottom=std::clamp(rect.bottom,rect.top,static_cast<LONG>(imageHeight));
        auto* mask=static_cast<std::uint8_t*>(memory);
        for(LONG y=rect.top;y<rect.bottom;++y)
            std::memset(mask+(static_cast<std::size_t>(y)*imageWidth+rect.left)*4,0,(rect.right-rect.left)*4);
        HFONT font=CreateFontW(-static_cast<int>(size),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,family.c_str());
        const auto previous=SelectObject(dc,font?font:GetStockObject(DEFAULT_GUI_FONT));
        DrawTextW(dc,text.c_str(),-1,&rect,flags);GdiFlush();
        const auto ink=[&](LONG x,LONG y){
            if(x<rect.left||x>=rect.right||y<rect.top||y>=rect.bottom)return 0.f;
            const auto p=(static_cast<std::size_t>(y)*imageWidth+x)*4;
            return std::max({mask[p],mask[p+1],mask[p+2]})/255.f;
        };
        if(settings.textShadow)for(LONG y=rect.top-1;y<=rect.bottom+1;++y)for(LONG x=rect.left-1;x<=rect.right+1;++x){
            const float shadow=std::max({ink(x-1,y-1),ink(x,y-1),ink(x+1,y-1),ink(x-1,y),ink(x+1,y),ink(x,y+1)});
            if(shadow>0)blend_pixel(image,x,y,0x070907,shadow*.90f);
        }
        for(LONG y=rect.top;y<rect.bottom;++y)for(LONG x=rect.left;x<rect.right;++x){
            const auto coverage=ink(x,y);if(coverage>0)blend_pixel(image,x,y,color,coverage);
        }
        SelectObject(dc,previous);if(font)DeleteObject(font);
    }
};

// The PNG preview uses the same colored triangles as Direct3D, including
// selection borders and gradients, rather than a separate approximation.
void raster_geometry(Pixels& image,const Status& current) {
    const auto vertices=wheel_geometry(current);
    const auto edge=[](const WheelVertex& a,const WheelVertex& b,float x,float y){return (x-a.x)*(b.y-a.y)-(y-a.y)*(b.x-a.x);};
    for(std::size_t i=0;i+2<vertices.size();i+=3){
        auto a=vertices[i],b=vertices[i+1],c=vertices[i+2];
        float area=edge(a,b,c.x,c.y);if(std::abs(area)<.0001f)continue;
        if(area<0){std::swap(b,c);area=-area;}
        const int left=std::max(0,static_cast<int>(std::floor(std::min({a.x,b.x,c.x}))));
        const int right=std::min(static_cast<int>(imageWidth)-1,static_cast<int>(std::ceil(std::max({a.x,b.x,c.x}))));
        const int top=std::max(0,static_cast<int>(std::floor(std::min({a.y,b.y,c.y}))));
        const int bottom=std::min(static_cast<int>(imageHeight)-1,static_cast<int>(std::ceil(std::max({a.y,b.y,c.y}))));
        const auto inside=[](float e,const WheelVertex& from,const WheelVertex& to){
            return e>0 || (e==0 && (to.y>from.y || (to.y==from.y && to.x<from.x)));
        };
        for(int y=top;y<=bottom;++y)for(int x=left;x<=right;++x){
            const auto u=edge(b,c,x+.5f,y+.5f),v=edge(c,a,x+.5f,y+.5f),w=edge(a,b,x+.5f,y+.5f);
            if(!inside(u,b,c)||!inside(v,c,a)||!inside(w,a,b))continue;
            const float alpha=(u*a.a+v*b.a+w*c.a)/area;
            const float colors[]{(u*a.r+v*b.r+w*c.r)/area,(u*a.g+v*b.g+w*c.g)/area,(u*a.b+v*b.b+w*c.b)/area};
            const auto p=(static_cast<std::size_t>(y)*imageWidth+x)*4;
            for(unsigned k=0;k<3;++k)image.bytes[p+k]=static_cast<std::uint8_t>(std::clamp(colors[k]*255+image.bytes[p+k]*(1-alpha),0.f,255.f));
            image.bytes[p+3]=static_cast<std::uint8_t>(alpha*255+image.bytes[p+3]*(1-alpha));
        }
    }
}
std::array<std::uint8_t,64*64*4> selectionImage{};
bool selectionImageLoaded=false;
void configure_icon_source() {
    auto path=std::filesystem::path(settings.iconSourceDirectory);
    if(!path.empty() && path.is_relative()){
        wchar_t filename[32768]{};
        if(owner && GetModuleFileNameW(owner,filename,32768))path=std::filesystem::path(filename).parent_path()/path;
    }
    item_icons::set_override(path);
}
void load_selection_image() {
    selectionImageLoaded=false;
    if(settings.selectionImage.empty() || !owner)return;
    wchar_t modulePath[32768]{};
    if(!GetModuleFileNameW(owner,modulePath,32768))return;
    auto filename=std::filesystem::path(settings.selectionImage);
    if(filename.is_relative())filename=std::filesystem::path(modulePath).parent_path()/filename;
    const auto init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    UINT width{},height{};
    selectionImageLoaded=SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))) &&
        SUCCEEDED(factory->CreateDecoderFromFilename(filename.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder)) &&
        SUCCEEDED(decoder->GetFrame(0,&frame)) && SUCCEEDED(frame->GetSize(&width,&height)) &&
        width==64 && height==64 && SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
        SUCCEEDED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)) &&
        SUCCEEDED(converter->CopyPixels(nullptr,64*4,static_cast<UINT>(selectionImage.size()),selectionImage.data()));
    if(logger)logger("Wheel selection image loaded=%u: %ls",selectionImageLoaded?1u:0u,filename.c_str());
    if(SUCCEEDED(init))CoUninitialize();
}
const std::vector<std::uint8_t>& game_icons() {
    static std::vector<std::uint8_t> pixels;
    static std::once_flag once;
    std::call_once(once,[] {
        wchar_t path[32768]{};
        if(!owner || !GetModuleFileNameW(owner,path,32768)) return;
        const auto filename=(std::filesystem::path(path).parent_path()/L".."/L"assets"/L"wheel-game-icons.rgba").wstring();
        auto file=CreateFileW(filename.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE) {if(logger)logger("Wheel game icon sheet unavailable: %ls.",filename.c_str());return;}
        constexpr DWORD length=768*64*4;
        if(GetFileSize(file,nullptr)==length) {
            pixels.resize(length);
            DWORD actual{};
            if(!ReadFile(file,pixels.data(),length,&actual,nullptr) || actual!=length)pixels.clear();
        }
        CloseHandle(file);
        if(logger)logger("Wheel game icon sheet loaded=%u.",pixels.empty()?0u:1u);
    });
    return pixels;
}
int icon_index(const std::wstring& effect) {
    constexpr const wchar_t* names[]={L"MAGIC",L"FIRE",L"LIGHTNING",L"HOLY",L"ROT",L"POISON",L"BLEED",L"SLEEP",L"FROST",L"ARROW",L"BOLT",L"MADNESS",L"DEATH BLIGHT"};
    for(int i=0;i<13;++i) if(effect==names[i]) return i==10?9:i>=11?i-1:i;
    return -1;
}
Pixels raster_card(const std::wstring& name,std::uint32_t count,const std::wstring& slotLabel) {
    Pixels result;result.bytes.resize(imageWidth*imageHeight*4);
    if(!settings.showIndicator || name.empty())return result;
    TextCanvas text;
    const LONG y=indicatorRow;
    text.draw(result,name,{4,y+4,416,y+43},settings.indicatorNameSize,settings.indicatorFont,settings.indicatorTextColor);
    text.draw(result,slotLabel+L"   \u00d7 "+std::to_wstring(count),{4,y+45,416,y+71},
        settings.indicatorDetailSize,settings.detailFont,settings.indicatorDetailColor);
    return result;
}

Pixels raster_wheel(const Status& current,bool background=true) {
    Pixels result;result.bytes.resize(imageWidth*imageHeight*4);
    if(current.wheel.empty())return result;
    constexpr float center=320.f,outer=285.f;
    constexpr double pi=3.14159265358979323846;
    const float inner=settings.cancelRadius*outer/240.f;
    const auto n=current.wheel.size();
    if(background)raster_geometry(result,current);
    // The replaceable marker points away from the cancel zone.
    if(settings.showMarker && current.selected>=0 && static_cast<std::size_t>(current.selected)<n){
        const double angle=2*pi*current.selected/n;
        const float sx=static_cast<float>(std::sin(angle)),cy=static_cast<float>(-std::cos(angle));
        const float scale=settings.markerScale/100.f;
        const int half=static_cast<int>(16*scale);
        for(int v=-half;v<half;++v)for(int u=-half;u<half;++u){
            const float localU=u/scale,localV=v/scale;
            const float radial=inner+18.f*scale-v*.7f;
            const int x=static_cast<int>(std::round(center+radial*sx-u*cy));
            const int y=static_cast<int>(std::round(center+radial*cy+u*sx));
            if(selectionImageLoaded){
                const int ix=std::clamp(static_cast<int>((localU+16)*2),0,63);
                const int iy=std::clamp(static_cast<int>((localV+16)*2),0,63);
                const auto source=(static_cast<std::size_t>(iy)*64+ix)*4;
                const unsigned color=(selectionImage[source]<<16)|(selectionImage[source+1]<<8)|selectionImage[source+2];
                blend_pixel(result,x,y,color,selectionImage[source+3]/255.f);
            }else if(localV>=-14 && localV<=13 && std::abs(localU)<=(localV<0?11+localV*.75f:localV<5?4.f:2.f)){
                blend_pixel(result,x,y,settings.selectionColor,1.f);
            }
        }
    }
    const auto& atlas=game_icons();
    if(settings.showIcons && settings.showItemIcons){
        std::vector<unsigned> ids;for(const auto& item:current.wheel)ids.push_back(item.iconId);
        item_icons::prefetch(ids);
    }
    TextCanvas text;
    for(std::size_t i=0;i<n;++i){
        const auto& item=current.wheel[i];
        const bool selected=static_cast<int>(i)==current.selected;
        const double angle=2*pi*i/n;
        const int cx=static_cast<int>(center+194*std::sin(angle));
        const int cy=static_cast<int>(center-194*std::cos(angle));
        const int halfWidth=std::clamp(static_cast<int>(pi*194*.78/n),42,78);
        const auto nameColor=selected?settings.selectedTextColor:settings.textColor;
        if(settings.showIcons && settings.iconBackdrop){
            const float size=40.f*settings.itemIconScale/100.f;
            const int radius=static_cast<int>(size);
            for(int y=-radius;y<=radius;++y)for(int x=-radius;x<=radius;++x){
                const float distance=std::sqrt(static_cast<float>(x*x+y*y))/size;
                if(distance>=1)continue;
                blend_pixel(result,cx+x,cy-39+y,0x080B09,(1-distance)*settings.iconBackdrop/100.f);
                if(selected)blend_pixel(result,cx+x,cy-39+y,settings.selectionColor,(1-distance)*settings.selectionGlow/500.f);
            }
        }
        const auto artwork=settings.showIcons && settings.showItemIcons?item_icons::request(item.iconId):nullptr;
        if(artwork){
            const int size=static_cast<int>(64*settings.itemIconScale/100);
            for(int y=0;y<size;++y)for(int x=0;x<size;++x){
                const auto source=(static_cast<std::size_t>(y*artwork->height/size)*artwork->width+x*artwork->width/size)*4;
                const auto& rgba=artwork->rgba;
                const unsigned color=(rgba[source]<<16)|(rgba[source+1]<<8)|rgba[source+2];
                blend_pixel(result,cx-size/2+x,cy-39-size/2+y,color,rgba[source+3]/255.f);
            }
        }
        const bool icons=settings.showEffectIcons && atlas.size()==768*64*4 && (item.iconsMask || icon_index(item.effect)>=0);
        if(settings.showIcons && icons){
            std::array<int,12> tiles{};int count=0;
            if(item.iconsMask){
                constexpr int order[]{0,1,2,3,4,5,6,7,8,10,11,9};
                for(const auto tile:order)if(item.iconsMask&(1u<<tile))tiles[count++]=tile;
            }else{const int tile=icon_index(item.effect);if(tile>=0)tiles[count++]=tile;}
            const int nominal=artwork?16:count==1?46:std::min(40,std::max(24,static_cast<int>((2*pi*194/n-14)/std::max(1,count))));
            const int size=std::max(16,static_cast<int>(nominal*settings.iconScale/100));
            const int gap=3,start=cx+(artwork?22:0)-(count*size+(count-1)*gap)/2;
            for(int part=0;part<count;++part)for(int y=0;y<size;++y)for(int x=0;x<size;++x){
                const int px=start+part*(size+gap)+x,py=cy-(artwork?20:38)-size/2+y;
                const int ax=tiles[part]*64+x*64/size,ay=y*64/size;
                const auto source=(static_cast<std::size_t>(ay)*768+ax)*4;
                const unsigned color=(atlas[source]<<16)|(atlas[source+1]<<8)|atlas[source+2];
                blend_pixel(result,px,py,color,atlas[source+3]/255.f);
            }
        }else if(settings.showIcons && !artwork){
            text.draw(result,std::wstring(1,item.badge),{cx-24,cy-62,cx+24,cy-16},27,L"Segoe UI Symbol",item.color);
        }
        if(settings.showEffectLabels)
            text.draw(result,item.effect,{cx-halfWidth,cy-15,cx+halfWidth,cy+1},settings.effectSize,settings.detailFont,settings.mutedColor);
        const int nameTop=settings.showEffectLabels?2:-8;
        text.draw(result,item.name,{cx-halfWidth,cy+nameTop,cx+halfWidth,cy+44},settings.nameSize,settings.fontFamily,nameColor,
            DT_CENTER|DT_WORDBREAK|DT_END_ELLIPSIS|DT_NOPREFIX);
        if(settings.showCounts)
            text.draw(result,L"\u00d7 "+std::to_wstring(item.count),{cx-halfWidth,cy+45,cx+halfWidth,cy+67},
                settings.countSize,settings.detailFont,selected?settings.selectedTextColor:settings.countColor);
    }
    const bool selected=current.selected>=0&&static_cast<std::size_t>(current.selected)<n;
    const LONG centerHalf=static_cast<LONG>(std::clamp(inner*.83f,55.f,137.f));
    text.draw(result,selected?current.wheel[current.selected].name:L"Cancel",
        {320-centerHalf,278,320+centerHalf,330},settings.centerSize,settings.fontFamily,
        selected?settings.selectedTextColor:settings.textColor,DT_CENTER|DT_WORDBREAK|DT_END_ELLIPSIS|DT_NOPREFIX);
    const bool paged=current.slotLabel.find(L'/')!=std::wstring::npos;
    text.draw(result,selected?current.wheel[current.selected].effect:current.slotLabel,
        {320-centerHalf,345,320+centerHalf,369},settings.effectSize,settings.detailFont,settings.mutedColor);
    if(paged && selected)text.draw(result,current.slotLabel,{320-centerHalf,371,320+centerHalf,393},
        settings.effectSize,settings.detailFont,settings.mutedColor);
    const auto card=raster_card(current.name,current.count,current.indicatorLabel);
    std::copy(card.bytes.begin()+indicatorRow*imageWidth*4,card.bytes.end(),result.bytes.begin()+indicatorRow*imageWidth*4);
    return result;
}

bool png(const wchar_t* filename,const std::uint8_t* pixels,UINT width,UINT height,UINT stride) {
    const auto co=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapEncoder> encoder;
    ComPtr<IWICBitmapFrameEncode> frame;
    bool ok=SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))) &&
        SUCCEEDED(factory->CreateStream(&stream)) && SUCCEEDED(stream->InitializeFromFilename(filename,GENERIC_WRITE)) &&
        SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder)) &&
        SUCCEEDED(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache)) &&
        SUCCEEDED(encoder->CreateNewFrame(&frame,nullptr)) && SUCCEEDED(frame->Initialize(nullptr)) &&
        SUCCEEDED(frame->SetSize(width,height));
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;
    std::vector<std::uint8_t> bgra(pixels,pixels+stride*height);
    for (UINT y=0;y<height;++y) for (UINT x=0;x<width;++x) std::swap(bgra[y*stride+x*4],bgra[y*stride+x*4+2]);
    if (ok) ok=SUCCEEDED(frame->SetPixelFormat(&format)) && format==GUID_WICPixelFormat32bppBGRA &&
        SUCCEEDED(frame->WritePixels(height,stride,stride*height,bgra.data())) &&
        SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
    frame.Reset(); encoder.Reset(); stream.Reset(); factory.Reset();
    if (SUCCEEDED(co)) CoUninitialize();
    return ok;
}

constexpr char shader[]=R"(
cbuffer Layout : register(b0) { float4 rect; float opacity; float hdr; float2 uvScale; float2 uvOffset; float brightness; float paperWhite; };
Texture2D image : register(t0); SamplerState linearClamp : register(s0);
struct Vertex { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
Vertex vs(uint id : SV_VertexID) {
    const float2 points[6]={float2(0,0),float2(1,0),float2(0,1),float2(0,1),float2(1,0),float2(1,1)};
    Vertex v; v.uv=points[id]; v.position=float4(rect.xy+v.uv*rect.zw,0,1); return v;
}
float3 pq(float3 n) {
    float3 p=pow(max(n,0),0.1593017578);
    return pow((0.8359375+18.8515625*p)/(1+18.6875*p),78.84375);
}
float4 ps(Vertex v) : SV_TARGET {
    float4 c=image.Sample(linearClamp,v.uv*uvScale+uvOffset);
    c.rgb*=brightness;
    if(hdr>0.5 && c.a>0.0001) {
        float3 linearColor=pow(max(c.rgb/c.a,0),2.2);
        c.rgb=(hdr>1.5?linearColor*(paperWhite/80.0):pq(linearColor*(paperWhite/10000.0)))*c.a;
    }
    return c*opacity;
})";

// Wheel surfaces are GPU triangles. Text and badges remain on the texture during migration.
constexpr UINT maxWheelVertices=6144;
constexpr char wheelShader[]=R"(
cbuffer Layout : register(b0) { float4 rect; float opacity; float hdr; float2 uvScale; float2 uvOffset; float brightness; float paperWhite; };
struct Input { float2 xy : POSITION; float4 color : COLOR; };
struct Output { float4 position : SV_POSITION; float4 color : COLOR; };
Output vs(Input v) {
    Output o;
    o.position=float4(rect.xy+(v.xy/float2(640.0,640.0))*rect.zw,0,1);
    o.color=v.color;
    return o;
}
float3 pq(float3 n) {
    float3 p=pow(max(n,0),0.1593017578);
    return pow((0.8359375+18.8515625*p)/(1+18.6875*p),78.84375);
}
float4 ps(Output v) : SV_TARGET {
    float4 c=v.color;
    c.rgb*=brightness;
    if(hdr>0.5 && c.a>0.0001) {
        float3 linearColor=pow(max(c.rgb/c.a,0),2.2);
        c.rgb=(hdr>1.5?linearColor*(paperWhite/80.0):pq(linearColor*(paperWhite/10000.0)))*c.a;
    }
    return c*opacity;
}
)";
std::vector<WheelVertex> wheel_geometry(const Status& current) {
    std::vector<WheelVertex> vertices;
    const auto count=current.wheel.size();if(!count)return vertices;
    constexpr float center=320.f,outer=285.f;
    constexpr double pi=3.14159265358979323846;
    const float inner=settings.cancelRadius*outer/240.f,border=settings.borderScale/100.f;
    const float pulse=settings.animateSelection && !previewBackdrop?.88f+.12f*static_cast<float>(std::sin(static_cast<double>(GetTickCount64())*.003)):1.f;
    const float glow=settings.selectionGlow/100.f*pulse;
    const auto vertex=[&](float radius,double angle,unsigned color,float alpha){
        alpha=std::clamp(alpha*settings.wheelOpacity/100.f,0.f,1.f);
        return WheelVertex{center+radius*static_cast<float>(std::sin(angle)),center-radius*static_cast<float>(std::cos(angle)),
            ((color>>16)&255)/255.f*alpha,((color>>8)&255)/255.f*alpha,(color&255)/255.f*alpha,alpha};
    };
    const auto ring=[&](float low,float high,double start,double end,unsigned innerColor,unsigned outerColor,
                        float innerAlpha,float outerAlpha,int segments){
        for(int i=0;i<segments;++i){
            const double a=start+(end-start)*i/segments,b=start+(end-start)*(i+1)/segments;
            const auto li=vertex(low,a,innerColor,innerAlpha),lo=vertex(high,a,outerColor,outerAlpha);
            const auto ri=vertex(low,b,innerColor,innerAlpha),ro=vertex(high,b,outerColor,outerAlpha);
            vertices.insert(vertices.end(),{li,lo,ri,ri,lo,ro});
        }
    };
    // Dark edge falloff separates the translucent wheel from bright scenery.
    ring(outer,outer+8,0,2*pi,0x000000,0x000000,.45f,0.f,64);
    for(std::size_t i=0;i<count;++i){
        const double angle=2*pi*i/count,a=angle-pi/count+.006,b=angle+pi/count-.006;
        const bool selected=static_cast<int>(i)==current.selected;
        ring(inner+1,outer-1,a,b,settings.backgroundColor,selected?settings.selectedFillColor:settings.backgroundColor,
            selected?.80f:.67f,selected?1.f:.87f,20);
        ring(outer-7*border,outer-4*border,a,b,current.wheel[i].color,current.wheel[i].color,
            selected?1.f:.66f,selected?1.f:.66f,20);
        if(selected){
            ring(inner+4,inner+36,a,b,settings.selectionColor,settings.selectionColor,glow*.38f,0.f,20);
            ring(outer-34,outer-8,a,b,settings.selectionColor,settings.selectionColor,0.f,glow*.40f,20);
            ring(inner+2,inner+4*border,a,b,settings.selectionColor,settings.selectionColor,.9f,.9f,20);
            ring(outer-3*border,outer-1,a,b,settings.selectionColor,settings.selectionColor,1.f,1.f,20);
        }
    }
    const bool cancel=current.selected<0;
    ring(0,inner-2,0,2*pi,settings.backgroundColor,cancel?settings.selectedFillColor:settings.backgroundColor,.35f,.65f,64);
    ring(inner-1.5f*border,inner+.5f*border,0,2*pi,cancel?settings.selectionColor:settings.accentColor,
        cancel?settings.selectionColor:settings.accentColor,.95f,.95f,64);
    ring(outer-.5f*border,outer+.5f*border,0,2*pi,settings.accentColor,settings.accentColor,.90f,.90f,64);
    if(settings.decorativeRings){
        ring(outer+4*border,outer+4.7f*border,0,2*pi,settings.accentColor,settings.accentColor,.48f,.48f,64);
        for(std::size_t i=0;i<count;++i){
            const double angle=2*pi*i/count;
            const auto top=vertex(outer+9*border,angle,settings.selectionColor,.9f);
            const auto left=vertex(outer+6*border,angle-.009,settings.accentColor,.75f);
            const auto right=vertex(outer+6*border,angle+.009,settings.accentColor,.75f);
            const auto bottom=vertex(outer+3*border,angle,settings.accentColor,.75f);
            vertices.insert(vertices.end(),{top,left,right,left,bottom,right});
        }
    }
    return vertices;
}

D3D12_RESOURCE_BARRIER transition(ID3D12Resource* resource,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER b{}; b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource=resource; b.Transition.StateBefore=before; b.Transition.StateAfter=after;
    b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; return b;
}

bool initialize_renderer(IDXGISwapChain* swapchain,ID3D12CommandQueue* queue) {
    auto& r=renderer;
    r.clear();
    ComPtr<IDXGISwapChain3> chain;
    DXGI_SWAP_CHAIN_DESC desc{};
    if (!queue || FAILED(swapchain->QueryInterface(IID_PPV_ARGS(&chain))) || FAILED(swapchain->GetDesc(&desc)) ||
        desc.BufferCount<2 || desc.BufferCount>8 || FAILED(swapchain->GetDevice(IID_PPV_ARGS(&r.device)))) return false;
    ComPtr<ID3D12Device> queueDevice;
    if (FAILED(queue->GetDevice(IID_PPV_ARGS(&queueDevice))) || queueDevice.Get()!=r.device.Get()) return false;
    r.queue=queue; r.chain=swapchain;
    D3D12_DESCRIPTOR_HEAP_DESC heap{};
    heap.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV; heap.NumDescriptors=desc.BufferCount;
    if (FAILED(r.device->CreateDescriptorHeap(&heap,IID_PPV_ARGS(&r.rtvHeap)))) return false;
    heap.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; heap.NumDescriptors=2; heap.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(r.device->CreateDescriptorHeap(&heap,IID_PPV_ARGS(&r.srvHeap)))) return false;
    r.frames.resize(desc.BufferCount);
    const auto stride=r.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    auto handle=r.rtvHeap->GetCPUDescriptorHandleForHeapStart();
    auto targetFormat=desc.BufferDesc.Format;
    if (targetFormat==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) targetFormat=DXGI_FORMAT_R8G8B8A8_UNORM;
    if (targetFormat==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) targetFormat=DXGI_FORMAT_B8G8R8A8_UNORM;
    for (UINT i=0;i<desc.BufferCount;++i) {
        auto& f=r.frames[i]; f.rtv=handle; handle.ptr+=stride;
        if (FAILED(chain->GetBuffer(i,IID_PPV_ARGS(&f.backbuffer))) ||
            FAILED(r.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&f.allocator)))) return false;
        D3D12_RESOURCE_DESC vertexBuffer{};
        vertexBuffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
        vertexBuffer.Width=sizeof(WheelVertex)*maxWheelVertices;
        vertexBuffer.Height=1; vertexBuffer.DepthOrArraySize=1; vertexBuffer.MipLevels=1;
        vertexBuffer.SampleDesc.Count=1; vertexBuffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        D3D12_HEAP_PROPERTIES vertexMemory{}; vertexMemory.Type=D3D12_HEAP_TYPE_UPLOAD;
        if(FAILED(r.device->CreateCommittedResource(&vertexMemory,D3D12_HEAP_FLAG_NONE,&vertexBuffer,
            D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&f.wheelVertices))))return false;
        D3D12_RENDER_TARGET_VIEW_DESC view{}; view.Format=targetFormat; view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2D;
        r.device->CreateRenderTargetView(f.backbuffer.Get(),&view,f.rtv);
    }
    D3D12_DESCRIPTOR_RANGE range{}; range.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV; range.NumDescriptors=1;
    D3D12_ROOT_PARAMETER params[2]{};
    params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS; params[0].Constants.Num32BitValues=12;
    params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; params[1].DescriptorTable.NumDescriptorRanges=1;
    params[1].DescriptorTable.pDescriptorRanges=&range; params[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{}; sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS; sampler.MaxLOD=D3D12_FLOAT32_MAX;
    sampler.MaxAnisotropy=1;
    sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC rootDesc{}; rootDesc.NumParameters=2; rootDesc.pParameters=params;
    rootDesc.NumStaticSamplers=1; rootDesc.pStaticSamplers=&sampler;
    rootDesc.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> rootBlob,errors,vertex,pixel,wheelVs,wheelPs;
    if (FAILED(D3D12SerializeRootSignature(&rootDesc,D3D_ROOT_SIGNATURE_VERSION_1,&rootBlob,&errors)) ||
        FAILED(r.device->CreateRootSignature(0,rootBlob->GetBufferPointer(),rootBlob->GetBufferSize(),IID_PPV_ARGS(&r.root))) ||
        FAILED(D3DCompile(shader,sizeof(shader),nullptr,nullptr,nullptr,"vs","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vertex,&errors)) ||
        FAILED(D3DCompile(shader,sizeof(shader),nullptr,nullptr,nullptr,"ps","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&pixel,&errors))) {
        if (logger && errors) logger("Indicator shader/root signature: %s",static_cast<const char*>(errors->GetBufferPointer()));
        return false;
    }
    if(FAILED(D3DCompile(wheelShader,sizeof(wheelShader),nullptr,nullptr,nullptr,"vs","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&wheelVs,&errors)) ||
       FAILED(D3DCompile(wheelShader,sizeof(wheelShader),nullptr,nullptr,nullptr,"ps","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&wheelPs,&errors))) {
        if(logger && errors)logger("Wheel geometry shader: %s",static_cast<const char*>(errors->GetBufferPointer()));
        return false;
    }
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature=r.root.Get(); pso.VS={vertex->GetBufferPointer(),vertex->GetBufferSize()};
    pso.PS={pixel->GetBufferPointer(),pixel->GetBufferSize()};
    pso.BlendState.RenderTarget[0].BlendEnable=TRUE;
    pso.BlendState.RenderTarget[0].SrcBlend=D3D12_BLEND_ONE; pso.BlendState.RenderTarget[0].DestBlend=D3D12_BLEND_INV_SRC_ALPHA;
    pso.BlendState.RenderTarget[0].BlendOp=D3D12_BLEND_OP_ADD;
    pso.BlendState.RenderTarget[0].SrcBlendAlpha=D3D12_BLEND_ONE; pso.BlendState.RenderTarget[0].DestBlendAlpha=D3D12_BLEND_INV_SRC_ALPHA;
    pso.BlendState.RenderTarget[0].BlendOpAlpha=D3D12_BLEND_OP_ADD; pso.BlendState.RenderTarget[0].RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
    pso.SampleMask=UINT_MAX; pso.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID; pso.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
    pso.RasterizerState.DepthClipEnable=TRUE; pso.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;
    pso.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; pso.NumRenderTargets=1;
    pso.RTVFormats[0]=targetFormat; pso.SampleDesc.Count=1;
    if (FAILED(r.device->CreateGraphicsPipelineState(&pso,IID_PPV_ARGS(&r.pipeline))) ||
        FAILED(r.device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,r.frames[0].allocator.Get(),r.pipeline.Get(),IID_PPV_ARGS(&r.commands))) ||
        FAILED(r.commands->Close()) || FAILED(r.device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&r.fence)))) return false;
    D3D12_INPUT_ELEMENT_DESC elements[]={{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
    pso.VS={wheelVs->GetBufferPointer(),wheelVs->GetBufferSize()};
    pso.PS={wheelPs->GetBufferPointer(),wheelPs->GetBufferSize()};
    pso.InputLayout={elements,2};
    const auto wheelPipelineResult=r.device->CreateGraphicsPipelineState(&pso,IID_PPV_ARGS(&r.wheelPipeline));
    if(FAILED(wheelPipelineResult)){
        if(logger)logger("Wheel geometry pipeline creation failed: %08lX",static_cast<unsigned long>(wheelPipelineResult));
        return false;
    }
    r.event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if (!r.event) return false;
    D3D12_RESOURCE_DESC texture{}; texture.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture.Width=imageWidth; texture.Height=imageHeight; texture.DepthOrArraySize=1; texture.MipLevels=1;
    texture.Format=DXGI_FORMAT_R8G8B8A8_UNORM; texture.SampleDesc.Count=1;
    D3D12_HEAP_PROPERTIES memory{}; memory.Type=D3D12_HEAP_TYPE_DEFAULT;
    if (FAILED(r.device->CreateCommittedResource(&memory,D3D12_HEAP_FLAG_NONE,&texture,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&r.texture)))) return false;
    UINT64 uploadSize{};
    r.device->GetCopyableFootprints(&texture,0,1,0,&r.footprint,nullptr,nullptr,&uploadSize);
    D3D12_RESOURCE_DESC buffer{}; buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER; buffer.Width=uploadSize;
    buffer.Height=1; buffer.DepthOrArraySize=1; buffer.MipLevels=1; buffer.SampleDesc.Count=1; buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    memory.Type=D3D12_HEAP_TYPE_UPLOAD;
    if (FAILED(r.device->CreateCommittedResource(&memory,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&r.upload)))) return false;
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{}; srv.Format=texture.Format; srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; srv.Texture2D.MipLevels=1;
    r.device->CreateShaderResourceView(r.texture.Get(),&srv,r.srvHeap->GetCPUDescriptorHandleForHeapStart());
    if (logger) logger("Indicator: D3D12 renderer ready (%u backbuffers, format %u).",desc.BufferCount,static_cast<unsigned>(targetFormat));
    return true;
}

bool render(IDXGISwapChain* swapchain,const Status& current,float opacity) {
    auto& r=renderer;
    if (r.failed) return false;
    if (r.chain!=swapchain || !r.commands) {
        if (!initialize_renderer(swapchain,capturedQueue.load())) { r.clear(); r.failed=true; if (logger) logger("Indicator renderer initialization failed; switching remains enabled."); return false; }
    }
    ComPtr<IDXGISwapChain3> chain;
    if (FAILED(swapchain->QueryInterface(IID_PPV_ARGS(&chain)))) return false;
    auto index=chain->GetCurrentBackBufferIndex();
    if (index>=r.frames.size()) return false;
    auto& f=r.frames[index];
    auto completed=r.fence->GetCompletedValue();
    if (completed==UINT64_MAX) { r.failed=true; return false; }
    const auto iconsRevision=item_icons::revision();
    const bool newTexture=current.serial!=r.textureSerial || iconsRevision!=r.iconRevision || !r.uploaded;
    if (completed<f.fence || (newTexture && completed<r.lastFence)) return false; // Never reset GPU work still in flight.
    if (FAILED(f.allocator->Reset()) || FAILED(r.commands->Reset(f.allocator.Get(),r.pipeline.Get()))) { r.failed=true; return false; }
    if (newTexture) {
        auto image=current.wheelOpen?raster_wheel(current,false):raster_card(current.name,current.count,current.indicatorLabel);
        void* mapped{}; D3D12_RANGE noRead{0,0};
        if (FAILED(r.upload->Map(0,&noRead,&mapped))) { r.commands->Close(); return false; }
        for (UINT row=0;row<imageHeight;++row)
            std::memcpy(static_cast<std::uint8_t*>(mapped)+r.footprint.Offset+row*r.footprint.Footprint.RowPitch,
                image.bytes.data()+row*imageWidth*4,imageWidth*4);
        r.upload->Unmap(0,nullptr);
        if (r.uploaded) { auto b=transition(r.texture.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST); r.commands->ResourceBarrier(1,&b); }
        D3D12_TEXTURE_COPY_LOCATION dst{},src{}; dst.pResource=r.texture.Get(); dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.pResource=r.upload.Get(); src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; src.PlacedFootprint=r.footprint;
        r.commands->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
        auto b=transition(r.texture.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE); r.commands->ResourceBarrier(1,&b);
        r.uploaded=true; r.textureSerial=current.serial;r.iconRevision=iconsRevision;
    }
    const auto desc=f.backbuffer->GetDesc();
    const float width=static_cast<float>(desc.Width),height=static_cast<float>(desc.Height);
    const auto placement=layout(width,height,settings.wheelScale,settings.scale,settings.indicatorGap,
        settings.wheelOffsetX,settings.wheelOffsetY,settings.indicatorOffsetX,settings.indicatorOffsetY);
    const auto& wheel=placement.wheel;
    float constants[12]={wheel.x/width*2-1,1-wheel.y/height*2,wheel.width/width*2,-wheel.height/height*2,1,0,
        1.f,static_cast<float>(wheelCanvas)/imageHeight,0,0,settings.brightness/100.f,static_cast<float>(settings.paperWhite)};
    if (desc.Format==DXGI_FORMAT_R10G10B10A2_UNORM) constants[5]=1;
    if (desc.Format==DXGI_FORMAT_R16G16B16A16_FLOAT) constants[5]=2;
    auto into=transition(f.backbuffer.Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);
    r.commands->ResourceBarrier(1,&into);
    r.commands->SetGraphicsRootSignature(r.root.Get());
    ID3D12DescriptorHeap* heaps[]={r.srvHeap.Get()}; r.commands->SetDescriptorHeaps(1,heaps);
    r.commands->SetGraphicsRoot32BitConstants(0,12,constants,0);
    r.commands->SetGraphicsRootDescriptorTable(1,r.srvHeap->GetGPUDescriptorHandleForHeapStart());
    D3D12_VIEWPORT viewport{0,0,width,height,0,1}; D3D12_RECT scissor{0,0,static_cast<LONG>(desc.Width),static_cast<LONG>(desc.Height)};
    r.commands->RSSetViewports(1,&viewport); r.commands->RSSetScissorRects(1,&scissor);
    r.commands->OMSetRenderTargets(1,&f.rtv,FALSE,nullptr);
    if(previewBackdrop){const float clear[]{.055f,.065f,.055f,1.f};r.commands->ClearRenderTargetView(f.rtv,clear,0,nullptr);}
    if(current.wheelOpen) {
        const auto geometry=wheel_geometry(current);
        if(geometry.size()>maxWheelVertices){r.commands->Close();return false;}
        void* mapped{};
        D3D12_RANGE noRead{0,0};
        if(FAILED(f.wheelVertices->Map(0,&noRead,&mapped))){r.commands->Close();return false;}
        std::memcpy(mapped,geometry.data(),geometry.size()*sizeof(WheelVertex));
        f.wheelVertices->Unmap(0,nullptr);
        D3D12_VERTEX_BUFFER_VIEW view{f.wheelVertices->GetGPUVirtualAddress(),
            static_cast<UINT>(geometry.size()*sizeof(WheelVertex)),sizeof(WheelVertex)};
        r.commands->SetPipelineState(r.wheelPipeline.Get());
        r.commands->IASetVertexBuffers(0,1,&view);
        r.commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        r.commands->DrawInstanced(static_cast<UINT>(geometry.size()),1,0,0);
        r.commands->SetPipelineState(r.pipeline.Get());
    }
    r.commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    constants[10]=settings.brightness*settings.contentBrightness/10000.f;
    r.commands->SetGraphicsRoot32BitConstants(0,12,constants,0);
    if(current.wheelOpen)r.commands->DrawInstanced(6,1,0,0);
    if(settings.showIndicator && !current.name.empty()){
        const auto& card=placement.indicator;
        constants[0]=card.x/width*2-1;constants[1]=1-card.y/height*2;
        constants[2]=card.width/width*2;constants[3]=-card.height/height*2;constants[4]=opacity;
        constants[6]=static_cast<float>(cardWidth)/imageWidth;constants[7]=static_cast<float>(cardHeight)/imageHeight;
        constants[8]=0;constants[9]=static_cast<float>(indicatorRow)/imageHeight;
        r.commands->SetGraphicsRoot32BitConstants(0,12,constants,0);
        r.commands->DrawInstanced(6,1,0,0);
    }
    if(settings_ui::is_open()){
        DXGI_SWAP_CHAIN_DESC swapDesc{};
        auto uiFormat=desc.Format;
        if(uiFormat==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)uiFormat=DXGI_FORMAT_R8G8B8A8_UNORM;
        if(uiFormat==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)uiFormat=DXGI_FORMAT_B8G8R8A8_UNORM;
        if(SUCCEEDED(swapchain->GetDesc(&swapDesc)) && settings_ui::install(swapDesc.OutputWindow,r.device.Get(),r.queue.Get(),
            static_cast<unsigned>(r.frames.size()),uiFormat,r.srvHeap.Get()))
            settings_ui::render(r.commands.Get());
    }
    auto out=transition(f.backbuffer.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT);
    r.commands->ResourceBarrier(1,&out);
    if (FAILED(r.commands->Close())) { r.failed=true; return false; }
    ID3D12CommandList* lists[]={r.commands.Get()};
    if (originalExecute) originalExecute(r.queue.Get(),1,lists); else r.queue->ExecuteCommandLists(1,lists);
    const auto value=++r.lastFence;
    if (FAILED(r.queue->Signal(r.fence.Get(),value))) { r.failed=true; return false; }
    f.fence=value; return true;
}

HRESULT STDMETHODCALLTYPE present_hook(IDXGISwapChain* chain,UINT sync,UINT flags) {
    try {
    DWORD foregroundPid{}; GetWindowThreadProcessId(GetForegroundWindow(),&foregroundPid);
    settings_ui::synchronize();
    if (!(flags&DXGI_PRESENT_TEST) && gameplay.load() && capturedQueue.load() &&
        foregroundPid==GetCurrentProcessId() && GetTickCount64()-gameplayUpdated.load()<250) {
        Status current;
        std::lock_guard<std::mutex> gpuGuard(gpuMutex);
        {
            std::lock_guard<std::mutex> guard(statusMutex);
            current=status;
            if(settingsPending){
                settings=std::move(pendingSettings);settingsPending=false;
                load_selection_image();configure_icon_source();renderer.textureSerial=UINT64_MAX;
            }
        }
        if(settings_ui::is_open()){
            current.wheelOpen=!current.wheel.empty();
            if(current.wheelOpen)current.selected=0;
            // Preview selection differs from the closed wheel's texture cache.
            current.serial+=0x100000000ull;
        }
        const auto age=GetTickCount64()-current.changed;
        if (settings_ui::is_open() || (settings.enabled && (current.wheelOpen || (settings.showIndicator && !current.name.empty() && (settings.alwaysShow || age<settings.duration))))) {
            DXGI_SWAP_CHAIN_DESC desc{}; DWORD pid{};
            if (SUCCEEDED(chain->GetDesc(&desc))) GetWindowThreadProcessId(desc.OutputWindow,&pid);
            if (pid==GetCurrentProcessId() && desc.BufferDesc.Width>=640 && desc.BufferDesc.Height>=360) {
                const auto smooth=[](float t){t=std::clamp(t,0.f,1.f);return t*t*(3.f-2.f*t);};
                const float fade=current.wheelOpen||settings_ui::is_open()||settings.alwaysShow?1.f:
                    smooth(settings.fadeIn?static_cast<float>(age)/settings.fadeIn:1.f)*
                    smooth(settings.fadeOut?static_cast<float>(settings.duration-age)/settings.fadeOut:1.f);
                try { render(chain,current,fade); } catch (...) { renderer.failed=true; }
            }
        }
    }
    } catch (...) { /* A visual failure must not interrupt the game's Present. */ }
    return originalPresent(chain,sync,flags);
}
HRESULT STDMETHODCALLTYPE resize_hook(IDXGISwapChain* chain,UINT count,UINT width,UINT height,DXGI_FORMAT format,UINT flags) {
    { std::lock_guard<std::mutex> guard(gpuMutex); if (renderer.chain==chain) { renderer.clear(); renderer.failed=false; } }
    return originalResize(chain,count,width,height,format,flags);
}
HRESULT STDMETHODCALLTYPE resize1_hook(IDXGISwapChain3* chain,UINT count,UINT width,UINT height,DXGI_FORMAT format,UINT flags,const UINT* masks,IUnknown* const* queues) {
    { std::lock_guard<std::mutex> guard(gpuMutex); if (renderer.chain==chain) { renderer.clear(); renderer.failed=false; } }
    return originalResize1(chain,count,width,height,format,flags,masks,queues);
}
void STDMETHODCALLTYPE execute_hook(ID3D12CommandQueue* queue,UINT count,ID3D12CommandList* const* lists) {
    if (!capturedQueue.load() && queue->GetDesc().Type==D3D12_COMMAND_LIST_TYPE_DIRECT) {
        queue->AddRef(); ID3D12CommandQueue* empty=nullptr;
        if (!capturedQueue.compare_exchange_strong(empty,queue)) queue->Release();
    }
    originalExecute(queue,count,lists);
}

struct Discovery {
    HWND window{};
    ComPtr<IDXGIFactory4> factory;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<IDXGISwapChain> chain;
    ComPtr<IDXGISwapChain3> chain3;
    bool create(bool warp=false) {
        WNDCLASSW wc{}; wc.lpfnWndProc=DefWindowProcW; wc.hInstance=owner; wc.lpszClassName=L"ArrowQuickSwitch_RenderProbe";
        RegisterClassW(&wc);
        window=CreateWindowExW(0,wc.lpszClassName,L"",WS_OVERLAPPED,0,0,800,450,nullptr,nullptr,owner,nullptr);
        if (!window || FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return false;
        ComPtr<IDXGIAdapter> adapter;
        if (warp && FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)))) return false;
        if (FAILED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)))) return false;
        D3D12_COMMAND_QUEUE_DESC q{}; q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
        if (FAILED(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)))) return false;
        DXGI_SWAP_CHAIN_DESC desc{}; desc.BufferDesc.Width=800; desc.BufferDesc.Height=450;
        desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1;
        desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.BufferCount=2; desc.OutputWindow=window;
        desc.Windowed=TRUE; desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
        return SUCCEEDED(factory->CreateSwapChain(queue.Get(),&desc,&chain)) && SUCCEEDED(chain.As(&chain3));
    }
    void clear() {
        chain3.Reset(); chain.Reset(); queue.Reset(); device.Reset(); factory.Reset();
        if (window) { DestroyWindow(window); window=nullptr; }
        UnregisterClassW(L"ArrowQuickSwitch_RenderProbe",owner);
    }
    ~Discovery() { clear(); }
};
}

void publish(const std::wstring& name,std::uint32_t id,std::uint32_t count,const wchar_t* slotLabel) {
    std::lock_guard<std::mutex> guard(statusMutex);
    if (status.id!=id || status.indicatorLabel!=slotLabel) status.changed=GetTickCount64();
    if (status.id!=id || status.count!=count || status.name!=name || status.indicatorLabel!=slotLabel) ++status.serial;
    status.name=name; status.id=id; status.count=count; status.indicatorLabel=slotLabel;
}
void publish_wheel(const std::vector<WheelItem>& items,int selected,bool open,const wchar_t* slotLabel) {
    std::lock_guard<std::mutex> guard(statusMutex);
    if(!open && !status.wheelOpen)return;
    if(status.wheelOpen!=open || status.selected!=selected || status.wheel.size()!=items.size() || status.slotLabel!=slotLabel ||
       (open && !std::equal(status.wheel.begin(),status.wheel.end(),items.begin(),[](const auto& a,const auto& b){
           return a.name==b.name && a.count==b.count && a.badge==b.badge && a.color==b.color && a.effect==b.effect && a.iconsMask==b.iconsMask && a.iconId==b.iconId;
       }))) ++status.serial;
    status.wheelOpen=open;status.selected=selected;status.slotLabel=slotLabel;
    if(!open && !status.name.empty())status.changed=GetTickCount64()-200;
    // Keep the last page for the settings preview after the real wheel closes.
    if(open)status.wheel=items;
}
void set_gameplay(bool value) { gameplayUpdated.store(GetTickCount64()); gameplay.store(value); }
void configure(const Settings& options) {
    std::lock_guard<std::mutex> guard(statusMutex);
    pendingSettings=options;settingsPending=true;
}

bool install(HMODULE module,const Settings& options,Logger log) {
    settings=options; logger=log; owner=module;
    load_selection_image();
    configure_icon_source();
    Discovery probe;
    if (!probe.create()) { if (logger) logger("Indicator: D3D12 target discovery failed."); return false; }
    auto swap=*reinterpret_cast<void***>(probe.chain.Get());
    auto swap3=*reinterpret_cast<void***>(probe.chain3.Get());
    auto queue=*reinterpret_cast<void***>(probe.queue.Get());
    struct Hook { void* target; void* detour; void** original; };
    Hook hooks[]={
        {swap[8],reinterpret_cast<void*>(&present_hook),reinterpret_cast<void**>(&originalPresent)},
        {swap[13],reinterpret_cast<void*>(&resize_hook),reinterpret_cast<void**>(&originalResize)},
        {swap3[39],reinterpret_cast<void*>(&resize1_hook),reinterpret_cast<void**>(&originalResize1)},
        {queue[10],reinterpret_cast<void*>(&execute_hook),reinterpret_cast<void**>(&originalExecute)}
    };
    // Destroy the probe before enabling capture, so its queue cannot be mistaken
    // for the real game's queue during swap-chain cleanup.
    probe.clear();
    std::vector<void*> created;
    for (const auto& hook:hooks) {
        if (MH_CreateHook(hook.target,hook.detour,hook.original)!=MH_OK) {
            for (auto target:created) MH_RemoveHook(target);
            if (logger) logger("Indicator: hook creation failed; switching remains enabled."); return false;
        }
        created.push_back(hook.target);
    }
    for (auto target:created) {
        if (MH_QueueEnableHook(target)!=MH_OK) { for (auto previous:created) MH_RemoveHook(previous); return false; }
    }
    if (MH_ApplyQueued()!=MH_OK) {
        for (auto target:created) { MH_DisableHook(target); MH_RemoveHook(target); }
        return false;
    }
    if (logger) logger("Indicator: rendering hooks ready (duration=%ums, always=%u).",settings.duration,settings.alwaysShow?1u:0u);
    return true;
}

namespace {
bool capture_frame(IDXGISwapChain3* chain,const std::wstring& filename,bool expectText=true) {
    auto& r=renderer;r.wait();
    auto& frame=r.frames[chain->GetCurrentBackBufferIndex()];
    const auto desc=frame.backbuffer->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 size{};
    r.device->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&size);
    D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=size;
    buffer.Height=1;buffer.DepthOrArraySize=buffer.MipLevels=1;buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_READBACK;ComPtr<ID3D12Resource> readback;
    if(FAILED(r.device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback))) ||
       FAILED(frame.allocator->Reset()) || FAILED(r.commands->Reset(frame.allocator.Get(),nullptr)))return false;
    auto before=transition(frame.backbuffer.Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_SOURCE);
    r.commands->ResourceBarrier(1,&before);
    D3D12_TEXTURE_COPY_LOCATION from{},to{};from.pResource=frame.backbuffer.Get();from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    to.pResource=readback.Get();to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=footprint;
    r.commands->CopyTextureRegion(&to,0,0,0,&from,nullptr);
    auto after=transition(frame.backbuffer.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PRESENT);
    r.commands->ResourceBarrier(1,&after);
    if(FAILED(r.commands->Close()))return false;
    ID3D12CommandList* lists[]{r.commands.Get()};r.queue->ExecuteCommandLists(1,lists);
    if(FAILED(r.queue->Signal(r.fence.Get(),++r.lastFence)))return false;r.wait();
    void* mapped{};if(FAILED(readback->Map(0,nullptr,&mapped)))return false;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(desc.Width)*desc.Height*4);
    const auto* source=static_cast<const std::uint8_t*>(mapped)+footprint.Offset;
    unsigned bright=0;
    for(UINT y=0;y<desc.Height;++y)for(UINT x=0;x<desc.Width;++x){
        const auto* pixel=source+y*footprint.Footprint.RowPitch+x*4;
        auto* out=pixels.data()+(static_cast<std::size_t>(y)*desc.Width+x)*4;
        std::memcpy(out,pixel,4);if(pixel[0]>120 && pixel[1]>100 && pixel[2]>80)++bright;
    }
    readback->Unmap(0,nullptr);
    if(logger)logger("Preview: GPU readback contains %u bright UI pixels.",bright);
    return (!expectText || bright>100) && png(filename.c_str(),pixels.data(),static_cast<UINT>(desc.Width),desc.Height,static_cast<UINT>(desc.Width*4));
}
}
bool preview(const wchar_t* filename,Logger diagnostics,const Settings* options) {
    logger=diagnostics;owner=GetModuleHandleW(nullptr);settings=options?*options:Settings{};previewBackdrop=true;
    load_selection_image();
    const auto save=[&](const wchar_t* suffix,const Pixels& image){
        const auto path=std::wstring(filename)+suffix;return png(path.c_str(),image.bytes.data(),imageWidth,imageHeight,imageWidth*4);
    };
    if(!save(L"",raster_card(L"Stormwing Bone Arrow",20,L"ARROW SLOT 1")))return false;
    Status example;example.name=L"Stormwing Bone Arrow";example.id=50190000;example.count=20;
    example.changed=GetTickCount64();example.serial=1;
    Status wheel;
    wheel.wheel={{L"Spiritflame Arrow",21,L'\u2191',0xE9D79A,L"HOLY + FROST",static_cast<std::uint16_t>(effects::bit(effects::holy)|effects::bit(effects::frost))},
        {L"Lightning Arrow",18,L'\u2191',0xE8D16C,L"LIGHTNING",effects::bit(effects::lightning)},
        {L"Bloodbone Arrow (Fletched)",40,L'\u2191',0xC94D52,L"BLEED + PHYSICAL",static_cast<std::uint16_t>(effects::bit(effects::bleed)|effects::bit(effects::physical))},
        {L"Normal Arrow",77,L'\u2191',0xD1C39A,L"PHYSICAL",effects::bit(effects::physical)}};
    wheel.wheelOpen=true;wheel.selected=1;wheel.serial=2;wheel.name=example.name;wheel.count=20;
    if(!save(L".wheel.png",raster_wheel(wheel)))return false;
    wheel.wheel.push_back({L"Rotbone Arrow",50,L'\u2191',0xB97B63,L"ROT + PHYSICAL",static_cast<std::uint16_t>(effects::bit(effects::rot)|effects::bit(effects::physical))});
    wheel.wheel.push_back({L"Golden Great Arrow",30,L'\u2191',0xE9D79A,L"HOLY",effects::bit(effects::holy)});
    wheel.wheel.push_back({L"Poisonbone Arrow (Fletched)",47,L'\u2191',0x87B66A,L"POISON + PHYSICAL",static_cast<std::uint16_t>(effects::bit(effects::poison)|effects::bit(effects::physical))});
    wheel.wheel.push_back({L"Stormwing Bone Arrow",15,L'\u2191',0xD1C39A,L"PHYSICAL",effects::bit(effects::physical)});
    wheel.slotLabel=L"ARROW SLOT 1  2/3";wheel.selected=-1;++wheel.serial;
    if(!save(L".page.png",raster_wheel(wheel)))return false;
    wheel.selected=1;++wheel.serial;if(!save(L".foreground.png",raster_wheel(wheel,false)))return false;
    Discovery probe;if(!probe.create(true))return false;
    capturedQueue.store(probe.queue.Get());previewBackdrop=true;
    bool ok=render(probe.chain.Get(),example,1.f) && capture_frame(probe.chain3.Get(),std::wstring(filename)+L".gpu-card.png",settings.showIndicator);
    renderer.clear();
    // A larger actual-GPU preview is legible enough to compare with the game.
    RECT client{0,0,1280,900};AdjustWindowRect(&client,WS_OVERLAPPED,FALSE);
    SetWindowPos(probe.window,nullptr,0,0,client.right-client.left,client.bottom-client.top,SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);
    ok=ok && SUCCEEDED(probe.chain->ResizeBuffers(2,1280,900,DXGI_FORMAT_R8G8B8A8_UNORM,0));
    if(ok)ok=render(probe.chain.Get(),wheel,1.f) && capture_frame(probe.chain3.Get(),std::wstring(filename)+L".gpu-wheel.png");
    // Exercise the actual ImGui backend, font upload and shared descriptor heap.
    const auto ini=std::filesystem::path(std::wstring(filename)+L".settings-test.ini");
    if(ok){
        ok=theme::save(ini,settings);settings_ui::set_config(ini);settings_ui::set_enabled(true);settings_ui::toggle();settings_ui::synchronize();
        if(ok)ok=render(probe.chain.Get(),wheel,1.f) && capture_frame(probe.chain3.Get(),std::wstring(filename)+L".settings.png");
        renderer.clear();
        // Resize while the panel is open, and compile both HDR menu shaders.
        for(const auto format:{DXGI_FORMAT_R10G10B10A2_UNORM,DXGI_FORMAT_R16G16B16A16_FLOAT}){
            ok=ok && SUCCEEDED(probe.chain->ResizeBuffers(2,1024,576,format,0));
            if(ok)ok=render(probe.chain.Get(),wheel,1.f);
            renderer.wait();renderer.clear();
        }
        settings_ui::close();settings_ui::synchronize();settings_ui::set_config({});
    }
    std::error_code error;std::filesystem::remove(ini,error);
    if(ok){
        ok=SUCCEEDED(probe.chain->ResizeBuffers(2,640,360,DXGI_FORMAT_R8G8B8A8_UNORM,0));
        ++example.serial;example.count=99;if(ok)ok=render(probe.chain.Get(),example,.6f);
    }
    renderer.wait();renderer.clear();capturedQueue.store(nullptr);previewBackdrop=false;
    return ok;
}
}
