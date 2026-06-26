#pragma once

#include <string>
#include <vector>

namespace config
{
    constexpr int k_version = 1;

    std::string config_dir();
    std::string profile_path(const char* name);

    std::vector<std::string> list_profiles();

    bool save(const char* name);
    bool load(const char* name);
    bool remove(const char* name);
    bool create(const char* name);

    bool sanitize_name(std::string& name);
}
