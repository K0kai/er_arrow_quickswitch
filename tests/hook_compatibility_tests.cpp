#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../third_party/minhook/include/MinHook.h"
extern "C" {
#include "../third_party/minhook/src/buffer.h"
}
#include <atomic>
#include <cassert>
#include <cstdio>
#include <thread>

namespace {
using Function=int(*)(int);
Function original{},secondOriginal{};
__declspec(noinline) int target(int value){volatile int step=value+7;return step*3;}
__declspec(noinline) int second(int value){volatile int step=value+11;return step*5;}
__declspec(noinline) int replacement(int value){return original(value)+1;}
__declspec(noinline) int secondReplacement(int value){return secondOriginal(value)+2;}
MEMORY_BASIC_INFORMATION info(void* address){MEMORY_BASIC_INFORMATION value{};assert(VirtualQuery(address,&value,sizeof(value)));return value;}
void executable_only(Function function){assert(info(reinterpret_cast<void*>(function)).Protect==PAGE_EXECUTE_READ);}
}
int main(){
    // Before any code is emitted, allocations are writable but not executable.
    void* raw=AllocateBuffer(reinterpret_cast<void*>(&target));assert(raw);
    assert(info(raw).Protect==PAGE_READWRITE);FreeBuffer(raw);assert(info(raw).State==MEM_FREE);
    Function volatile invoke=target;
    Function volatile invokeSecond=second;
    assert(invoke(2)==27 && invokeSecond(2)==65);
    assert(MH_Initialize()==MH_OK);
    assert(MH_CreateHook(reinterpret_cast<void*>(&target),reinterpret_cast<void*>(&replacement),reinterpret_cast<void**>(&original))==MH_OK);
    executable_only(original);
    assert(MH_EnableHook(reinterpret_cast<void*>(&target))==MH_OK);
    assert(invoke(2)==28 && original(2)==27);

    // A running original trampoline must stay RX while another hook is created.
    std::atomic<bool> running{true},entered{false};
    std::thread worker([&]{while(running.load()){assert(original(2)==27);entered.store(true);}});
    while(!entered.load())std::this_thread::yield();
    assert(MH_CreateHook(reinterpret_cast<void*>(&second),reinterpret_cast<void*>(&secondReplacement),reinterpret_cast<void**>(&secondOriginal))==MH_OK);
    executable_only(original);executable_only(secondOriginal);
    assert(info(reinterpret_cast<void*>(original)).AllocationBase!=info(reinterpret_cast<void*>(secondOriginal)).AllocationBase);
    assert(MH_EnableHook(reinterpret_cast<void*>(&second))==MH_OK);
    assert(invokeSecond(2)==67 && secondOriginal(2)==65);
    running.store(false);worker.join();

    assert(MH_DisableHook(reinterpret_cast<void*>(&target))==MH_OK);
    assert(invoke(2)==27);
    void* oldBuffer=reinterpret_cast<void*>(original);
    assert(MH_RemoveHook(reinterpret_cast<void*>(&target))==MH_OK);
    assert(info(oldBuffer).State==MEM_FREE);
    executable_only(secondOriginal);assert(invokeSecond(2)==67);
    // Uninitialize restores a remaining active hook and frees its RX page.
    oldBuffer=reinterpret_cast<void*>(secondOriginal);
    assert(MH_Uninitialize()==MH_OK);
    assert(invokeSecond(2)==65 && info(oldBuffer).State==MEM_FREE);

    assert(MH_Initialize()==MH_OK);
    assert(MH_CreateHook(reinterpret_cast<void*>(&target),reinterpret_cast<void*>(&replacement),reinterpret_cast<void**>(&original))==MH_OK);
    executable_only(original);
    assert(MH_CreateHook(reinterpret_cast<void*>(&second),reinterpret_cast<void*>(&secondReplacement),reinterpret_cast<void**>(&secondOriginal))==MH_OK);
    assert(MH_QueueEnableHook(reinterpret_cast<void*>(&target))==MH_OK);
    assert(MH_QueueEnableHook(reinterpret_cast<void*>(&second))==MH_OK);
    assert(invoke(2)==27 && invokeSecond(2)==65); // Queuing must not patch code yet.
    assert(MH_ApplyQueued()==MH_OK && invoke(2)==28 && invokeSecond(2)==67);
    assert(MH_QueueDisableHook(reinterpret_cast<void*>(&target))==MH_OK);
    assert(MH_QueueDisableHook(reinterpret_cast<void*>(&second))==MH_OK);
    assert(invoke(2)==28 && invokeSecond(2)==67);
    assert(MH_ApplyQueued()==MH_OK && invoke(2)==27 && invokeSecond(2)==65);
    // A restored target does not make its original trampoline safe to free yet.
    executable_only(original);executable_only(secondOriginal);
    assert(original(2)==27 && secondOriginal(2)==65);
    assert(MH_Uninitialize()==MH_OK);
    std::puts("Hook memory: RW construction, RX trampolines, CFG calls, concurrent originals, independent pages, removal, queued enable and cleanup passed.");
}
