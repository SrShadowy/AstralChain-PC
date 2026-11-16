// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] INI and environment parsing and formatting helpers.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "additions/config/ac_ini_store.h"
#include "additions/config/ac_key_map.h"

namespace ac::config {


constexpr const char* kDefaultRpcId    = "1568955734606086196";

constexpr int kDefaultWinWidth         = 1600;
constexpr int kDefaultWinHeight        = 900;
constexpr int kMinWinDim               = 320;
constexpr int kMaxWinDim               = 7680;
constexpr int kDefaultFpsLimit         = 60;
constexpr int kMaxFpsLimit             = 1000;
constexpr float kMinRenderScale        = 0.50f;
constexpr float kMaxRenderScale        = 2.00f;
constexpr float kMinFsrSharpness       = 0.00f;
constexpr float kMaxFsrSharpness       = 1.00f;
constexpr float kDefaultFsrSharpness   = 0.80f;
constexpr float kMaxLodBias            = 3.00f;
constexpr float kMaxLodMin             = 4.00f;
constexpr int kDefaultAudioVolume      = 100;
constexpr double kMouseSensPercentDiv  = 100.0;
constexpr double kDefaultMouseSensPct  = 100.0;
constexpr double kMinMouseSensPct      = 5.0;
constexpr double kMaxMouseSensPct      = 4000.0;
constexpr double kMinMouseYScale       = 0.05;
constexpr double kMaxMouseYScale       = 10.0;
constexpr double kCurveLinearExp       = 1.0;
constexpr double kCurvePowerExp        = 1.35;
constexpr double kCurveAggressiveExp   = 1.75;
constexpr double kMinMouseCurve        = 0.2;
constexpr double kMaxMouseCurve        = 5.0;
constexpr double kCurveMatchEps        = 0.02;
constexpr float kMinMenuCell           = 10.0f;
constexpr float kMaxMenuCell           = 200.0f;
constexpr int kCutsceneBaseFps         = 30;
constexpr int kPerfHudLegacyCompact    = 1;
constexpr int kPerfHudLegacyDetailed   = 2;
constexpr int kPerfHudLegacyMinimal    = 3;

constexpr const char* kPmMailbox       = "mailbox";
constexpr const char* kPmFifo          = "fifo";
constexpr const char* kPmFifoRelaxed   = "fifo_relaxed";
constexpr const char* kPmImmediate     = "immediate";

struct SyncState {
    int win_w = kDefaultWinWidth;
    int win_h = kDefaultWinHeight;
    bool boot_async_env = false;
};

inline bool EnvNonEmpty(const char* name) {
    const char* v = std::getenv(name);
    return v != nullptr && *v != '\0';
}

inline const char* EnvStr(const char* name) {
    const char* v = std::getenv(name);
    return (v && *v) ? v : nullptr;
}

inline const std::string* FindKey(const std::vector<IniEntry>& entries,
                                  const std::string& section,
                                  const std::string& key,
                                  bool allow_global_fallback) {
    const std::string* v = FindIniValue(entries, section, key);
    if (!v && allow_global_fallback) {
        v = FindIniValue(entries, "", key);
    }
    return v;
}

inline bool ParseBoolVal(const std::string& s, bool fallback) {
    if (EqualsIgnoreCase(s, "true") || EqualsIgnoreCase(s, "on") ||
        EqualsIgnoreCase(s, "yes") || s == "1") {
        return true;
    }
    if (EqualsIgnoreCase(s, "false") || EqualsIgnoreCase(s, "off") ||
        EqualsIgnoreCase(s, "no") || s == "0") {
        return false;
    }
    char* end = nullptr;
    long n = std::strtol(s.c_str(), &end, 10);
    return (end != s.c_str()) ? (n != 0) : fallback;
}

inline int ParseIntVal(const std::string& s, int fallback) {
    char* end = nullptr;
    long n = std::strtol(s.c_str(), &end, 10);
    return (end != s.c_str() && *end == '\0') ? static_cast<int>(n) : fallback;
}

inline double ParseDoubleVal(const std::string& s, double fallback) {
    char* end = nullptr;
    double d = std::strtod(s.c_str(), &end);
    return (end != s.c_str() && std::isfinite(d)) ? d : fallback;
}

inline const char* CanonicalPresentMode(const char* raw) {
    if (!raw || !*raw) {
        return kPmMailbox;
    }
    if (std::strcmp(raw, "fifo") == 0) {
        return kPmFifo;
    }
    if (std::strcmp(raw, "immediate") == 0) {
        return kPmImmediate;
    }
    if (std::strcmp(raw, "fifo_relaxed") == 0 || std::strcmp(raw, "fifo-relaxed") == 0) {
        return kPmFifoRelaxed;
    }
    return kPmMailbox;
}

inline std::string FormatCompactDouble(double v) {
    double rounded = std::round(v * 100.0) / 100.0;
    if (std::fabs(rounded) < 1e-6) {
        rounded = 0.0;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.4g", rounded);
    return buf;
}

inline std::string FormatBoolVal(bool v) {
    return v ? "true" : "false";
}

inline double ParseMouseCurve(const char* raw) {
    if (!raw || !*raw) {
        return kCurvePowerExp;
    }
    if (std::strcmp(raw, "linear") == 0 || std::strcmp(raw, "1") == 0) {
        return kCurveLinearExp;
    }
    if (std::strcmp(raw, "power") == 0) {
        return kCurvePowerExp;
    }
    if (std::strcmp(raw, "aggressive") == 0 || std::strcmp(raw, "2") == 0) {
        return kCurveAggressiveExp;
    }
    double d = ParseDoubleVal(raw, kCurvePowerExp);
    return std::clamp(d, kMinMouseCurve, kMaxMouseCurve);
}

} // namespace ac::config
