#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include "../src/item_icons.hpp"
#include "../src/icon_archive.hpp"
#include <map>
#include <cstdio>
#include <cstdarg>

void report(const char* text,...){va_list args;va_start(args,text);std::vprintf(text,args);va_end(args);std::putchar('\n');}
int wmain(int argc,wchar_t** argv){
    if(argc<4 || argc>5)return 2;
    if(std::wstring(argv[2])==L"0"){
        using namespace arrow_switch::item_icons;
        Archive archive(argv[1],argc==5?argv[4]:L"");Bytes bytes;
        if(!archive.read("menu/low/01_common.sblytbnd.dcx",bytes))return 1;
        std::map<std::string,std::pair<unsigned,unsigned>> ranges;
        for(const auto& [id,rect]:parse_layout(bytes)){
            auto& range=ranges[rect.atlas];if(!range.first || id<range.first)range.first=id;if(id>range.second)range.second=id;
        }
        for(const auto& [atlas,range]:ranges)std::printf("%s: %u..%u\n",atlas.c_str(),range.first,range.second);
        return 0;
    }
    const auto image=arrow_switch::item_icons::extract(argv[1],argc==5?argv[4]:L"",static_cast<unsigned>(std::wcstoul(argv[2],nullptr,10)),report);
    if(!image){std::puts("Item icon was unavailable.");return 1;}
    using Microsoft::WRL::ComPtr;
    const auto init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICStream> stream;ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> frame;
    bool ok=SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))) &&
        SUCCEEDED(factory->CreateStream(&stream)) && SUCCEEDED(stream->InitializeFromFilename(argv[3],GENERIC_WRITE)) &&
        SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder)) && SUCCEEDED(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache)) &&
        SUCCEEDED(encoder->CreateNewFrame(&frame,nullptr)) && SUCCEEDED(frame->Initialize(nullptr)) && SUCCEEDED(frame->SetSize(image->width,image->height));
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppRGBA;
    ok=ok && SUCCEEDED(frame->SetPixelFormat(&format)) && SUCCEEDED(frame->WritePixels(image->height,image->width*4,
        static_cast<UINT>(image->rgba.size()),const_cast<BYTE*>(image->rgba.data()))) && SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
    frame.Reset();encoder.Reset();stream.Reset();factory.Reset();
    if(SUCCEEDED(init))CoUninitialize();
    std::printf("Runtime extraction and BC texture crop: %s\n",ok?"passed":"failed");return ok?0:1;
}
