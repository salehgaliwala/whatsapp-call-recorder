#include "core/Config.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#endif

namespace recorder {

namespace fs = std::filesystem;

Config& Config::Instance() {
    static Config instance;
    return instance;
}

Config::Config() {
    config_.outputDirectory = GetDefaultOutputDir();
    config_.preferredMicDeviceId = "default";
    config_.debounceTimeSeconds = 2.5;
    config_.targetSampleRate = 48000;
    config_.targetChannels = 2;
    config_.targetBitDepth = 16;
    config_.aacBitrateKbps = 160;
}

std::string Config::GetDefaultOutputDir() const {
#if defined(_WIN32)
    PWSTR pathPtr = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, 0, NULL, &pathPtr))) {
        std::wstring ws(pathPtr);
        CoTaskMemFree(pathPtr);
        fs::path p(ws);
        p /= "Recordings";
        p /= "WhatsApp";
        return p.string();
    }
#endif
    const char* userProfile = std::getenv("USERPROFILE");
    if (!userProfile) {
        userProfile = std::getenv("HOME");
    }
    if (userProfile) {
        fs::path p(userProfile);
        p /= "Recordings";
        p /= "WhatsApp";
        return p.string();
    }
    return fs::current_path().string();
}

AppConfig Config::Get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void Config::Update(const AppConfig& newConfig) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = newConfig;
}

void Config::LoadFromFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream file(path);
    if (!file.is_open()) return;
    // Simple key-value loading if needed or json
    std::string line;
    while (std::getline(file, line)) {
        auto pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string val = line.substr(pos + 1);
            if (key == "outputDirectory") config_.outputDirectory = val;
            else if (key == "preferredMicDeviceId") config_.preferredMicDeviceId = val;
            else if (key == "debounceTimeSeconds") config_.debounceTimeSeconds = std::stod(val);
        }
    }
}

void Config::SaveToFile(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream file(path);
    if (!file.is_open()) return;
    file << "outputDirectory=" << config_.outputDirectory << "\n";
    file << "preferredMicDeviceId=" << config_.preferredMicDeviceId << "\n";
    file << "debounceTimeSeconds=" << config_.debounceTimeSeconds << "\n";
}

} // namespace recorder
