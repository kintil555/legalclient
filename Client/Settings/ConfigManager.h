#pragma once
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include "../Core/ModuleManager.h"

namespace Client::Settings {

// Handles persisting every module's config to/from a JSON file under
// configs/. Supports multiple named profiles (default, pvp, survival, ...).
class ConfigManager {
public:
    static ConfigManager& get() {
        static ConfigManager instance;
        return instance;
    }

    void save(const std::string& profileName) const {
        nlohmann::json root;
        for (const auto& mod : Core::ModuleManager::get().modules()) {
            nlohmann::json modJson;
            mod->onConfigSave(modJson);
            root[mod->name()] = modJson;
        }

        std::filesystem::create_directories("configs");
        std::ofstream file("configs/" + profileName);
        file << root.dump(4);
    }

    void load(const std::string& profileName) const {
        const std::string path = "configs/" + profileName;
        if (!std::filesystem::exists(path)) return;

        std::ifstream file(path);
        nlohmann::json root;
        file >> root;

        for (auto& mod : Core::ModuleManager::get().modules()) {
            if (root.contains(mod->name())) {
                mod->onConfigLoad(root.at(mod->name()));
            }
        }
    }
};

} // namespace Client::Settings
