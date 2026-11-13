// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] Key name <-> Windows VK and [Controls] action table for live INI reload.
#pragma once

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "hooks/input/vk_codes.h"
#include "runtime/las/input_bindings.h"
#include "additions/config/ac_ini_store.h"

namespace ac::config {

struct ControlActionMap {
    const char* ini_name;
    int action_index;
};

constexpr int kVkAlphabetStart = 'A';
constexpr int kVkAlphabetEnd   = 'Z';
constexpr int kVkDigitStart    = '0';
constexpr int kVkDigitEnd      = '9';
constexpr int kVkMinDecimal    = 10;
constexpr int kVkMaxCode       = 255;
constexpr int kFnKeyCount      = 12;

inline const ControlActionMap kControlActionMap[] = {
    {"Button_A",      IB_A},
    {"Button_B",      IB_B},
    {"Button_X",      IB_X},
    {"Button_Y",      IB_Y},
    {"Stick_L_Click", IB_LSTICK},
    {"Stick_R_Click", IB_RSTICK},
    {"Button_L",      IB_L},
    {"Button_R",      IB_R},
    {"Button_ZL",     IB_ZL},
    {"Button_ZR",     IB_ZR},
    {"Button_Plus",   IB_PLUS},
    {"Button_Minus",  IB_MINUS},
    {"DPad_Left",     IB_DLEFT},
    {"DPad_Up",       IB_DUP},
    {"DPad_Right",    IB_DRIGHT},
    {"DPad_Down",     IB_DDOWN},
    {"Stick_L_Left",  IB_LLEFT},
    {"Stick_L_Up",    IB_LUP},
    {"Stick_L_Right", IB_LRIGHT},
    {"Stick_L_Down",  IB_LDOWN},
    {"Stick_R_Left",  IB_RLEFT},
    {"Stick_R_Up",    IB_RUP},
    {"Stick_R_Right", IB_RRIGHT},
    {"Stick_R_Down",  IB_RDOWN},
};

inline bool EqualsIgnoreCase(const std::string& a, const char* b) {
    if (!b || a.size() != std::strlen(b)) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

inline int LookupNamedVk(const std::string& key) {
    struct NamedVk {
        const char* name;
        int vk;
    };
    static const NamedVk kTable[] = {
        {"Space", VK_SPACE},
        {"Shift", VK_SHIFT},
        {"LeftShift", VK_SHIFT},
        {"LShift", VK_SHIFT},
        {"RightShift", VK_SHIFT},
        {"RShift", VK_SHIFT},
        {"Ctrl", VK_CONTROL},
        {"Control", VK_CONTROL},
        {"LeftCtrl", VK_CONTROL},
        {"LCtrl", VK_CONTROL},
        {"RightCtrl", VK_CONTROL},
        {"RCtrl", VK_CONTROL},
        {"Alt", VK_MENU},
        {"Menu", VK_MENU},
        {"LeftAlt", VK_MENU},
        {"LAlt", VK_MENU},
        {"RightAlt", VK_MENU},
        {"RAlt", VK_MENU},
        {"Tab", VK_TAB},
        {"Escape", VK_ESCAPE},
        {"Esc", VK_ESCAPE},
        {"Return", VK_RETURN},
        {"Enter", VK_RETURN},
        {"Backspace", VK_BACK},
        {"Back", VK_BACK},
        {"Insert", VK_INSERT},
        {"Delete", VK_DELETE},
        {"Del", VK_DELETE},
        {"Home", VK_HOME},
        {"End", VK_END},
        {"PageUp", VK_PRIOR},
        {"PgUp", VK_PRIOR},
        {"PageDown", VK_NEXT},
        {"PgDn", VK_NEXT},
        {"CapsLock", VK_CAPITAL},
        {"Caps", VK_CAPITAL},
        {"Up", VK_UP},
        {"Down", VK_DOWN},
        {"Left", VK_LEFT},
        {"Right", VK_RIGHT},
        {"Mouse_Left", IB_VK_LBUTTON},
        {"Mouse_Right", IB_VK_RBUTTON},
        {"Mouse_Middle", IB_VK_MBUTTON},
        {"Mouse_Back", IB_VK_XBUTTON1},
        {"Mouse_X1", IB_VK_XBUTTON1},
        {"Mouse_Forward", IB_VK_XBUTTON2},
        {"Mouse_X2", IB_VK_XBUTTON2},
        {"Wheel_Up", IB_VK_WHEEL_UP},
        {"Mouse_Wheel_Up", IB_VK_WHEEL_UP},
        {"Wheel_Down", IB_VK_WHEEL_DOWN},
        {"Mouse_Wheel_Down", IB_VK_WHEEL_DOWN},
    };
    for (const auto& entry : kTable) {
        if (EqualsIgnoreCase(key, entry.name)) {
            return entry.vk;
        }
    }
    return -1;
}

inline int KeyNameToVk(const std::string& key, bool* out_valid) {
    if (out_valid) {
        *out_valid = true;
    }
    if (key.empty() || EqualsIgnoreCase(key, "None") || EqualsIgnoreCase(key, "Unbound")) {
        return 0;
    }
    if (key.size() == 1) {
        char up = static_cast<char>(std::toupper(static_cast<unsigned char>(key[0])));
        if ((up >= kVkAlphabetStart && up <= kVkAlphabetEnd) ||
            (up >= kVkDigitStart && up <= kVkDigitEnd)) {
            return static_cast<int>(up);
        }
        if (key[0] == '0') {
            return 0;
        }
    }
    int named = LookupNamedVk(key);
    if (named >= 0) {
        return named;
    }
    if ((key[0] == 'F' || key[0] == 'f') && key.size() >= 2 && key.size() <= 3) {
        char* end = nullptr;
        long fn = std::strtol(key.c_str() + 1, &end, 10);
        if (end && *end == '\0' && fn >= 1 && fn <= kFnKeyCount) {
            return VK_F1 + static_cast<int>(fn - 1);
        }
    }
    char* end = nullptr;
    long num = std::strtol(key.c_str(), &end, 10);
    if (end != key.c_str() && *end == '\0' && num >= kVkMinDecimal && num <= kVkMaxCode) {
        return static_cast<int>(num);
    }
    if (out_valid) {
        *out_valid = false;
    }
    return 0;
}

inline void BuildEffectiveKeyMap(const std::vector<IniEntry>& entries,
                                 uint8_t out_map[2][IB_ACTIONS]) {
    ib_load(out_map);
    for (const auto& m : kControlActionMap) {
        const std::string* val = FindIniValue(entries, "Controls", m.ini_name);
        if (!val) {
            continue;
        }
        bool valid = false;
        int vk = KeyNameToVk(*val, &valid);
        if (valid) {
            out_map[0][m.action_index] = static_cast<uint8_t>(vk);
        }
    }
}

} // namespace ac::config
