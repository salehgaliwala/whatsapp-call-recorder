#ifndef CONFIG_H
#define CONFIG_H

#include "core/Types.h"
#include <mutex>

namespace recorder {

class Config {
public:
    static Config& Instance();

    AppConfig Get() const;
    void Update(const AppConfig& newConfig);

    std::string GetDefaultOutputDir() const;
    void LoadFromFile(const std::string& path);
    void SaveToFile(const std::string& path) const;

private:
    Config();
    mutable std::mutex mutex_;
    AppConfig config_;
};

} // namespace recorder

#endif // CONFIG_H
