#include "config.h"
#include "../menu/menu_vars.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cctype>
#include <cstdio>
#include <cstring>

namespace
{
    std::string strip_newline(char* line)
    {
        size_t len = std::strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        return line;
    }

    bool ensure_directory(const std::string& path)
    {
        if (path.empty())
            return false;

        if (CreateDirectoryA(path.c_str(), nullptr))
            return true;

        const DWORD err = GetLastError();
        return err == ERROR_ALREADY_EXISTS;
    }

    std::string game_root()
    {
        char path[MAX_PATH]{};
        if (!GetModuleFileNameA(nullptr, path, MAX_PATH))
            return {};

        char* slash = std::strrchr(path, '\\');
        if (slash)
            *slash = '\0';

        return path;
    }

    std::string cstrike_dir()
    {
        const std::string root = game_root();
        if (root.empty())
            return {};

        const std::string nested = root + "\\cstrike";
        const DWORD attrs = GetFileAttributesA(nested.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY))
            return nested;

        const std::string lower = root;
        if (lower.size() >= 7)
        {
            const std::string tail = lower.substr(lower.size() - 7);
            if (_stricmp(tail.c_str(), "cstrike") == 0)
                return root;
        }

        return nested;
    }

    bool parse_bool(const char* value, bool& out)
    {
        if (!value || !*value)
            return false;

        if (std::strcmp(value, "1") == 0 || _stricmp(value, "true") == 0)
        {
            out = true;
            return true;
        }

        if (std::strcmp(value, "0") == 0 || _stricmp(value, "false") == 0)
        {
            out = false;
            return true;
        }

        return false;
    }

    bool parse_int(const char* value, int& out)
    {
        if (!value || !*value)
            return false;

        char* end = nullptr;
        const long v = std::strtol(value, &end, 10);
        if (end == value)
            return false;

        out = static_cast<int>(v);
        return true;
    }

    bool parse_uint(const char* value, uint32_t& out)
    {
        if (!value || !*value)
            return false;

        char* end = nullptr;
        const unsigned long v = std::strtoul(value, &end, 10);
        if (end == value)
            return false;

        out = static_cast<uint32_t>(v);
        return true;
    }

    bool parse_float(const char* value, float& out)
    {
        if (!value || !*value)
            return false;

        char* end = nullptr;
        const float v = std::strtof(value, &end);
        if (end == value)
            return false;

        out = v;
        return true;
    }

    bool parse_color(const char* value, float out[4])
    {
        if (!value || !*value)
            return false;

        return sscanf_s(value, "%f,%f,%f,%f", &out[0], &out[1], &out[2], &out[3]) == 4;
    }

    void apply_value(const char* key, const char* value)
    {
        if (!key || !value)
            return;

        bool b = false;
        int i = 0;
        uint32_t u = 0;
        float f = 0.f;

        if (std::strcmp(key, "visuals_enabled") == 0 && parse_bool(value, b)) menu_vars::visuals_enabled = b;
        else if (std::strcmp(key, "name_esp") == 0 && parse_bool(value, b)) menu_vars::name_esp = b;
        else if (std::strcmp(key, "weapon_esp") == 0 && parse_bool(value, b)) menu_vars::weapon_esp = b;
        else if (std::strcmp(key, "skeleton_esp") == 0 && parse_bool(value, b)) menu_vars::skeleton_esp = b;
        else if (std::strcmp(key, "box_esp") == 0 && parse_bool(value, b)) menu_vars::box_esp = b;
        else if (std::strcmp(key, "teammates") == 0 && parse_bool(value, b)) menu_vars::teammates = b;
        else if (std::strcmp(key, "chams") == 0 && parse_bool(value, b)) menu_vars::chams = b;
        else if (std::strcmp(key, "chams_xqz") == 0 && parse_bool(value, b)) menu_vars::chams_xqz = b;
        else if (std::strcmp(key, "chams_vis_color") == 0) parse_color(value, menu_vars::chams_vis_color);
        else if (std::strcmp(key, "chams_xqz_color") == 0) parse_color(value, menu_vars::chams_xqz_color);
        else if (std::strcmp(key, "visual_removals") == 0 && parse_uint(value, u)) menu_vars::visual_removals = u;
        else if (std::strcmp(key, "thirdperson_enabled") == 0 && parse_bool(value, b)) menu_vars::thirdperson_enabled = b;
        else if (std::strcmp(key, "thirdperson_key") == 0 && parse_int(value, i)) menu_vars::thirdperson_key = i;
        else if (std::strcmp(key, "thirdperson_key_mode") == 0 && parse_int(value, i)) menu_vars::thirdperson_key_mode = i;
        else if (std::strcmp(key, "norecoil_enabled") == 0 && parse_bool(value, b)) menu_vars::norecoil_enabled = b;
        else if (std::strcmp(key, "bhop_enabled") == 0 && parse_bool(value, b)) menu_vars::bhop_enabled = b;
        else if (std::strcmp(key, "aimbot_enabled") == 0 && parse_bool(value, b)) menu_vars::aimbot_enabled = b;
        else if (std::strcmp(key, "aimbot_key") == 0 && parse_int(value, i)) menu_vars::aimbot_key = i;
        else if (std::strcmp(key, "aimbot_key_mode") == 0 && parse_int(value, i)) menu_vars::aimbot_key_mode = i;
        else if (std::strcmp(key, "aimbot_fov") == 0 && parse_int(value, i)) menu_vars::aimbot_fov = i;
        else if (std::strcmp(key, "aimbot_fov_circle") == 0 && parse_bool(value, b)) menu_vars::aimbot_fov_circle = b;
        else if (std::strcmp(key, "aimbot_smooth") == 0 && parse_float(value, f)) menu_vars::aimbot_smooth = f;
        else if (std::strcmp(key, "aimbot_hitboxes") == 0 && parse_uint(value, u)) menu_vars::aimbot_hitboxes = u;
        else if (std::strcmp(key, "antiaim_enabled") == 0 && parse_bool(value, b)) menu_vars::antiaim_enabled = b;
        else if (std::strcmp(key, "antiaim_yaw_mode") == 0 && parse_int(value, i)) menu_vars::antiaim_yaw_mode = i;
        else if (std::strcmp(key, "antiaim_pitch_mode") == 0 && parse_int(value, i)) menu_vars::antiaim_pitch_mode = i;
        else if (std::strcmp(key, "antiaim_spin_speed") == 0 && parse_float(value, f)) menu_vars::antiaim_spin_speed = f;
        else if (std::strcmp(key, "ragebot_enabled") == 0 && parse_bool(value, b)) menu_vars::ragebot_enabled = b;
        else if (std::strcmp(key, "ragebot_key") == 0 && parse_int(value, i)) menu_vars::ragebot_key = i;
        else if (std::strcmp(key, "ragebot_key_mode") == 0 && parse_int(value, i)) menu_vars::ragebot_key_mode = i;
        else if (std::strcmp(key, "ragebot_auto_fire") == 0 && parse_bool(value, b)) menu_vars::ragebot_auto_fire = b;
        else if (std::strcmp(key, "ragebot_auto_stop") == 0 && parse_bool(value, b)) menu_vars::ragebot_auto_stop = b;
        else if (std::strcmp(key, "ragebot_autowall") == 0 && parse_bool(value, b)) menu_vars::ragebot_autowall = b;
        else if (std::strcmp(key, "ragebot_hitboxes") == 0 && parse_uint(value, u)) menu_vars::ragebot_hitboxes = u;
    }

    bool write_settings(FILE* file)
    {
        if (!file)
            return false;

        std::fprintf(file, "version=%d\n", config::k_version);

        std::fprintf(file, "visuals_enabled=%d\n", menu_vars::visuals_enabled ? 1 : 0);
        std::fprintf(file, "box_esp=%d\n", menu_vars::box_esp ? 1 : 0);
        std::fprintf(file, "name_esp=%d\n", menu_vars::name_esp ? 1 : 0);
        std::fprintf(file, "weapon_esp=%d\n", menu_vars::weapon_esp ? 1 : 0);
        std::fprintf(file, "skeleton_esp=%d\n", menu_vars::skeleton_esp ? 1 : 0);
        std::fprintf(file, "teammates=%d\n", menu_vars::teammates ? 1 : 0);

        std::fprintf(file, "chams=%d\n", menu_vars::chams ? 1 : 0);
        std::fprintf(file, "chams_xqz=%d\n", menu_vars::chams_xqz ? 1 : 0);
        std::fprintf(file, "chams_vis_color=%.0f,%.0f,%.0f,%.0f\n",
            menu_vars::chams_vis_color[0], menu_vars::chams_vis_color[1],
            menu_vars::chams_vis_color[2], menu_vars::chams_vis_color[3]);
        std::fprintf(file, "chams_xqz_color=%.0f,%.0f,%.0f,%.0f\n",
            menu_vars::chams_xqz_color[0], menu_vars::chams_xqz_color[1],
            menu_vars::chams_xqz_color[2], menu_vars::chams_xqz_color[3]);

        std::fprintf(file, "visual_removals=%u\n", menu_vars::visual_removals);
        std::fprintf(file, "thirdperson_enabled=%d\n", menu_vars::thirdperson_enabled ? 1 : 0);
        std::fprintf(file, "thirdperson_key=%d\n", menu_vars::thirdperson_key);
        std::fprintf(file, "thirdperson_key_mode=%d\n", menu_vars::thirdperson_key_mode);
        std::fprintf(file, "norecoil_enabled=%d\n", menu_vars::norecoil_enabled ? 1 : 0);
        std::fprintf(file, "bhop_enabled=%d\n", menu_vars::bhop_enabled ? 1 : 0);

        std::fprintf(file, "aimbot_enabled=%d\n", menu_vars::aimbot_enabled ? 1 : 0);
        std::fprintf(file, "aimbot_key=%d\n", menu_vars::aimbot_key);
        std::fprintf(file, "aimbot_key_mode=%d\n", menu_vars::aimbot_key_mode);
        std::fprintf(file, "aimbot_fov=%d\n", menu_vars::aimbot_fov);
        std::fprintf(file, "aimbot_fov_circle=%d\n", menu_vars::aimbot_fov_circle ? 1 : 0);
        std::fprintf(file, "aimbot_smooth=%.6g\n", menu_vars::aimbot_smooth);
        std::fprintf(file, "aimbot_hitboxes=%u\n", menu_vars::aimbot_hitboxes);

        std::fprintf(file, "antiaim_enabled=%d\n", menu_vars::antiaim_enabled ? 1 : 0);
        std::fprintf(file, "antiaim_yaw_mode=%d\n", menu_vars::antiaim_yaw_mode);
        std::fprintf(file, "antiaim_pitch_mode=%d\n", menu_vars::antiaim_pitch_mode);
        std::fprintf(file, "antiaim_spin_speed=%.6g\n", menu_vars::antiaim_spin_speed);

        std::fprintf(file, "ragebot_enabled=%d\n", menu_vars::ragebot_enabled ? 1 : 0);
        std::fprintf(file, "ragebot_key=%d\n", menu_vars::ragebot_key);
        std::fprintf(file, "ragebot_key_mode=%d\n", menu_vars::ragebot_key_mode);
        std::fprintf(file, "ragebot_auto_fire=%d\n", menu_vars::ragebot_auto_fire ? 1 : 0);
        std::fprintf(file, "ragebot_auto_stop=%d\n", menu_vars::ragebot_auto_stop ? 1 : 0);
        std::fprintf(file, "ragebot_autowall=%d\n", menu_vars::ragebot_autowall ? 1 : 0);
        std::fprintf(file, "ragebot_hitboxes=%u\n", menu_vars::ragebot_hitboxes);

        return true;
    }
}

namespace config
{
    std::string config_dir()
    {
        const std::string cstrike = cstrike_dir();
        if (cstrike.empty())
            return {};

        const std::string dir = cstrike + "\\semtex";
        ensure_directory(dir);
        return dir;
    }

    std::string profile_path(const char* name)
    {
        if (!name || !*name)
            return {};

        const std::string dir = config_dir();
        if (dir.empty())
            return {};

        return dir + "\\" + name + ".cfg";
    }

    std::vector<std::string> list_profiles()
    {
        std::vector<std::string> profiles;
        const std::string dir = config_dir();
        if (dir.empty())
            return profiles;

        WIN32_FIND_DATAA find_data{};
        const std::string pattern = dir + "\\*.cfg";
        const HANDLE handle = FindFirstFileA(pattern.c_str(), &find_data);
        if (handle == INVALID_HANDLE_VALUE)
            return profiles;

        do
        {
            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;

            std::string name = find_data.cFileName;
            const size_t ext = name.rfind(".cfg");
            if (ext != std::string::npos)
                name = name.substr(0, ext);

            profiles.push_back(name);
        } while (FindNextFileA(handle, &find_data));

        FindClose(handle);
        return profiles;
    }

    bool sanitize_name(std::string& name)
    {
        while (!name.empty() && std::isspace(static_cast<unsigned char>(name.front())))
            name.erase(name.begin());

        while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back())))
            name.pop_back();

        if (name.empty())
            return false;

        for (char c : name)
        {
            if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
                return false;
        }

        return true;
    }

    bool save(const char* name)
    {
        std::string profile = name ? name : "";
        if (!sanitize_name(profile))
            return false;

        const std::string path = profile_path(profile.c_str());
        if (path.empty())
            return false;

        FILE* file = nullptr;
        if (fopen_s(&file, path.c_str(), "w") != 0 || !file)
            return false;

        const bool ok = write_settings(file);
        std::fclose(file);
        return ok;
    }

    bool load(const char* name)
    {
        std::string profile = name ? name : "";
        if (!sanitize_name(profile))
            return false;

        const std::string path = profile_path(profile.c_str());
        if (path.empty())
            return false;

        FILE* file = nullptr;
        if (fopen_s(&file, path.c_str(), "r") != 0 || !file)
            return false;

        char line[512];
        while (std::fgets(line, sizeof(line), file))
        {
            strip_newline(line);

            if (!line[0] || line[0] == '#' || line[0] == ';')
                continue;

            char* eq = std::strchr(line, '=');
            if (!eq)
                continue;

            *eq = '\0';
            const char* key = line;
            const char* value = eq + 1;

            if (std::strcmp(key, "version") == 0)
                continue;

            apply_value(key, value);
        }

        std::fclose(file);
        return true;
    }

    bool remove(const char* name)
    {
        std::string profile = name ? name : "";
        if (!sanitize_name(profile))
            return false;

        const std::string path = profile_path(profile.c_str());
        if (path.empty())
            return false;

        return DeleteFileA(path.c_str()) != 0;
    }

    bool create(const char* name)
    {
        std::string profile = name ? name : "";
        if (!sanitize_name(profile))
            return false;

        const std::string path = profile_path(profile.c_str());
        if (path.empty())
            return false;

        const DWORD attrs = GetFileAttributesA(path.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES)
            return false;

        FILE* file = nullptr;
        if (fopen_s(&file, path.c_str(), "w") != 0 || !file)
            return false;

        const bool ok = write_settings(file);
        std::fclose(file);
        return ok;
    }
}
