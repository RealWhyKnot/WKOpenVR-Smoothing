#include "Configuration.h"

#include <picojson.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>

namespace fs_config
{

namespace {

protocol::SmoothingConfig Defaults()
{
    protocol::SmoothingConfig cfg{};
    cfg.master_enabled    = false;          // opt-in by default
    cfg.mincutoff         = 1.0f;           // One-Euro paper defaults
    cfg.beta              = 0.007f;
    cfg.dcutoff           = 1.0f;
    cfg.finger_mask       = protocol::kAllFingersMask;
    cfg.adaptive_enabled  = false;
    return cfg;
}

std::string ConfigPath()
{
    return AppDataDir() + "\\config.json";
}

} // anonymous

std::string AppDataDir()
{
    PWSTR roamingPath = nullptr;
    std::string out;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roamingPath))) {
        // Convert wide path to UTF-8 narrow.
        int needed = WideCharToMultiByte(CP_UTF8, 0, roamingPath, -1, nullptr, 0, nullptr, nullptr);
        if (needed > 0) {
            std::string narrow(needed - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, roamingPath, -1, narrow.data(), needed, nullptr, nullptr);
            out = narrow + "\\FingerSmoothing";
        }
        CoTaskMemFree(roamingPath);
    }
    if (out.empty()) {
        // Fallback: cwd. Should never happen on a sane Windows install.
        out = ".\\FingerSmoothing";
    }

    std::error_code ec;
    std::filesystem::create_directories(out, ec);
    return out;
}

protocol::SmoothingConfig Load()
{
    auto cfg = Defaults();

    std::ifstream f(ConfigPath());
    if (!f.is_open()) return cfg;

    std::stringstream buf;
    buf << f.rdbuf();

    picojson::value v;
    std::string err = picojson::parse(v, buf.str());
    if (!err.empty() || !v.is<picojson::object>()) {
        // Malformed JSON — fall through to defaults silently. The next Save()
        // will overwrite with a clean file.
        return cfg;
    }

    const auto &obj = v.get<picojson::object>();
    auto getBool   = [&](const char *k, bool def)  { auto it = obj.find(k); return (it != obj.end() && it->second.is<bool>())   ? it->second.get<bool>()   : def; };
    auto getDouble = [&](const char *k, double def){ auto it = obj.find(k); return (it != obj.end() && it->second.is<double>()) ? it->second.get<double>() : def; };

    cfg.master_enabled    = getBool("master_enabled",   cfg.master_enabled);
    cfg.mincutoff         = (float)getDouble("mincutoff",        cfg.mincutoff);
    cfg.beta              = (float)getDouble("beta",             cfg.beta);
    cfg.dcutoff           = (float)getDouble("dcutoff",          cfg.dcutoff);
    cfg.finger_mask       = (uint16_t)getDouble("finger_mask",   cfg.finger_mask);
    cfg.adaptive_enabled  = getBool("adaptive_enabled", cfg.adaptive_enabled);
    return cfg;
}

void Save(const protocol::SmoothingConfig &cfg)
{
    picojson::object obj;
    obj["master_enabled"]   = picojson::value(cfg.master_enabled);
    obj["mincutoff"]        = picojson::value((double)cfg.mincutoff);
    obj["beta"]             = picojson::value((double)cfg.beta);
    obj["dcutoff"]          = picojson::value((double)cfg.dcutoff);
    obj["finger_mask"]      = picojson::value((double)cfg.finger_mask);
    obj["adaptive_enabled"] = picojson::value(cfg.adaptive_enabled);

    std::ofstream f(ConfigPath(), std::ios::trunc);
    if (!f.is_open()) {
        fprintf(stderr, "[Configuration] failed to open %s for write\n", ConfigPath().c_str());
        return;
    }
    f << picojson::value(obj).serialize(true);
}

} // namespace fs_config
