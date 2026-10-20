#include "game_access.hpp"
#include "arrow_names.hpp"
#include "indicator.hpp"
#include "bindings.hpp"
#include "wheel_bindings.hpp"
#include "controller.hpp"
#include "wheel.hpp"
#include "ammo_effects.hpp"
#include "config_file.hpp"
#include "theme_config.hpp"
#include "item_icons.hpp"
#include "settings_ui.hpp"
#include "window_input.hpp"
#include <MinHook.h>
#include <atomic>
#include <bit>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace {
using namespace arrow_switch;
HMODULE module{};
std::atomic<bool> started{}, enabled{true};
std::atomic_flag updating=ATOMIC_FLAG_INIT;
std::filesystem::path folder;
std::mutex logMutex;
game::Addresses addresses;
std::uintptr_t namesStatic{};
indicator::Settings indicatorSettings;
bool refreshIndicator{};
std::atomic<int> rawDx{},rawDy{};
std::atomic<int> rawPageSteps{};
std::atomic<bool> rawWheelOpen{};
using RawInputData=UINT(WINAPI*)(HRAWINPUT,UINT,LPVOID,PUINT,UINT);
RawInputData originalRawInputData{};
using XInputGetStateFn=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);
XInputGetStateFn xinputGetState{};
HMODULE xinputModule{};
// The second argument is frame time in XMM1, not a pointer in RDX.
using HudUpdate=void(*)(void*,float,void*);
HudUpdate originalHud{};
using Equip=void(*)(void*,std::int32_t,const std::uint32_t*,std::int32_t,bool,bool,bool);
Equip equip{};
struct EquipmentSoundEvent {
    std::int32_t eventId=0x2710;
    std::int32_t target=-1;
    std::int32_t category=3;
    std::int32_t padding=0;
    void* vtable=nullptr;
};
using EquipmentSoundFn=void(*)(EquipmentSoundEvent*);
EquipmentSoundFn equipmentSound{};
void* equipmentSoundVtable{};

struct Config {
    input::Binding diagnostic{VK_F8,0},wheel{'1',0},settingsKey{VK_F10,0};
    std::array<input::Binding,3> otherWheels{{{'2',0},{'3',0},{'4',0}}};
    std::wstring diagnosticText,wheelText,settingsKeyText;
    std::array<std::wstring,3> otherWheelTexts;
    std::array<controller::Binding,4> controllerWheels{};
    std::array<std::wstring,4> controllerWheelTexts;
    controller::Binding controllerNextPage{},controllerPreviousPage{};
    unsigned controllerIndex=0,controllerDeadzone=25,controllerTrigger=180;
    bool controllerEnabled=false;
    unsigned settle=150, timeout=3000;
    bool deferMouse=true;
    bool cycleSameFamily=false;
    unsigned cancelRadius=78;
    bool soundEnabled=true;
    unsigned hoverSoundCooldown=90;
} config;
AmmoSlot activeIndicatorSlot=AmmoSlot::arrow1;

void log(const char* format,...) {
    std::lock_guard<std::mutex> guard(logMutex);
    FILE* file{};
    if (_wfopen_s(&file,(folder/L"ArrowQuickSwitch-0.4.log").c_str(),L"a")) return;
    SYSTEMTIME t{}; GetLocalTime(&t);
    std::fprintf(file,"[%02u:%02u:%02u] ",t.wHour,t.wMinute,t.wSecond);
    va_list args; va_start(args,format); std::vfprintf(file,format,args); va_end(args);
    std::fputc('\n',file); std::fclose(file);
}

bool supported_version() {
    wchar_t filename[32768]{};
    if (!GetModuleFileNameW(nullptr,filename,32768)) return false;
    DWORD ignored{};
    auto size=GetFileVersionInfoSizeW(filename,&ignored);
    if (!size) return false;
    std::vector<std::uint8_t> bytes(size);
    if (!GetFileVersionInfoW(filename,0,size,bytes.data())) return false;
    VS_FIXEDFILEINFO* info{}; UINT length{};
    if (!VerQueryValueW(bytes.data(),L"\\",reinterpret_cast<void**>(&info),&length) || length<sizeof(*info)) return false;
    return info->dwSignature==0xFEEF04BD && info->dwFileVersionMS==MAKELONG(7,2) &&
        info->dwFileVersionLS==MAKELONG(0,1); // file 2.7.1.0 / application 1.17.1
}

bool load_config() {
    const auto ini=(folder/L"ArrowQuickSwitch.ini").wstring();
    const auto get=[&](const wchar_t* section,const wchar_t* key,int fallback) {
        return GetPrivateProfileIntW(section,key,fallback,ini.c_str());
    };
    const auto binding=[&](const wchar_t* key,const wchar_t* fallback,input::Binding& parsed,std::wstring& text,bool optional=false) {
        wchar_t buffer[256]{};
        const auto length=GetPrivateProfileStringW(L"Controls",key,fallback,buffer,256,ini.c_str());
        text=buffer;
        if (length>=255 || !(optional?input::parse_optional(text,parsed):input::parse(text,parsed))) { log("Invalid %ls binding: %ls",key,text.c_str()); return false; }
        return true;
    };
    if (!binding(L"DiagnosticKey",L"F8",config.diagnostic,config.diagnosticText) ||
        !binding(L"SettingsKey",L"F10",config.settingsKey,config.settingsKeyText,true) ||
        !binding(L"Arrow1WheelKey",L"Key1",config.wheel,config.wheelText) ||
        !binding(L"Arrow2WheelKey",L"Key2",config.otherWheels[0],config.otherWheelTexts[0]) ||
        !binding(L"Bolt1WheelKey",L"Key3",config.otherWheels[1],config.otherWheelTexts[1]) ||
        !binding(L"Bolt2WheelKey",L"Key4",config.otherWheels[2],config.otherWheelTexts[2])) return false;
    const auto controllerBinding=[&](const wchar_t* key,const wchar_t* fallback,controller::Binding& parsed,std::wstring* text=nullptr) {
        wchar_t buffer[256]{};
        const auto length=GetPrivateProfileStringW(L"Controller",key,fallback,buffer,256,ini.c_str());
        if(text)*text=buffer;
        if(length>=255 || !controller::parse(buffer,parsed)) {log("Invalid controller binding %ls: %ls",key,buffer);return false;}
        return true;
    };
    constexpr const wchar_t* controllerKeys[]={L"Arrow1WheelButton",L"Arrow2WheelButton",L"Bolt1WheelButton",L"Bolt2WheelButton"};
    for(std::size_t i=0;i<config.controllerWheels.size();++i)
        if(!controllerBinding(controllerKeys[i],L"None",config.controllerWheels[i],&config.controllerWheelTexts[i]))return false;
    if(!controllerBinding(L"NextPageButton",L"RB",config.controllerNextPage) ||
       !controllerBinding(L"PreviousPageButton",L"LB",config.controllerPreviousPage))return false;
    const auto controllerIndex=get(L"Controller",L"ControllerIndex",0);
    const auto deadzone=get(L"Controller",L"RightStickDeadzonePercent",25);
    const auto trigger=get(L"Controller",L"TriggerThreshold",180);
    if(controllerIndex>3 || deadzone<5 || deadzone>90 || trigger<32 || trigger>255)return false;
    config.controllerIndex=static_cast<unsigned>(controllerIndex);
    config.controllerDeadzone=static_cast<unsigned>(deadzone);
    config.controllerTrigger=static_cast<unsigned>(trigger);
    for(std::size_t i=0;i<config.controllerWheels.size();++i){
        config.controllerEnabled=config.controllerEnabled || config.controllerWheels[i].buttons!=0;
        for(std::size_t j=i+1;j<config.controllerWheels.size();++j)
            if(controller::ambiguous(config.controllerWheels[i],config.controllerWheels[j]))return false;
        if(controller::ambiguous(config.controllerWheels[i],config.controllerNextPage) &&
           controller::down(config.controllerNextPage,config.controllerWheels[i].buttons))return false;
        if(controller::ambiguous(config.controllerWheels[i],config.controllerPreviousPage) &&
           controller::down(config.controllerPreviousPage,config.controllerWheels[i].buttons))return false;
    }
    if(controller::ambiguous(config.controllerNextPage,config.controllerPreviousPage))return false;
    config.settle=get(L"Switching",L"MouseReleaseDelayMs",150);
    config.timeout=get(L"Switching",L"RequestTimeoutMs",3000);
    config.deferMouse=get(L"Switching",L"DeferWhileMouseHeld",1)!=0;
    config.cycleSameFamily=get(L"Switching",L"CycleSameFamily",0)!=0;
    config.soundEnabled=get(L"Audio",L"Enabled",1)!=0;
    config.hoverSoundCooldown=std::clamp(get(L"Audio",L"HoverCooldownMs",90),50u,300u);
    enabled.store(get(L"Switching",L"Enabled",1)!=0);
    indicatorSettings=theme::load(ini);
    config.cancelRadius=indicatorSettings.cancelRadius;
    const std::array<input::Binding,6> keys{config.diagnostic,config.wheel,config.settingsKey,
        config.otherWheels[0],config.otherWheels[1],config.otherWheels[2]};
    for(std::size_t i=0;i<keys.size();++i)for(std::size_t j=i+1;j<keys.size();++j)if(keys[i]==keys[j])return false;
    return config.settle<=2000 && config.timeout>=250 && config.timeout<=10000 && config.settle<config.timeout;
}

UINT WINAPI hooked_raw_input(HRAWINPUT input,UINT command,LPVOID data,PUINT size,UINT headerSize) {
    const auto result=originalRawInputData(input,command,data,size,headerSize);
    if(command==RID_INPUT && data && size && result!=static_cast<UINT>(-1) && result>=sizeof(RAWINPUT)){
        const auto* raw=static_cast<const RAWINPUT*>(data);
        if(raw->header.dwType==RIM_TYPEMOUSE)window_input::raw_mouse(raw->data.mouse);
    }
    if(settings_ui::is_open() && command==RID_INPUT && data && size && result!=static_cast<UINT>(-1) && result>=sizeof(RAWINPUT)){
        auto* raw=static_cast<RAWINPUT*>(data);
        if(raw->header.dwType==RIM_TYPEMOUSE){settings_ui::mouse_input(raw->data.mouse);raw->data.mouse={};}
        return result;
    }
    if(rawWheelOpen.load(std::memory_order_relaxed) && command==RID_INPUT && data && size &&
       result!=static_cast<UINT>(-1) && result>=sizeof(RAWINPUT)) {
        auto* raw=static_cast<RAWINPUT*>(data);
        if(raw->header.dwType==RIM_TYPEMOUSE && !(raw->data.mouse.usFlags&MOUSE_MOVE_ABSOLUTE)) {
            rawDx.fetch_add(raw->data.mouse.lLastX,std::memory_order_relaxed);
            rawDy.fetch_add(raw->data.mouse.lLastY,std::memory_order_relaxed);
        }
        if(raw->header.dwType==RIM_TYPEMOUSE && (raw->data.mouse.usButtonFlags&RI_MOUSE_WHEEL)){
            rawPageSteps.fetch_add(static_cast<SHORT>(raw->data.mouse.usButtonData)/WHEEL_DELTA,std::memory_order_relaxed);
            raw->data.mouse.usButtonFlags&=static_cast<USHORT>(~RI_MOUSE_WHEEL);
            raw->data.mouse.usButtonData=0;
        }
    }
    return result;
}

// Keep the SEH boundary free of C++ objects that require stack unwinding.
bool native_equip(std::uintptr_t player,AmmoSlot slot,std::uint32_t handle,std::uint32_t index,DWORD* fault) {
    __try {
        equip(reinterpret_cast<void*>(player+0x2B0),native_slot(slot),&handle,static_cast<std::int32_t>(index),true,true,false);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        *fault=GetExceptionCode();
        return false;
    }
}

void diagnostic(void* hud) {
    const auto fe=reinterpret_cast<std::uintptr_t>(hud)-0x4E60;
    std::uint8_t state{}; game::read(fe+0x78,state);
    auto world=game::pointer(addresses.world);
    auto chr=world ? game::pointer(world+0x1E508) : 0;
    auto bag=chr ? game::pointer(chr+0x190) : 0;
    auto stats=bag ? game::pointer(bag) : 0;
    std::int32_t hp{}; if (stats) game::read(stats+0x138,hp);
    log("DIAGNOSTIC enabled=%u gameplay=%u hud_state=%u fe_vtable_RVA=%llX hp=%d",
        enabled.load()?1u:0u,game::normal_gameplay(addresses,hud)?1u:0u,static_cast<unsigned>(state),
        static_cast<unsigned long long>(game::pointer(fe)-game::base()),hp);
    game::Snapshot snapshot;
    if (!game::snapshot(addresses,snapshot)) { log("Snapshot unavailable: player/inventory/params not ready."); return; }
    log("SNAPSHOT slot1=%u slot2=%u mapping_valid=%u",snapshot.first,snapshot.second,game::current_matches(snapshot)?1u:0u);
    for (const auto& arrow:snapshot.arrows)
        log("ARROW id=%u quantity=%u type=%u%s",arrow.id,arrow.count,arrow.type,arrow.id==snapshot.first?" SLOT1":"");
}

void switch_ammo(AmmoSlot slot,void* hud,std::uint32_t target) {
    game::Snapshot before;
    if (!game::snapshot(addresses,before) || !game::current_matches_slot(before,slot)) {
        log("SWITCH rejected: current inventory mapping is unavailable or inconsistent."); return;
    }
    const auto index=slot_index(slot),peer=slot_index(peer_slot(slot));
    const auto options=wheel::eligible(before.arrows,before.slotIds[index],before.slotIds[peer],slot,
        config.cycleSameFamily);
    const auto selectedIt=std::find_if(options.begin(),options.end(),[&](const Arrow& a){return a.id==target;});
    const auto* selected=selectedIt==options.end()?nullptr:&*selectedIt;
    if (!selected || !slot_accepts(slot,selected->type)) { log("SWITCH no eligible owned ammo outside the paired slot."); return; }
    if(selected->id==before.slotIds[index]) return;
    const Arrow candidate=*selected;
    if (!game::normal_gameplay(addresses,hud) || !game::revalidate_slot(addresses,before,candidate,slot)) {
        log("SWITCH canceled: gameplay or equipment changed during selection."); return;
    }
    log("SWITCH request native_slot=%d id=%u -> %u quantity=%u",native_slot(slot),before.slotIds[index],candidate.id,candidate.count);
    DWORD fault{};
    if (!native_equip(before.player,slot,candidate.handle,candidate.index+before.tail,&fault)) {
        enabled.store(false);
        log("SWITCH DISABLED: native equipment call raised exception %08lX. Restart before further testing.",fault);
        return;
    }
    std::array<std::uint32_t,4> ids{};
    std::uint32_t handle{},equippedIndex{};
    std::array<std::uint32_t,22> indices{};
    bool ok=game::pointer(addresses.gdm)==before.manager && game::pointer(before.manager+8)==before.player &&
        game::read(before.player+handle_offset(slot),handle) && handle==candidate.handle &&
        game::read(before.player+index_offset(slot),equippedIndex) && equippedIndex==candidate.index+before.tail &&
        game::read(before.player+0x2B8,indices);
    for(std::size_t i=0;i<4;++i){
        if(!game::read(before.player+id_offset(static_cast<AmmoSlot>(i)),ids[i]) ||
           ids[i]!=(i==index?candidate.id:before.slotIds[i]))ok=false;
    }
    for (std::size_t i=0;i<indices.size();++i) if (i!=static_cast<std::size_t>(native_slot(slot)) && indices[i]!=before.indices[i]) ok=false;
    if (!ok) {
        enabled.store(false);
        log("SWITCH DISABLED: native post-check failed (native_slot=%d id=%u expected=%u).",native_slot(slot),ids[index],candidate.id);
        return;
    }
    log("SWITCH OK native_slot=%d id=%u quantity=%u; other ammo slots and equipment indices preserved.",native_slot(slot),candidate.id,candidate.count);
    activeIndicatorSlot=slot;
    refreshIndicator=true;
}

void update_indicator(bool allowed,std::uint64_t now) {
    indicator::set_gameplay(allowed);
    if (!allowed || !indicatorSettings.enabled) return;
    static std::uint64_t lastPoll{};
    static std::uint32_t previousId=0xFFFFFFFFu;
    static AmmoSlot previousSlot=AmmoSlot::arrow1;
    static std::wstring currentName;
    if (!refreshIndicator && now-lastPoll<80) return;
    lastPoll=now; refreshIndicator=false;
    auto manager=game::pointer(addresses.gdm);
    auto player=manager ? game::pointer(manager+8) : 0;
    auto inventory=player ? game::pointer(player+0x5D0) : 0;
    auto list=inventory ? game::pointer(inventory+0x10) : 0;
    std::uint32_t id{},handle{},index{},tail{},capacity{};
    game::Entry entry{};
    const auto slot=activeIndicatorSlot;
    if (!list || !game::read(player+id_offset(slot),id) || !game::read(player+handle_offset(slot),handle) ||
        !game::read(player+index_offset(slot),index) || !game::read(inventory+0x1C,tail) || index<tail ||
        !game::read(inventory+0xC,capacity) || capacity>10000 || index-tail>=capacity ||
        !game::read(list+(index-tail)*sizeof(game::Entry),entry) || entry.id!=id || entry.handle!=handle) {
        indicator::publish(L"",0xFFFFFFFFu,0,slot_label(slot)); previousId=0xFFFFFFFFu; return;
    }
    if (id!=previousId || slot!=previousSlot) {
        currentName=arrow_name(namesStatic,id);
        if (currentName.empty()) { currentName=slot_index(slot)<2?L"Arrow":L"Bolt"; log("Indicator: active weapon-name lookup unavailable for ammo %u.",id); }
        previousId=id;previousSlot=slot;
    }
    indicator::publish(currentName,id,entry.count,slot_label(slot));
}

bool read_effects(std::uintptr_t weaponParam,std::uintptr_t effectParam,std::uint32_t id,effects::Evidence& evidence) {
    const auto weapon=game::row(weaponParam,id);
    if(!weapon)return false;
    constexpr std::uintptr_t damageOffsets[]{0xC8,0xCA,0xCC,0xCE,0x18C};
    for(std::size_t i=0;i<5;++i)if(!game::read(weapon+damageOffsets[i],evidence.damage[i]))return false;
    constexpr std::uintptr_t statusOffsets[]{0xCC,0xD0,0xD4,0xD8,0x1A8,0x338,0x33C};
    for(std::size_t effect=0;effect<3;++effect){
        std::int32_t effectId{};
        if(!game::read(weapon+0x44+effect*4,effectId))return false;
        if(effectId<=0 || !effectParam)continue;
        const auto row=game::row(effectParam,static_cast<std::uint32_t>(effectId));
        if(!row)continue;
        for(std::size_t status=0;status<7;++status){
            std::int32_t power{};
            if(game::read(row+statusOffsets[status],power))
                evidence.buildup[status]=std::max(evidence.buildup[status],power);
        }
    }
    return true;
}

void play_wheel_sound(bool hover) {
    (void)hover;
    if(!config.soundEnabled || !equipmentSound || !equipmentSoundVtable)return;
    EquipmentSoundEvent event{};
    event.vtable=equipmentSoundVtable;
    equipmentSound(&event);
}
void publish_wheel_page(const std::vector<indicator::WheelItem>& items,std::size_t page,int selected,AmmoSlot slot){
    const auto start=page*wheel::pageSize;
    const auto count=wheel::page_items(items.size(),page);
    const std::vector<indicator::WheelItem> visible(items.begin()+start,items.begin()+start+count);
    auto label=std::wstring(slot_label(slot));
    const auto pages=wheel::page_count(items.size());
    if(pages>1)label+=L"  " + std::to_wstring(page+1) + L"/" + std::to_wstring(pages);
    indicator::publish_wheel(visible,selected,true,label.c_str());
}
void update(void* hud) {
    static bool first=true;
    static input::Edge diagnosticEdge,settingsEdge;
    static bool initialSnapshot=false;
    static std::array<bool,4> wheelWasDown{};
    static std::array<bool,4> controllerWheelWasDown{};
    static std::uint32_t previousPadButtons{};
    static bool wheelOpen=false;
    static bool wheelController=false;
    static AmmoSlot wheelSlot=AmmoSlot::arrow1,pendingSlot=AmmoSlot::arrow1;
    static std::vector<Arrow> wheelArrows;
    static std::vector<indicator::WheelItem> wheelItems;
    static float wheelX=0,wheelY=0;
    static int wheelSelected=-1;
    static std::uint64_t lastHoverSound{};
    static std::size_t wheelPage=0;
    static std::uint32_t pendingWheel=0xFFFFFFFFu;
    static std::uint64_t pendingSince{};
    if (first) { first=false; log("Equipment HUD hook is executing."); }
    const auto now=GetTickCount64();
    static std::uint64_t lastInputCheck=0;
    if(now-lastInputCheck>=500){lastInputCheck=now;settings_ui::ensure_input_window();}
    // Apply safe customization changes between wheel interactions. The renderer
    // receives a snapshot and consumes it on its own thread between frames.
    static std::uint64_t lastThemeCheck=0;
    static std::filesystem::file_time_type themeWrite{};
    if(!wheelOpen && pendingWheel==0xFFFFFFFFu && now-lastThemeCheck>=500){
        lastThemeCheck=now;
        const auto path=folder/L"ArrowQuickSwitch.ini";
        std::error_code error;
        const auto modified=std::filesystem::last_write_time(path,error);
        if(!error && modified!=themeWrite && std::filesystem::file_size(path,error)>0 && !error){
            themeWrite=modified;
            indicatorSettings=theme::load(path);
            config.cancelRadius=indicatorSettings.cancelRadius;
            const auto ini=path.wstring();
            config.cycleSameFamily=GetPrivateProfileIntW(L"Switching",L"CycleSameFamily",0,ini.c_str())!=0;
            config.soundEnabled=GetPrivateProfileIntW(L"Audio",L"Enabled",1,ini.c_str())!=0;
            config.hoverSoundCooldown=std::clamp(GetPrivateProfileIntW(L"Audio",L"HoverCooldownMs",90,ini.c_str()),50u,300u);
            const auto bindings=wheel_bindings::load(path);
            if(const auto* reason=wheel_bindings::validate(bindings))log("Controls reload rejected: %s",reason);
            else{
                input::Binding next;input::parse_optional(bindings.shortcut,next);
                if(next!=config.settingsKey){config.settingsKey=next;settingsEdge.held=true;}
                settings_ui::set_enabled(next.key!=0);
                input::parse(bindings.keyboard[0],config.wheel);
                for(unsigned i=1;i<4;++i)input::parse(bindings.keyboard[i],config.otherWheels[i-1]);
                config.controllerEnabled=false;
                for(unsigned i=0;i<4;++i){
                    controller::parse(bindings.pad[i],config.controllerWheels[i]);
                    config.controllerEnabled|=config.controllerWheels[i].buttons!=0;
                }
                controller::parse(bindings.pad[4],config.controllerNextPage);
                controller::parse(bindings.pad[5],config.controllerPreviousPage);
                // Require release before a newly assigned held binding can open a wheel.
                wheelWasDown.fill(true);controllerWheelWasDown.fill(true);previousPadButtons=0xFFFFFFFFu;
            }
            indicator::configure(indicatorSettings);
            log("Customization reloaded from ArrowQuickSwitch.ini.");
        }
    }

    DWORD pid{}; GetWindowThreadProcessId(GetForegroundWindow(),&pid);
    const auto inputState=window_input::snapshot();
    const auto key_down=[&](int key){return inputState.down(key);};
    const bool focused=pid==GetCurrentProcessId() && inputState.focused;
    const unsigned modifiers=(key_down(VK_CONTROL)?input::ctrl:0u) | (key_down(VK_SHIFT)?input::shift:0u) |
        (key_down(VK_MENU)?input::alt:0u) | ((key_down(VK_LWIN)||key_down(VK_RWIN))?input::win:0u);
    const bool diagPressed=diagnosticEdge.sample(key_down(config.diagnostic.key),input::matches(config.diagnostic,modifiers),focused);
    const bool gameplay=focused && game::normal_gameplay(addresses,hud);
    if(settingsEdge.sample(key_down(config.settingsKey.key),input::matches(config.settingsKey,modifiers),focused) && gameplay){
        settings_ui::toggle();
        if(wheelOpen){wheelOpen=false;rawWheelOpen.store(false);indicator::publish_wheel({},-1,false);}
        pendingWheel=0xFFFFFFFFu;
    }
    if(!focused || !gameplay)settings_ui::close();
    const std::array<input::Binding,4> wheelBindings{config.wheel,config.otherWheels[0],config.otherWheels[1],config.otherWheels[2]};
    std::array<bool,4> wheelDown{};
    for(std::size_t i=0;i<4;++i)wheelDown[i]=key_down(wheelBindings[i].key) && input::matches(wheelBindings[i],modifiers);
    XINPUT_STATE padState{};
    const bool padConnected=config.controllerEnabled && xinputGetState &&
        xinputGetState(config.controllerIndex,&padState)==ERROR_SUCCESS;
    const auto padButtons=padConnected?controller::held_buttons(padState.Gamepad,config.controllerTrigger):0u;
    std::array<bool,4> controllerWheelDown{};
    for(std::size_t i=0;i<4;++i)controllerWheelDown[i]=controller::down(config.controllerWheels[i],padButtons);
    int opening=-1;
    bool openingController=false;
    if(!wheelOpen && !settings_ui::is_open() && gameplay && enabled.load() && indicatorSettings.enabled)
        for(std::size_t i=0;i<4;++i){
            if(wheelDown[i]&&!wheelWasDown[i]){opening=static_cast<int>(i);break;}
            if(controllerWheelDown[i]&&!controllerWheelWasDown[i]){
                opening=static_cast<int>(i);openingController=true;break;
            }
        }
    if(opening>=0) {
        wheelController=openingController;
        wheelSlot=static_cast<AmmoSlot>(opening);
        activeIndicatorSlot=wheelSlot;
        game::Snapshot current;
        const bool snap=game::snapshot(addresses,current);
        if(snap && game::current_matches_slot(current,wheelSlot)) {
            const auto selectedIndex=slot_index(wheelSlot);
            const auto* equipped=game::equipped_entry(current,wheelSlot);
            if(equipped && equipped->handle!=current.slotHandles[selectedIndex])
                log("WHEEL %ls: secondary equipped handle is stale; ID and inventory index match (equipped=%u inventory=%u).",
                    slot_label(wheelSlot),current.slotHandles[selectedIndex],equipped->handle);
            const auto effectParam=game::effect_param(addresses.params);
            log("WHEEL %ls: live status-effect parameter %s.",slot_label(wheelSlot),effectParam?"found":"unavailable");
            wheelArrows=wheel::eligible(current.arrows,current.slotIds[selectedIndex],
                current.slotIds[slot_index(peer_slot(wheelSlot))],wheelSlot,config.cycleSameFamily);
            wheelItems.clear();wheelItems.reserve(wheelArrows.size());
            for(const auto& arrow:wheelArrows){
                auto name=arrow_name(namesStatic,arrow.id);
                if(name.empty())name=(selectedIndex<2?L"Arrow ":L"Bolt ")+std::to_wstring(arrow.id);
                effects::Evidence evidence;
                if(read_effects(current.param,effectParam,arrow.id,evidence)){
                    const auto mask=effects::classify(evidence);
                    const auto label=mask?effects::label(mask):std::wstring(L"UNCLASSIFIED");
                    wheelItems.push_back({std::move(name),arrow.count,L'\u2191',effects::accent(mask),label,mask});
                    if(std::popcount(static_cast<unsigned>(mask))>1)
                        log("WHEEL ammo id=%u multiple effects: %ls.",arrow.id,label.c_str());
                }else{
                    const auto hint=wheel::affinity(name);
                    const auto effect=std::wstring(hint.label)==L"ARROW" && selectedIndex>=2?L"BOLT":hint.label;
                    wheelItems.push_back({std::move(name),arrow.count,hint.badge,hint.color,effect,0});
                    log("WHEEL %ls: damage/status data unavailable for ammo id=%u; using name hint.",slot_label(wheelSlot),arrow.id);
                }
            }
            for(std::size_t i=0;i<wheelItems.size();++i){
                std::uint16_t iconId{};
                const auto row=game::row(current.param,wheelArrows[i].id);
                if(row && game::read(row+0xBE,iconId))wheelItems[i].iconId=iconId;
            }
            wheelOpen=!wheelItems.empty();wheelX=wheelY=0;wheelSelected=-1;wheelPage=0;
            rawDx.store(0);rawDy.store(0);rawPageSteps.store(0);rawWheelOpen.store(wheelOpen&&!wheelController);
            if(wheelOpen) {
                publish_wheel_page(wheelItems,wheelPage,wheelSelected,wheelSlot);
                play_wheel_sound(false);
            }
            else log("WHEEL %ls: no eligible owned ammo (current=%u peer=%u).",slot_label(wheelSlot),
                current.slotIds[selectedIndex],current.slotIds[slot_index(peer_slot(wheelSlot))]);
        } else if(!snap) {
            log("WHEEL %ls: inventory snapshot unavailable.",slot_label(wheelSlot));
        } else {
            const auto i=slot_index(wheelSlot);
            const auto found=std::find_if(current.arrows.begin(),current.arrows.end(),[&](const auto& item){return item.id==current.slotIds[i];});
            log("WHEEL %ls: mapping rejected id=%u handle=%u index=%u tail=%u; inventory handle=%u index=%u type=%u.",
                slot_label(wheelSlot),current.slotIds[i],current.slotHandles[i],current.slotIndices[i],current.tail,
                found==current.arrows.end()?0:found->handle,found==current.arrows.end()?0:found->index,
                found==current.arrows.end()?0:found->type);
            std::array<std::uint32_t,16> nearby{};
            if(game::read(current.player+0x350,nearby))
                log("WHEEL nearby handles 350-38F: %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X",
                    nearby[0],nearby[1],nearby[2],nearby[3],nearby[4],nearby[5],nearby[6],nearby[7],
                    nearby[8],nearby[9],nearby[10],nearby[11],nearby[12],nearby[13],nearby[14],nearby[15]);
            log("WHEEL equipment indices 6-15: %u %u %u %u %u %u %u %u %u %u",
                current.indices[6],current.indices[7],current.indices[8],current.indices[9],current.indices[10],
                current.indices[11],current.indices[12],current.indices[13],current.indices[14],current.indices[15]);
            std::array<std::uint32_t,24> ids{};
            if(game::read(current.player+0x3A0,ids))
                log("WHEEL nearby IDs 3A0-3FF: %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X",
                    ids[0],ids[1],ids[2],ids[3],ids[4],ids[5],ids[6],ids[7],ids[8],ids[9],ids[10],ids[11],
                    ids[12],ids[13],ids[14],ids[15],ids[16],ids[17],ids[18],ids[19],ids[20],ids[21],ids[22],ids[23]);
        }
    }
    const bool activeWheelDown=wheelController?controllerWheelDown[slot_index(wheelSlot)]:wheelDown[slot_index(wheelSlot)];
    if(wheelOpen && activeWheelDown && gameplay && focused) {
        int steps=rawPageSteps.exchange(0);
        if(wheelController){
            steps=0;
            if(controller::down(config.controllerNextPage,padButtons) &&
               !controller::down(config.controllerNextPage,previousPadButtons))++steps;
            if(controller::down(config.controllerPreviousPage,padButtons) &&
               !controller::down(config.controllerPreviousPage,previousPadButtons))--steps;
        }
        bool pageChanged=false;
        const auto pages=wheel::page_count(wheelItems.size());
        if(steps && pages>1){
            wheelPage=wheel::turn_page(wheelPage,pages,steps);
            pageChanged=true;
        }
        if(wheelController){
            controller::right_stick(padState.Gamepad,config.controllerDeadzone,wheelX,wheelY);
        }else{
            wheelX=std::clamp(wheelX+static_cast<float>(rawDx.exchange(0))*1.1f,-240.f,240.f);
            wheelY=std::clamp(wheelY+static_cast<float>(rawDy.exchange(0))*1.1f,-240.f,240.f);
            // Some input setups do not use GetRawInputData. Use the actual cursor then.
            if(!originalRawInputData){
                POINT cursor{};RECT client{};auto window=GetForegroundWindow();
                if(GetCursorPos(&cursor) && ScreenToClient(window,&cursor) && GetClientRect(window,&client)) {
                    wheelX=static_cast<float>(cursor.x-(client.right-client.left)/2);
                    wheelY=static_cast<float>(cursor.y-(client.bottom-client.top)/2);
                }
            }
        }
        const auto selected=wheel::selection(wheelX,wheelY,wheel::page_items(wheelItems.size(),wheelPage),
                                             static_cast<float>(config.cancelRadius));
        if(pageChanged || selected!=wheelSelected){
            if(selected>=0 && now-lastHoverSound>=config.hoverSoundCooldown){
                play_wheel_sound(true);lastHoverSound=now;
            }
            wheelSelected=selected;publish_wheel_page(wheelItems,wheelPage,wheelSelected,wheelSlot);
        }
    }
    if(wheelOpen && (!activeWheelDown || !gameplay || !focused)) {
        rawWheelOpen.store(false);rawDx.store(0);rawDy.store(0);rawPageSteps.store(0);
        indicator::publish_wheel({},-1,false,slot_label(wheelSlot));
        const auto selectedIndex=wheelPage*wheel::pageSize+static_cast<std::size_t>(std::max(wheelSelected,0));
        if(gameplay && focused && (!wheelController || padConnected) && wheelSelected>=0 && selectedIndex<wheelArrows.size()){
            pendingWheel=wheelArrows[selectedIndex].id;pendingSlot=wheelSlot;pendingSince=now;
        }
        wheelOpen=false;wheelController=false;wheelArrows.clear();wheelItems.clear();wheelSelected=-1;
    }
    wheelWasDown=wheelDown;
    controllerWheelWasDown=controllerWheelDown;
    previousPadButtons=padButtons;
    if (focused && diagPressed) diagnostic(hud);
    if (!initialSnapshot && gameplay) { initialSnapshot=true; diagnostic(hud); }
    if(pendingWheel!=0xFFFFFFFFu){
        const bool buttonsDown=key_down(VK_LBUTTON)||key_down(VK_RBUTTON);
        if(!gameplay || !enabled.load() || now-pendingSince>config.timeout)pendingWheel=0xFFFFFFFFu;
        else if(!config.deferMouse || !buttonsDown && now-pendingSince>=config.settle){
            const auto target=pendingWheel;pendingWheel=0xFFFFFFFFu;
            switch_ammo(pendingSlot,hud,target);
        }
    }
    update_indicator(gameplay,now);
}

void hooked_hud(void* hud,float arg2,void* arg3) {
    originalHud(hud,arg2,arg3);
    if (!hud || updating.test_and_set()) return;
    try { update(hud); }
    catch (...) { enabled.store(false); log("SWITCH DISABLED: unexpected error in equipment update."); }
    updating.clear();
}

DWORD WINAPI initialize(void*) {
    wchar_t path[32768]{}; GetModuleFileNameW(module,path,32768);
    folder=std::filesystem::path(path).parent_path();
    log("ArrowQuickSwitch 0.6.2: paged ammo wheels and secondary-slot handle compatibility.");
    if (!supported_version()) { log("Disabled: unsupported executable version (requires file 2.7.1.0 / game 1.17.1)."); return 0; }
    wchar_t executable[32768]{};
    if(GetModuleFileNameW(nullptr,executable,32768))item_icons::initialize(std::filesystem::path(executable).parent_path(),log);
    if (!config_file::ensure(folder/L"ArrowQuickSwitch.ini"))
        log("Could not create or update ArrowQuickSwitch.ini; using available settings and built-in defaults.");
    if (!load_config()) { log("Disabled: invalid or conflicting controls/timing configuration."); return 0; }
    settings_ui::set_config(folder/L"ArrowQuickSwitch.ini");
    settings_ui::ensure_input_window();
    settings_ui::set_enabled(config.settingsKey.key!=0);
    {
        constexpr const wchar_t* libraries[]={L"xinput1_4.dll",L"xinput9_1_0.dll",L"xinput1_3.dll"};
        for(const auto* library:libraries){
            xinputModule=LoadLibraryExW(library,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
            if(!xinputModule)continue;
            xinputGetState=reinterpret_cast<XInputGetStateFn>(GetProcAddress(xinputModule,"XInputGetState"));
            if(xinputGetState){log("Controller input: %ls, index %u.",library,config.controllerIndex);break;}
            FreeLibrary(xinputModule);xinputModule=nullptr;
        }
        if(!xinputGetState)log("Controller input unavailable; keyboard wheels remain active.");
    }
    if (!addresses.resolve()) { log("Disabled: missing, ambiguous, or modified game signatures. No hook installed."); return 0; }
    {
        constexpr std::uintptr_t soundRva=0x815D50, vtableRva=0x2A9EDD0;
        const auto soundAddress=game::base()+soundRva;
        const auto vtableAddress=game::base()+vtableRva;
        std::uintptr_t firstVtableEntry{};
        if(game::equal(soundAddress,{0x40,0x53,0x48,0x83,0xEC,0x20,0x83,0x39,0x00,0x48,0x8B,0xD9}) &&
            game::read(vtableAddress,firstVtableEntry) && firstVtableEntry>=game::base()){
            equipmentSound=reinterpret_cast<EquipmentSoundFn>(soundAddress);
            equipmentSoundVtable=reinterpret_cast<void*>(vtableAddress);
            log("Wheel audio: game menu cue available.");
        }else log("Wheel audio disabled: game sound signature mismatch.");
    }
    namesStatic=game::rip(game::find({0x48,0x8B,0x3D,-1,-1,-1,-1,0x44,0x0F,0xB6,0x30,0x48,0x85,0xFF,0x75}));
    equip=reinterpret_cast<Equip>(addresses.equip);
    auto status=MH_Initialize();
    if (status!=MH_OK && status!=MH_ERROR_ALREADY_INITIALIZED) { log("Disabled: hook initialization failed (%d).",status); return 0; }
    status=MH_CreateHook(reinterpret_cast<void*>(addresses.hud),reinterpret_cast<void*>(&hooked_hud),reinterpret_cast<void**>(&originalHud));
    if (status!=MH_OK) { log("Disabled: cannot create equipment HUD hook (%d).",status); return 0; }
    // Queue the two core hooks so MinHook suspends/resumes threads once.
    const auto hudTarget=reinterpret_cast<void*>(addresses.hud);
    status=MH_QueueEnableHook(hudTarget);
    if(status!=MH_OK){MH_RemoveHook(hudTarget);log("Disabled: cannot queue HUD hook (%d).",status);return 0;}
    void* rawTarget=nullptr;
    if(auto* rawProc=GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetRawInputData")){
        const auto target=reinterpret_cast<void*>(rawProc);
        const auto created=MH_CreateHook(target,reinterpret_cast<void*>(&hooked_raw_input),reinterpret_cast<void**>(&originalRawInputData));
        if(created==MH_OK){
            if(MH_QueueEnableHook(target)==MH_OK)rawTarget=target;
            else{MH_RemoveHook(target);originalRawInputData=nullptr;log("Raw mouse hook unavailable: queue failed.");}
        }else log("Raw mouse hook unavailable: creation failed (%d).",created);
    }
    status=MH_ApplyQueued();
    if(status!=MH_OK){
        // Applying a batch can fail after some patches were installed. Disable
        // them together, but retain buffers: an in-flight detour can still be
        // about to call its original trampoline even after its target is restored.
        MH_QueueDisableHook(hudTarget);
        if(rawTarget)MH_QueueDisableHook(rawTarget);
        const auto rollback=MH_ApplyQueued();
        enabled.store(false);
        log("Disabled: core hook activation failed (%d), rollback=%d. Trampolines retained for callback safety.",status,rollback);
        return 0;
    }
    log("Ready: Arrow1WheelKey=%ls Arrow2WheelKey=%ls Bolt1WheelKey=%ls Bolt2WheelKey=%ls DiagnosticKey=%ls. Mouse release delay=%ums. Raw mouse=%u.",
        config.wheelText.c_str(),config.otherWheelTexts[0].c_str(),
        config.otherWheelTexts[1].c_str(),config.otherWheelTexts[2].c_str(),config.diagnosticText.c_str(),config.settle,originalRawInputData?1u:0u);
    if(config.controllerEnabled)
        log("Controller wheels: Arrow1=%ls Arrow2=%ls Bolt1=%ls Bolt2=%ls; right-stick deadzone=%u%%.",
            config.controllerWheelTexts[0].c_str(),config.controllerWheelTexts[1].c_str(),
            config.controllerWheelTexts[2].c_str(),config.controllerWheelTexts[3].c_str(),config.controllerDeadzone);
    if (!indicator::install(module,indicatorSettings,log)) log("Visual indicator unavailable; the arrow switcher remains active.");
    return 0;
}
DWORD WINAPI initialize_guarded(void* arg) {
    try { return initialize(arg); }
    catch (...) { enabled.store(false); return 1; }
}
}

extern "C" __declspec(dllexport) bool QuickSwitchInitialize() {
    if (started.exchange(true)) return true;
    HMODULE pinned{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&QuickSwitchInitialize),&pinned)) { started.store(false); return false; }
    auto thread=CreateThread(nullptr,0,initialize_guarded,nullptr,0,nullptr);
    if (!thread) { started.store(false); return false; }
    CloseHandle(thread);
    return true;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID) {
    if (reason==DLL_PROCESS_ATTACH) { module=instance; DisableThreadLibraryCalls(instance); }
    return TRUE;
}
