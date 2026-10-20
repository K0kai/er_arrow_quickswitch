#pragma once
#include <windows.h>
#include <d3d12.h>
#include <filesystem>

namespace arrow_switch::settings_ui {
void set_config(const std::filesystem::path& path);
bool is_open();
void toggle();
void close();
void set_enabled(bool value);
void mouse_input(const RAWMOUSE& mouse);
// Process open/close transitions even when no overlay is visible.
void synchronize();
bool install(HWND window,ID3D12Device* device,ID3D12CommandQueue* queue,
    unsigned frames,DXGI_FORMAT format,ID3D12DescriptorHeap* heap);
void render(ID3D12GraphicsCommandList* commands);
void shutdown();
}
