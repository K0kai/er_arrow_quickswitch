/*
 *  MinHook - The Minimalistic API Hooking Library for x64/x86
 *  Copyright (C) 2009-2017 Tsuda Kageyu.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *  PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
 *  OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <windows.h>
#include "buffer.h"

// Local hardening: one page per trampoline, RW during creation then RX.
// Allocation metadata stays on the non-executable process heap.
#define MEMORY_BLOCK_SIZE 0x1000
#define MAX_MEMORY_RANGE 0x40000000
#define PAGE_EXECUTE_FLAGS \
    (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)

typedef struct _BUFFER_NODE
{
    struct _BUFFER_NODE *next;
    LPVOID address;
} BUFFER_NODE;
static BUFFER_NODE *g_buffers;

VOID InitializeBuffer(VOID) {}
VOID UninitializeBuffer(VOID)
{
    while (g_buffers)
    {
        BUFFER_NODE *node = g_buffers;
        g_buffers = node->next;
        VirtualFree(node->address, 0, MEM_RELEASE);
        HeapFree(GetProcessHeap(), 0, node);
    }
}

//-------------------------------------------------------------------------
#if defined(_M_X64) || defined(__x86_64__)
static LPVOID FindPrevFreeRegion(LPVOID pAddress, LPVOID pMinAddr, DWORD dwAllocationGranularity)
{
    ULONG_PTR tryAddr = (ULONG_PTR)pAddress;

    // Round down to the allocation granularity.
    tryAddr -= tryAddr % dwAllocationGranularity;

    // Start from the previous allocation granularity multiply.
    tryAddr -= dwAllocationGranularity;

    while (tryAddr >= (ULONG_PTR)pMinAddr)
    {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery((LPVOID)tryAddr, &mbi, sizeof(mbi)) == 0)
            break;

        if (mbi.State == MEM_FREE)
            return (LPVOID)tryAddr;

        if ((ULONG_PTR)mbi.AllocationBase < dwAllocationGranularity)
            break;

        tryAddr = (ULONG_PTR)mbi.AllocationBase - dwAllocationGranularity;
    }

    return NULL;
}
#endif

//-------------------------------------------------------------------------
#if defined(_M_X64) || defined(__x86_64__)
static LPVOID FindNextFreeRegion(LPVOID pAddress, LPVOID pMaxAddr, DWORD dwAllocationGranularity)
{
    ULONG_PTR tryAddr = (ULONG_PTR)pAddress;

    // Round down to the allocation granularity.
    tryAddr -= tryAddr % dwAllocationGranularity;

    // Start from the next allocation granularity multiply.
    tryAddr += dwAllocationGranularity;

    while (tryAddr <= (ULONG_PTR)pMaxAddr)
    {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery((LPVOID)tryAddr, &mbi, sizeof(mbi)) == 0)
            break;

        if (mbi.State == MEM_FREE)
            return (LPVOID)tryAddr;

        tryAddr = (ULONG_PTR)mbi.BaseAddress + mbi.RegionSize;

        // Round up to the next allocation granularity.
        tryAddr += dwAllocationGranularity - 1;
        tryAddr -= tryAddr % dwAllocationGranularity;
    }

    return NULL;
}
#endif

static LPVOID AllocatePage(LPVOID origin)
{
    LPVOID address = NULL;
#if defined(_M_X64) || defined(__x86_64__)
    SYSTEM_INFO si;
    ULONG_PTR minAddr, maxAddr;
    LPVOID candidate;
    GetSystemInfo(&si);
    minAddr = (ULONG_PTR)si.lpMinimumApplicationAddress;
    maxAddr = (ULONG_PTR)si.lpMaximumApplicationAddress;
    if ((ULONG_PTR)origin > MAX_MEMORY_RANGE && minAddr < (ULONG_PTR)origin - MAX_MEMORY_RANGE)
        minAddr = (ULONG_PTR)origin - MAX_MEMORY_RANGE;
    if (maxAddr > (ULONG_PTR)origin + MAX_MEMORY_RANGE)
        maxAddr = (ULONG_PTR)origin + MAX_MEMORY_RANGE;
    maxAddr -= MEMORY_BLOCK_SIZE - 1;

    candidate = origin;
    while ((ULONG_PTR)candidate >= minAddr)
    {
        candidate = FindPrevFreeRegion(candidate, (LPVOID)minAddr, si.dwAllocationGranularity);
        if (!candidate) break;
        address = VirtualAlloc(candidate, MEMORY_BLOCK_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (address) break;
    }
    if (!address)
    {
        candidate = origin;
        while ((ULONG_PTR)candidate <= maxAddr)
        {
            candidate = FindNextFreeRegion(candidate, (LPVOID)maxAddr, si.dwAllocationGranularity);
            if (!candidate) break;
            address = VirtualAlloc(candidate, MEMORY_BLOCK_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (address) break;
        }
    }
#else
    address = VirtualAlloc(NULL, MEMORY_BLOCK_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#endif
    return address;
}

LPVOID AllocateBuffer(LPVOID origin)
{
    BUFFER_NODE *node;
    LPVOID address = AllocatePage(origin);
    if (!address) return NULL;
    node = (BUFFER_NODE *)HeapAlloc(GetProcessHeap(), 0, sizeof(*node));
    if (!node)
    {
        VirtualFree(address, 0, MEM_RELEASE);
        return NULL;
    }
    node->address = address;
    node->next = g_buffers;
    g_buffers = node;
    return address;
}

BOOL SealBuffer(LPVOID buffer)
{
    DWORD previous;
    if (!VirtualProtect(buffer, MEMORY_BLOCK_SIZE, PAGE_EXECUTE_READ, &previous))
        return FALSE;
    return FlushInstructionCache(GetCurrentProcess(), buffer, MEMORY_BLOCK_SIZE);
}

VOID FreeBuffer(LPVOID buffer)
{
    BUFFER_NODE **link = &g_buffers;
    while (*link)
    {
        BUFFER_NODE *node = *link;
        if (node->address == buffer)
        {
            *link = node->next;
            VirtualFree(node->address, 0, MEM_RELEASE);
            HeapFree(GetProcessHeap(), 0, node);
            return;
        }
        link = &node->next;
    }
}

BOOL IsExecutableAddress(LPVOID address)
{
    MEMORY_BASIC_INFORMATION info;
    if (!VirtualQuery(address, &info, sizeof(info))) return FALSE;
    return info.State == MEM_COMMIT && (info.Protect & PAGE_EXECUTE_FLAGS);
}
