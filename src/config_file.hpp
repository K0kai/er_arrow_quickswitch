#pragma once
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>
#include "default_config.hpp"

namespace arrow_switch::config_file {

// Create a documented file for new installs. On upgrades, add missing keys
// through the Windows INI API so values already chosen by the player survive.
inline bool ensure(const std::filesystem::path& path) {
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) return false;
    if (!exists) {
        std::ofstream output(path, std::ios::binary);
        if (!output) return false;
        output.write(default_config::ini, sizeof(default_config::ini) - 1);
        output.close();
        return output.good();
    }
    if (!std::filesystem::is_regular_file(path, error) || error) return false;
    const auto filename = path.wstring();
    for (const auto& setting : default_config::settings) {
        wchar_t found[2]{};
        GetPrivateProfileStringW(setting.section, setting.key, L"\x01", found, 2, filename.c_str());
        if (found[0] == L'\x01' &&
            !WritePrivateProfileStringW(setting.section, setting.key, setting.value, filename.c_str()))
            return false;
    }
    wchar_t currentVersion[32]{};
    GetPrivateProfileStringW(L"Meta", L"ConfigVersion", L"", currentVersion, 32, filename.c_str());
    return std::wstring(currentVersion) == default_config::version ||
        WritePrivateProfileStringW(L"Meta", L"ConfigVersion", default_config::version,
                                   filename.c_str()) != FALSE;
}

}
