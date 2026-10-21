#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "settings_ui.hpp"
#include "indicator.hpp"
#include "theme_config.hpp"
#include "bindings.hpp"
#include "wheel_bindings.hpp"
#include "ui_cursor.hpp"
#include "window_input.hpp"
#include "binding_capture.hpp"
#include "../third_party/imgui/imgui.h"
#include "../third_party/imgui/backends/imgui_impl_win32.h"
#include "../third_party/imgui/backends/imgui_impl_dx12.h"
#include <array>
#include <atomic>
#include <mutex>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace arrow_switch::settings_ui {
namespace {
std::atomic<bool> visible{false};
std::atomic<bool> panelEnabled{true};
std::recursive_mutex mutex;
std::filesystem::path config;
ImGuiContext* context{};
HWND window{};WNDPROC originalProc{};
Cursor cursor;
bool synchronized=false,dirty=false,sameFamily=false,audio=true,ready=false;
unsigned hoverCooldown=90;
indicator::Settings editing;
wheel_bindings::Values controls;
std::string notice;
binding_capture::Capture capture;
int captureTarget=-1;
bool manualBindings=false;
std::atomic<bool> captureBlocked{false},padConnected{false};
std::atomic<std::uint32_t> padButtons{0};
std::wstring& capture_value(int target){
    if(target<4)return controls.keyboard[static_cast<unsigned>(target)];
    if(target<10)return controls.pad[static_cast<unsigned>(target-4)];
    return controls.shortcut;
}
void update_capture(){
    const auto keys=window_input::snapshot();
    capture.keyboard(keys);
    capture.controller_buttons(padButtons.load(),padConnected.load());
    if(capture.completed && captureTarget>=0){
        capture_value(captureTarget)=capture.result;dirty=true;capture.completed=false;
        notice="Binding captured. Save changes to apply it.";
    }
    if(capture.cancelled){capture.cancelled=false;notice="Rebinding cancelled.";}
    if(!capture.active && !binding_capture::held(keys) && !padButtons.load())captureBlocked.store(false);
}

struct Context {
    ImGuiContext* previous=ImGui::GetCurrentContext();
    Context(){ImGui::SetCurrentContext(context);}
    ~Context(){ImGui::SetCurrentContext(previous);}
};
LRESULT CALLBACK messages(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam){
    WNDPROC forward{};
    {
        std::lock_guard<std::recursive_mutex> guard(mutex);
        forward=originalProc;
        window_input::message(message,wParam,lParam);
        if(message==WM_NCDESTROY){window=nullptr;originalProc=nullptr;ready=false;visible.store(false);}
        if(context && ready && visible.load()){
            Context current;
            const bool capturing=captureBlocked.load();
            capture.keyboard(window_input::snapshot());
            if(capturing && (message==WM_KEYDOWN || message==WM_SYSKEYDOWN || message==WM_CHAR))return 0;
            if(message==WM_INPUT){
                RAWINPUT raw{};UINT bytes=sizeof(raw);
                if(GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam),RID_INPUT,&raw,&bytes,sizeof(RAWINPUTHEADER))!=static_cast<UINT>(-1) && raw.header.dwType==RIM_TYPEMOUSE)
                    {window_input::raw_mouse(raw.data.mouse);mouse_input(raw.data.mouse);}
                // DefWindowProc performs the required foreground RAWINPUT cleanup.
                return DefWindowProcW(hwnd,message,wParam,lParam);
            }
            // The game warps the OS pointer to the center. Use raw mouse input
            // for both cursor motion and clicks; ignore resulting legacy events.
            if((message>=WM_MOUSEFIRST && message<=WM_MOUSELAST) || message==WM_NCMOUSEMOVE || message==WM_MOUSELEAVE || message==WM_NCMOUSELEAVE)return 0;
            ImGui_ImplWin32_WndProcHandler(hwnd,message,wParam,lParam);
            if(message==WM_KEYDOWN || message==WM_KEYUP ||
               message==WM_SYSKEYDOWN || message==WM_SYSKEYUP || message==WM_CHAR)return 0;
        }
    }
    return forward?CallWindowProcW(forward,hwnd,message,wParam,lParam):DefWindowProcW(hwnd,message,wParam,lParam);
}
void load(){
    editing=theme::load(config);const auto file=config.wstring();
    sameFamily=GetPrivateProfileIntW(L"Switching",L"CycleSameFamily",0,file.c_str())!=0;
    audio=GetPrivateProfileIntW(L"Audio",L"Enabled",1,file.c_str())!=0;
    hoverCooldown=std::clamp(GetPrivateProfileIntW(L"Audio",L"HoverCooldownMs",90,file.c_str()),50u,300u);
    controls=wheel_bindings::load(config);
    dirty=false;notice.clear();capture={};captureTarget=-1;captureBlocked.store(false);
}
bool slider(const char* label,unsigned& setting,int low,int high,const char* format="%d"){
    int value=static_cast<int>(setting);if(!ImGui::SliderInt(label,&value,low,high,format))return false;
    setting=static_cast<unsigned>(value);return true;
}
bool color(const char* label,unsigned& setting){
    float value[]{((setting>>16)&255)/255.f,((setting>>8)&255)/255.f,(setting&255)/255.f};
    if(!ImGui::ColorEdit3(label,value,ImGuiColorEditFlags_NoInputs))return false;
    setting=(static_cast<unsigned>(value[0]*255+.5f)<<16)|(static_cast<unsigned>(value[1]*255+.5f)<<8)|static_cast<unsigned>(value[2]*255+.5f);return true;
}
bool text(const char* label,std::wstring& value){
    std::array<char,2048> buffer{};
    WideCharToMultiByte(CP_UTF8,0,value.c_str(),-1,buffer.data(),static_cast<int>(buffer.size()),nullptr,nullptr);
    if(ImGui::InputText(label,buffer.data(),buffer.size())){
        std::array<wchar_t,1024> wide{};MultiByteToWideChar(CP_UTF8,0,buffer.data(),-1,wide.data(),static_cast<int>(wide.size()));
        value=wide.data();
    }
    return ImGui::IsItemDeactivatedAfterEdit();
}
bool binding(const char* label,std::wstring& value,int target,bool optional=false){
    if(manualBindings)return text(label,value);
    ImGui::PushID(target);
    std::array<char,512> displayed{};
    WideCharToMultiByte(CP_UTF8,0,value.c_str(),-1,displayed.data(),static_cast<int>(displayed.size()),nullptr,nullptr);
    ImGui::BeginDisabled(captureBlocked.load());
    if(ImGui::Button(displayed[0]?displayed.data():"None",ImVec2(155,0))){
        captureTarget=target;
        capture.begin(target>=4 && target<10?binding_capture::Device::controller:binding_capture::Device::keyboard,
            window_input::snapshot(),padButtons.load());
        captureBlocked.store(true);notice.clear();
    }
    ImGui::SameLine();
    const std::string_view caption(label);const auto end=caption.find("##");
    ImGui::TextUnformatted(label,label+(end==caption.npos?caption.size():end));
    bool changed=false;
    if(optional){ImGui::SameLine();if(ImGui::SmallButton("Clear")){value=L"None";changed=true;}}
    ImGui::EndDisabled();
    if(capture.active && captureTarget==target){
        if(capture.waiting)ImGui::TextWrapped("Release held inputs first. Esc cancels.");
        else if(capture.device==binding_capture::Device::controller)
            ImGui::TextWrapped(padConnected.load()?"Hold your button or chord together, then release. Esc cancels.":"Connect a controller, then press its buttons. Esc cancels.");
        else ImGui::TextWrapped("Press a key or mouse button. Hold modifiers first for a chord. Esc cancels.");
    }
    ImGui::PopID();return changed;
}
void panel(){
    const auto& io=ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(450,std::min(700.f,io.DisplaySize.y-40)),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(std::max(12.f,io.DisplaySize.x-470),20),ImGuiCond_FirstUseEver);
    bool keep=true;
    if(ImGui::Begin("Arrow Wheel settings",&keep,ImGuiWindowFlags_NoCollapse)){
        ImGui::TextWrapped("Changes preview immediately. Save to keep them. Your last opened wheel is shown behind this panel.");
        bool changed=false;
        ImGui::BeginChild("Options",ImVec2(0,-85));
        if(ImGui::BeginTabBar("Customization")){
            if(ImGui::BeginTabItem("Wheel")){
                changed|=slider("Size",editing.wheelScale,50,150,"%d%%");
                changed|=slider("Surface opacity",editing.wheelOpacity,0,100,"%d%%");
                changed|=ImGui::SliderInt("Horizontal position",&editing.wheelOffsetX,-600,600);
                changed|=ImGui::SliderInt("Vertical position",&editing.wheelOffsetY,-400,400);
                changed|=slider("Cancel area",editing.cancelRadius,30,140);
                ImGui::SeparatorText("Colors");
                changed|=color("Background",editing.backgroundColor);changed|=color("Selected wedge",editing.selectedFillColor);
                changed|=color("Border",editing.accentColor);changed|=color("Selection marker",editing.selectionColor);
                changed|=color("Names",editing.textColor);changed|=color("Selected name",editing.selectedTextColor);
                changed|=color("Counts",editing.countColor);changed|=color("Details",editing.mutedColor);
                ImGui::SeparatorText("Text and icons");
                changed|=text("Name font",editing.fontFamily);changed|=text("Detail font",editing.detailFont);
                changed|=slider("Name size",editing.nameSize,12,24);changed|=slider("Count size",editing.countSize,10,22);
                ImGui::TextWrapped("Long names scale down to fit their wedge.");
                changed|=slider("Center text size",editing.centerSize,14,28);changed|=slider("Detail text size",editing.effectSize,9,15);
                changed|=ImGui::Checkbox("Text shadows",&editing.textShadow);changed|=ImGui::Checkbox("Show counts",&editing.showCounts);
                changed|=ImGui::Checkbox("Show effect labels",&editing.showEffectLabels);changed|=ImGui::Checkbox("Show icons",&editing.showIcons);
                changed|=ImGui::Checkbox("Use item artwork",&editing.showItemIcons);changed|=ImGui::Checkbox("Show effect badges",&editing.showEffectIcons);
                changed|=slider("Item artwork size",editing.itemIconScale,50,150,"%d%%");changed|=slider("Effect badge size",editing.iconScale,50,125,"%d%%");
                changed|=ImGui::Checkbox("Show selection marker",&editing.showMarker);changed|=slider("Marker size",editing.markerScale,50,200,"%d%%");
                changed|=text("Marker PNG",editing.selectionImage);changed|=text("Icon source directory",editing.iconSourceDirectory);
                ImGui::TextWrapped("Leave icon source blank for automatic loading. For custom mod assets, choose the root containing the menu folder.");
                changed|=slider("Border thickness",editing.borderScale,50,200,"%d%%");changed|=slider("Brightness",editing.brightness,50,200,"%d%%");
                changed|=slider("Text and artwork brightness",editing.contentBrightness,75,150,"%d%%");
                changed|=slider("Selection glow",editing.selectionGlow,0,100,"%d%%");
                changed|=slider("Icon backdrop",editing.iconBackdrop,0,80,"%d%%");
                changed|=ImGui::Checkbox("Decorative border",&editing.decorativeRings);
                changed|=ImGui::Checkbox("Animate selection glow",&editing.animateSelection);
                changed|=slider("HDR white level",editing.paperWhite,80,400,"%d nits");
                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("Indicator")){
                changed|=ImGui::Checkbox("Show ammo text",&editing.showIndicator);changed|=ImGui::Checkbox("Always show",&editing.alwaysShow);
                changed|=slider("Text scale",editing.scale,50,200,"%d%%");changed|=slider("Display time",editing.duration,500,30000,"%d ms");
                changed|=text("Font",editing.indicatorFont);changed|=slider("Name size",editing.indicatorNameSize,12,30);
                changed|=slider("Detail size",editing.indicatorDetailSize,9,18);changed|=color("Text color",editing.indicatorTextColor);
                changed|=color("Detail color",editing.indicatorDetailColor);changed|=slider("Gap below wheel",editing.indicatorGap,0,180);
                changed|=ImGui::SliderInt("Horizontal offset",&editing.indicatorOffsetX,-600,600);
                changed|=ImGui::SliderInt("Vertical offset",&editing.indicatorOffsetY,-400,400);
                changed|=slider("Fade in",editing.fadeIn,0,3000,"%d ms");changed|=slider("Fade out",editing.fadeOut,0,5000,"%d ms");
                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("Controls")){
                constexpr const char* keyboardLabels[]{"Arrow slot 1##key","Arrow slot 2##key","Bolt slot 1##key","Bolt slot 2##key"};
                constexpr const char* padLabels[]{"Arrow slot 1##pad","Arrow slot 2##pad","Bolt slot 1##pad","Bolt slot 2##pad","Next page##pad","Previous page##pad"};
                ImGui::Checkbox("Edit binding names manually",&manualBindings);
                ImGui::TextWrapped("Click a binding to record a key, mouse button or controller chord.");
                ImGui::SeparatorText("Keyboard and mouse");
                for(unsigned i=0;i<4;++i)changed|=binding(keyboardLabels[i],controls.keyboard[i],static_cast<int>(i));
                ImGui::TextWrapped("Examples: Key1, E, Ctrl+E, Mouse4. Hold to open; release to select.");
                ImGui::SeparatorText("Controller");
                for(unsigned i=0;i<6;++i)changed|=binding(padLabels[i],controls.pad[i],static_cast<int>(i+4),true);
                ImGui::TextWrapped("Use A, B, X, Y, LB, RB, LT, RT, LS, RS, Back, Start, or DPadUp/Down/Left/Right. Join buttons with + for a chord. None disables a binding. Names use Xbox notation.");
                ImGui::TextWrapped("Bindings apply after saving, without restarting the game.");
                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("Behavior")){
                ImGui::Checkbox("Edit binding names manually",&manualBindings);
                changed|=binding("Settings shortcut",controls.shortcut,10,true);
                ImGui::TextWrapped("Examples: F10, Ctrl+F10. None or an empty value disables the panel.");
                ImGui::TextWrapped("After saving a removed shortcut, re-enable the panel by setting SettingsKey in the INI.");
                ImGui::Separator();
                changed|=ImGui::Checkbox("Limit each wheel to its equipped ammo type",&sameFamily);
                changed|=ImGui::Checkbox("Opening and hover sounds",&audio);changed|=slider("Hover sound interval",hoverCooldown,50,300,"%d ms");
                ImGui::TextWrapped("Behavior and shortcuts apply after saving, without restarting the game.");
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::EndChild();
        if(changed){dirty=true;indicator::configure(editing);notice.clear();}
        ImGui::Separator();
        ImGui::BeginDisabled(captureBlocked.load());
        if(ImGui::Button("Save changes")){
            input::Binding shortcut;
            if(const auto* error=wheel_bindings::validate(controls))notice=error;
            else{
            input::parse_optional(controls.shortcut,shortcut);
            const auto file=config.wstring();
            bool ok=theme::save(config,editing);
            ok=WritePrivateProfileStringW(L"Switching",L"CycleSameFamily",sameFamily?L"1":L"0",file.c_str())!=0 && ok;
            ok=WritePrivateProfileStringW(L"Audio",L"Enabled",audio?L"1":L"0",file.c_str())!=0 && ok;
            ok=WritePrivateProfileStringW(L"Audio",L"HoverCooldownMs",std::to_wstring(hoverCooldown).c_str(),file.c_str())!=0 && ok;
            ok=WritePrivateProfileStringW(L"Controls",L"SettingsKey",controls.shortcut.c_str(),file.c_str())!=0 && ok;
            for(unsigned i=0;i<4;++i)ok=WritePrivateProfileStringW(L"Controls",wheel_bindings::keyboardKeys[i],controls.keyboard[i].c_str(),file.c_str())!=0 && ok;
            for(unsigned i=0;i<6;++i)ok=WritePrivateProfileStringW(L"Controller",wheel_bindings::controllerKeys[i],controls.pad[i].c_str(),file.c_str())!=0 && ok;
            notice=ok?"Saved.":"Could not save the INI. Check that the folder is writable.";if(ok)dirty=false;
            if(ok && !shortcut.key){panelEnabled.store(false);visible.store(false);}
            }
        }
        ImGui::SameLine();if(ImGui::Button("Revert")){load();indicator::configure(editing);}
        ImGui::SameLine();if(ImGui::Button(dirty?"Close without saving":"Close"))visible.store(false);
        ImGui::EndDisabled();
        if(!notice.empty())ImGui::TextWrapped("%s",notice.c_str());
        else if(dirty)ImGui::TextUnformatted("Unsaved changes");
    }
    ImGui::End();if(!keep)visible.store(false);
}
}
bool attach_input_window(HWND hwnd){
    std::lock_guard<std::recursive_mutex> guard(mutex);
    DWORD pid{};GetWindowThreadProcessId(hwnd,&pid);
    if(!hwnd || pid!=GetCurrentProcessId() || !IsWindow(hwnd))return false;
    if(window==hwnd && originalProc)return true;
    if(window && IsWindow(window)){
        // Do not tear down a different mod's subclass chain.
        if(reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window,GWLP_WNDPROC))!=messages)return false;
        SetWindowLongPtrW(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(originalProc));
    }
    SetLastError(0);
    const auto previous=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(messages)));
    if(!previous){window=nullptr;originalProc=nullptr;window_input::focus(false);return false;}
    window=hwnd;originalProc=previous;window_input::focus(GetForegroundWindow()==hwnd);
    return true;
}
void ensure_input_window(){
    std::lock_guard<std::recursive_mutex> guard(mutex);
    if(window && IsWindow(window) && originalProc)return;
    HWND candidate{};
    EnumWindows([](HWND hwnd,LPARAM value)->BOOL{
        DWORD pid{};GetWindowThreadProcessId(hwnd,&pid);
        if(pid!=GetCurrentProcessId() || !IsWindowVisible(hwnd) || GetWindow(hwnd,GW_OWNER))return TRUE;
        *reinterpret_cast<HWND*>(value)=hwnd;return FALSE;
    },reinterpret_cast<LPARAM>(&candidate));
    if(candidate)attach_input_window(candidate);
}
void set_config(const std::filesystem::path& path){std::lock_guard<std::recursive_mutex> guard(mutex);config=path;}
bool is_open(){return visible.load();}
bool is_capturing(){return visible.load() && captureBlocked.load();}
void controller_input(std::uint32_t buttons,bool connected){padButtons.store(buttons);padConnected.store(connected);}
void toggle(){if(panelEnabled.load())visible.store(!visible.load());}
void close(){visible.store(false);}
void set_enabled(bool value){panelEnabled.store(value);if(!value)visible.store(false);}
void mouse_input(const RAWMOUSE& mouse){
    std::lock_guard<std::recursive_mutex> guard(mutex);
    if(!ready || !visible.load() || !context)return;
    capture.keyboard(window_input::snapshot());
    Context current;RECT area{};GetClientRect(window,&area);
    auto& io=ImGui::GetIO();
    if(mouse.usFlags&MOUSE_MOVE_ABSOLUTE){
        const bool desktop=(mouse.usFlags&MOUSE_VIRTUAL_DESKTOP)!=0;
        POINT point{};if(desktop)point={GetSystemMetrics(SM_XVIRTUALSCREEN),GetSystemMetrics(SM_YVIRTUALSCREEN)};
        point.x+=static_cast<LONG>(mouse.lLastX/65535.f*GetSystemMetrics(desktop?SM_CXVIRTUALSCREEN:SM_CXSCREEN));
        point.y+=static_cast<LONG>(mouse.lLastY/65535.f*GetSystemMetrics(desktop?SM_CYVIRTUALSCREEN:SM_CYSCREEN));
        ScreenToClient(window,&point);cursor.x=static_cast<float>(point.x);cursor.y=static_cast<float>(point.y);cursor.initialized=true;
        cursor.move(0,0,static_cast<float>(area.right),static_cast<float>(area.bottom));
    }else cursor.move(static_cast<float>(mouse.lLastX),static_cast<float>(mouse.lLastY),static_cast<float>(area.right),static_cast<float>(area.bottom));
    io.AddMousePosEvent(cursor.x,cursor.y);
    constexpr USHORT down[]{RI_MOUSE_LEFT_BUTTON_DOWN,RI_MOUSE_RIGHT_BUTTON_DOWN,RI_MOUSE_MIDDLE_BUTTON_DOWN,RI_MOUSE_BUTTON_4_DOWN,RI_MOUSE_BUTTON_5_DOWN};
    constexpr USHORT up[]{RI_MOUSE_LEFT_BUTTON_UP,RI_MOUSE_RIGHT_BUTTON_UP,RI_MOUSE_MIDDLE_BUTTON_UP,RI_MOUSE_BUTTON_4_UP,RI_MOUSE_BUTTON_5_UP};
    for(int i=0;i<5;++i){if(!captureBlocked.load() && (mouse.usButtonFlags&down[i]))io.AddMouseButtonEvent(i,true);if(mouse.usButtonFlags&up[i])io.AddMouseButtonEvent(i,false);}
    const auto wheel=static_cast<SHORT>(mouse.usButtonData)/static_cast<float>(WHEEL_DELTA);
    if(mouse.usButtonFlags&RI_MOUSE_WHEEL)io.AddMouseWheelEvent(0,wheel);
    if(mouse.usButtonFlags&RI_MOUSE_HWHEEL)io.AddMouseWheelEvent(wheel,0);
}
void synchronize(){
    std::lock_guard<std::recursive_mutex> guard(mutex);
    const auto open=visible.load();if(open==synchronized || config.empty())return;
    synchronized=open;
    if(open)load();else indicator::configure(theme::load(config));
    if(context){Context current;auto& io=ImGui::GetIO();io.AddFocusEvent(open);if(!open)for(int i=0;i<5;++i)io.AddMouseButtonEvent(i,false);}
}
bool install(HWND hwnd,ID3D12Device* device,ID3D12CommandQueue* queue,unsigned frames,DXGI_FORMAT format,ID3D12DescriptorHeap* heap){
    std::lock_guard<std::recursive_mutex> guard(mutex);
    if(ready)return true;
    if(!attach_input_window(hwnd))return false;
    const auto previous=ImGui::GetCurrentContext();context=ImGui::CreateContext();ImGui::SetCurrentContext(context);
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly=true;
    wchar_t windows[1024]{};GetWindowsDirectoryW(windows,1024);
    const auto fontPath=std::filesystem::path(windows)/L"Fonts/segoeui.ttf";
    char utf8[4096]{};WideCharToMultiByte(CP_UTF8,0,fontPath.c_str(),-1,utf8,4096,nullptr,nullptr);
    if(GetFileAttributesW(fontPath.c_str())!=INVALID_FILE_ATTRIBUTES)io.Fonts->AddFontFromFileTTF(utf8,16.f);
    ImGui::StyleColorsDark();auto& style=ImGui::GetStyle();style.WindowRounding=7;style.FrameRounding=4;
    style.WindowPadding=ImVec2(14,14);style.ItemSpacing=ImVec2(9,8);
    style.Colors[ImGuiCol_WindowBg]=ImVec4(.055f,.065f,.055f,.98f);
    style.Colors[ImGuiCol_CheckMark]=ImVec4(.88f,.76f,.50f,1);
    style.Colors[ImGuiCol_SliderGrab]=ImVec4(.70f,.59f,.38f,1);
    style.Colors[ImGuiCol_SliderGrabActive]=ImVec4(.95f,.83f,.57f,1);
    style.Colors[ImGuiCol_Header]=ImVec4(.30f,.27f,.19f,1);
    style.Colors[ImGuiCol_Button]=ImVec4(.27f,.25f,.18f,1);
    style.Colors[ImGuiCol_TitleBgActive]=ImVec4(.24f,.21f,.14f,1);
    style.Colors[ImGuiCol_FrameBg]=ImVec4(.15f,.16f,.13f,1);
    style.Colors[ImGuiCol_FrameBgHovered]=ImVec4(.29f,.27f,.19f,1);
    style.Colors[ImGuiCol_Tab]=ImVec4(.17f,.18f,.14f,1);
    style.Colors[ImGuiCol_TabSelected]=ImVec4(.32f,.28f,.18f,1);
    ImGui_ImplDX12_InitInfo info;info.Device=device;info.CommandQueue=queue;info.NumFramesInFlight=static_cast<int>(frames);
    info.RTVFormat=format;info.SrvDescriptorHeap=heap;
    const auto stride=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    info.LegacySingleSrvCpuDescriptor=heap->GetCPUDescriptorHandleForHeapStart();info.LegacySingleSrvCpuDescriptor.ptr+=stride;
    info.LegacySingleSrvGpuDescriptor=heap->GetGPUDescriptorHandleForHeapStart();info.LegacySingleSrvGpuDescriptor.ptr+=stride;
    if(!ImGui_ImplWin32_Init(hwnd)){ImGui::DestroyContext(context);context=nullptr;ImGui::SetCurrentContext(previous);return false;}
    ImGui_ImplWin32_UseExternalMousePosition(true);
    if(!ImGui_ImplDX12_Init(&info)){ImGui_ImplWin32_Shutdown();ImGui::DestroyContext(context);context=nullptr;ImGui::SetCurrentContext(previous);return false;}
    RECT area{};GetClientRect(hwnd,&area);cursor.move(0,0,static_cast<float>(area.right),static_cast<float>(area.bottom));
    io.AddMousePosEvent(cursor.x,cursor.y);
    ready=true;
    if(!ready){ImGui_ImplDX12_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext(context);context=nullptr;}
    ImGui::SetCurrentContext(previous);return ready;
}
void render(ID3D12GraphicsCommandList* commands){
    std::lock_guard<std::recursive_mutex> guard(mutex);if(!ready || !visible.load())return;
    update_capture();
    Context current;auto& io=ImGui::GetIO();io.MouseDrawCursor=true;
    ImGui_ImplDX12_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();panel();ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(),commands);
}
void shutdown(){
    std::lock_guard<std::recursive_mutex> guard(mutex);if(!context)return;
    Context current;
    ImGui_ImplDX12_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext(context);
    current.previous=nullptr;context=nullptr;ready=false;
}
}
