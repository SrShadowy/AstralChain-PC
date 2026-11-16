#pragma once

#include <QHash>
#include <QString>

namespace ac::launcher {

constexpr int kVkMouseLeft     = 0x01;
constexpr int kVkMouseRight    = 0x02;
constexpr int kVkMouseMiddle   = 0x04;
constexpr int kVkMouseX1       = 0x05;
constexpr int kVkMouseX2       = 0x06;
constexpr int kVkBack          = 0x08;
constexpr int kVkTab           = 0x09;
constexpr int kVkReturn        = 0x0D;
constexpr int kVkWheelUp       = 0x0E;
constexpr int kVkWheelDown     = 0x0F;
constexpr int kVkShift         = 0x10;
constexpr int kVkControl       = 0x11;
constexpr int kVkMenu          = 0x12;
constexpr int kVkCapital       = 0x14;
constexpr int kVkEscape        = 0x1B;
constexpr int kVkSpace         = 0x20;
constexpr int kVkPrior         = 0x21;
constexpr int kVkNext          = 0x22;
constexpr int kVkEnd           = 0x23;
constexpr int kVkHome          = 0x24;
constexpr int kVkLeft          = 0x25;
constexpr int kVkUp            = 0x26;
constexpr int kVkRight         = 0x27;
constexpr int kVkDown          = 0x28;
constexpr int kVkInsert        = 0x2D;
constexpr int kVkDelete        = 0x2E;
constexpr int kVkF1            = 0x70;
constexpr int kMaxFnNumber     = 12;

inline int keyToVk(const QString& key) {
    const QString k = key.trimmed();
    if (k.isEmpty() || k.compare("None", Qt::CaseInsensitive) == 0 ||
        k.compare("Unbound", Qt::CaseInsensitive) == 0) {
        return 0;
    }
    if (k.length() == 1) {
        const char ch = k.at(0).toUpper().toLatin1();
        if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
            return static_cast<int>(ch);
        }
    }
    static const QHash<QString, int> kNamed = {
        {"space", kVkSpace}, {"leftshift", kVkShift}, {"lshift", kVkShift},
        {"rightshift", kVkShift}, {"rshift", kVkShift}, {"shift", kVkShift},
        {"leftctrl", kVkControl}, {"lctrl", kVkControl}, {"rightctrl", kVkControl},
        {"rctrl", kVkControl}, {"ctrl", kVkControl}, {"control", kVkControl},
        {"alt", kVkMenu}, {"menu", kVkMenu}, {"leftalt", kVkMenu}, {"lalt", kVkMenu},
        {"rightalt", kVkMenu}, {"ralt", kVkMenu},
        {"tab", kVkTab}, {"escape", kVkEscape}, {"esc", kVkEscape},
        {"return", kVkReturn}, {"enter", kVkReturn}, {"backspace", kVkBack}, {"back", kVkBack},
        {"insert", kVkInsert}, {"delete", kVkDelete}, {"del", kVkDelete},
        {"home", kVkHome}, {"end", kVkEnd}, {"pageup", kVkPrior}, {"pgup", kVkPrior},
        {"pagedown", kVkNext}, {"pgdn", kVkNext}, {"capslock", kVkCapital}, {"caps", kVkCapital},
        {"up", kVkUp}, {"down", kVkDown}, {"left", kVkLeft}, {"right", kVkRight},
        {"mouse_left", kVkMouseLeft}, {"mouse_right", kVkMouseRight}, {"mouse_middle", kVkMouseMiddle},
        {"mouse_back", kVkMouseX1}, {"mouse_x1", kVkMouseX1},
        {"mouse_forward", kVkMouseX2}, {"mouse_x2", kVkMouseX2},
        {"wheel_up", kVkWheelUp}, {"mouse_wheel_up", kVkWheelUp},
        {"wheel_down", kVkWheelDown}, {"mouse_wheel_down", kVkWheelDown},
    };
    const QString low = k.toLower();
    if (kNamed.contains(low)) {
        return kNamed.value(low);
    }
    if (low.startsWith('f') && low.size() >= 2 && low.size() <= 3) {
        bool ok = false;
        const int fn = low.mid(1).toInt(&ok);
        if (ok && fn >= 1 && fn <= kMaxFnNumber) {
            return kVkF1 + (fn - 1);
        }
    }
    return 0;
}

inline const char* actionToAc(const QString& action) {
    static const QHash<QString, const char*> kMap = {
        {"Button_A", "A"}, {"Button_B", "B"}, {"Button_X", "X"}, {"Button_Y", "Y"},
        {"Button_L", "L"}, {"Button_R", "R"}, {"Button_ZL", "ZL"}, {"Button_ZR", "ZR"},
        {"Button_Plus", "PLUS"}, {"Button_Minus", "MINUS"},
        {"DPad_Up", "DUP"}, {"DPad_Down", "DDOWN"}, {"DPad_Left", "DLEFT"}, {"DPad_Right", "DRIGHT"},
        {"Stick_L_Up", "LUP"}, {"Stick_L_Down", "LDOWN"}, {"Stick_L_Left", "LLEFT"}, {"Stick_L_Right", "LRIGHT"},
        {"Stick_L_Click", "LSTICK"}, {"Stick_R_Click", "RSTICK"},
    };
    return kMap.value(action, nullptr);
}

} // namespace ac::launcher
