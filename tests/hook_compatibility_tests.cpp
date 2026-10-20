#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../third_party/minhook/include/MinHook.h"
#include <cassert>
#include <cstdio>

namespace {
using Function=int(*)(int);
Function original{};
__declspec(noinline) int target(int value){volatile int step=value+7;return step*3;}
__declspec(noinline) int replacement(int value){return original(value)+1;}
}
int main(){
    // Exercise calls into the detour and allocated trampoline with CFG enabled.
    Function volatile invoke=target;
    assert(invoke(2)==27);
    assert(MH_Initialize()==MH_OK);
    assert(MH_CreateHook(reinterpret_cast<void*>(&target),reinterpret_cast<void*>(&replacement),reinterpret_cast<void**>(&original))==MH_OK);
    assert(MH_EnableHook(reinterpret_cast<void*>(&target))==MH_OK);
    assert(invoke(2)==28);
    assert(original(2)==27);
    assert(MH_DisableHook(reinterpret_cast<void*>(&target))==MH_OK);
    assert(invoke(2)==27);
    assert(MH_RemoveHook(reinterpret_cast<void*>(&target))==MH_OK);
    assert(MH_Uninitialize()==MH_OK);
    std::puts("CFG compatibility: detour, original trampoline, disable and cleanup passed.");
}
