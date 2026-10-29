#include "../src/config_file.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    using namespace arrow_switch;
    const auto path = std::filesystem::current_path() /
        (L"config-file-test-" + std::to_wstring(GetCurrentProcessId()) + L".ini");
    std::error_code error;
    std::filesystem::remove(path, error);
    assert(config_file::ensure(path));
    assert(std::filesystem::is_regular_file(path));
    for (const auto& setting : default_config::settings) {
        wchar_t actual[260]{};
        GetPrivateProfileStringW(setting.section, setting.key, L"\x01", actual, 260, path.c_str());
        assert(std::wstring(actual) == setting.value);
    }
    assert(GetPrivateProfileIntW(L"Switching", L"CycleSameFamily", -1, path.c_str()) == 0);
    assert(GetPrivateProfileIntW(L"Switching", L"BlockCameraWhileSelecting", -1, path.c_str()) == 1);
    wchar_t value[128]{};
    GetPrivateProfileStringW(L"Meta", L"ConfigVersion", L"", value, 128, path.c_str());
    assert(std::wstring(value) == default_config::version);

    // An older, partial INI gains missing settings without changing user values.
    {
        std::ofstream old(path, std::ios::binary | std::ios::trunc);
        old << "[Switching]\r\nCycleSameFamily=1\r\n"
               "[Controls]\r\nArrow1WheelKey=Ctrl+Q\r\n"
               "[Personal]\r\nMyKey=keep-me\r\n";
    }
    assert(config_file::ensure(path));
    assert(GetPrivateProfileIntW(L"Switching", L"CycleSameFamily", -1, path.c_str()) == 1);
    GetPrivateProfileStringW(L"Controls", L"Arrow1WheelKey", L"", value, 128, path.c_str());
    assert(std::wstring(value) == L"Ctrl+Q");
    GetPrivateProfileStringW(L"Personal", L"MyKey", L"", value, 128, path.c_str());
    assert(std::wstring(value) == L"keep-me");
    assert(GetPrivateProfileIntW(L"Theme", L"CancelRadius", -1, path.c_str()) == 78);
    for (const auto& setting : default_config::settings) {
        if ((std::wstring(setting.section) == L"Switching" && std::wstring(setting.key) == L"CycleSameFamily") ||
            (std::wstring(setting.section) == L"Controls" && std::wstring(setting.key) == L"Arrow1WheelKey"))
            continue;
        wchar_t actual[260]{};
        GetPrivateProfileStringW(setting.section, setting.key, L"\x01", actual, 260, path.c_str());
        assert(std::wstring(actual) == setting.value);
    }
    GetPrivateProfileStringW(L"Meta", L"ConfigVersion", L"", value, 128, path.c_str());
    assert(std::wstring(value) == default_config::version);
    assert(config_file::ensure(path));
    std::filesystem::remove(path);
}
