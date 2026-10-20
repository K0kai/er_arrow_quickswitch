Pinned source: https://github.com/ocornut/imgui/tree/v1.91.9b

Dear ImGui core, Win32 input, and Direct3D 12 rendering are statically linked.
See LICENSE.txt. A local DX12 pixel shader change handles HDR10/scRGB panel
colors. A small Win32 backend extension disables OS cursor polling when raw
input supplies the settings cursor, preventing game cursor warps from moving
the panel pointer. Other backend code follows the pinned upstream version.
