#include "Config.hpp"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <algorithm>

namespace ac::launcher {

namespace {
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

int keyToVk(const QString& key) {
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

const char* actionToAc(const QString& action) {
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
} // namespace

Config::Config(const QString& iniPath)
    : m_iniPath(iniPath) {
    load();
}

void Config::load() {
    QSettings s(m_iniPath, QSettings::IniFormat);

    s.beginGroup("Graphics");
    m_graphics.width = s.value("width", GraphicsSettings::kDefaultWidth).toInt();
    m_graphics.height = s.value("height", GraphicsSettings::kDefaultHeight).toInt();
    m_graphics.fullscreen = s.value("fullscreen", false).toBool();
    m_graphics.vsync = s.value("vsync", true).toBool();
    m_graphics.presentMode = s.value("present_mode", "mailbox").toString();
    m_graphics.fpsLimit = s.value("fps_limit", GraphicsSettings::kDefaultFpsLimit).toInt();
    m_graphics.fpsUnlocked = s.value("fps_unlocked", false).toBool();
    m_graphics.renderScale = s.value("render_scale", GraphicsSettings::kDefaultRenderScale).toDouble();
    m_graphics.upscaler = s.value("upscaler", "fsr3").toString();
    m_graphics.antialias = s.value("antialias", "smaa").toString();
    m_graphics.frameGen = s.value("frame_gen", false).toBool();
    m_graphics.fsrJitter = s.value("fsr_jitter", true).toBool();
    m_graphics.fsrSharpness = s.value("fsr_sharpness", GraphicsSettings::kDefaultFsrSharpness).toDouble();
    m_graphics.lodBias = s.value("lod_bias", GraphicsSettings::kDefaultLodBias).toDouble();
    m_graphics.lodMin = s.value("lod_min", GraphicsSettings::kDefaultLodMin).toDouble();
    m_graphics.noShadow = s.value("no_shadow", false).toBool();
    s.endGroup();

    s.beginGroup("Audio");
    m_audio.masterVolume = s.value("master_volume", AudioSettings::kDefaultVolume).toInt();
    m_audio.muteOnUnfocus = s.value("mute_on_unfocus", false).toBool();
    s.endGroup();

    s.beginGroup("Mouse");
    m_mouse.mouseLook = s.value("mouse_look", MouseSettings::kDefaultMouseLook).toBool();
    m_mouse.sensitivity = s.value("sensitivity", MouseSettings::kDefaultSensitivity).toDouble();
    m_mouse.yScale = s.value("y_scale", MouseSettings::kDefaultYScale).toDouble();
    m_mouse.curve = s.value("curve", MouseSettings::kDefaultCurve).toString();
    m_mouse.invertY = s.value("invert_y", MouseSettings::kDefaultInvertY).toBool();
    m_mouse.menuNav = s.value("menu_nav", MouseSettings::kDefaultMenuNav).toBool();
    m_mouse.menuMotion = s.value("menu_motion", MouseSettings::kDefaultMenuMotion).toBool();
    m_mouse.menuCell = s.value("menu_cell", MouseSettings::kDefaultMenuCell).toInt();
    s.endGroup();

    s.beginGroup("Game");
    m_game.parallelThreads = s.value("parallel_threads", true).toBool();
    m_game.ignoreFocus = s.value("ignore_focus", false).toBool();
    m_game.bc7Precompute = s.value("bc7_precompute", false).toBool();
    m_game.forceShaders = s.value("force_shaders", false).toBool();
    m_game.verboseLog = s.value("verbose_log", false).toBool();
    m_game.cutscene30Fps = s.value("cutscene_30fps", false).toBool();
    m_game.asyncSubmit = s.value("async_submit", true).toBool();
    m_game.xboxConfirmA = s.value("xbox_confirm_a", true).toBool();
    m_game.buttonPrompts = s.value("button_prompts", "auto").toString();
    m_game.language = s.value("language", "auto").toString();
    m_game.voiceLanguage = s.value("voice_language", "auto").toString();
    m_game.commandWrapper = s.value("command_wrapper", "").toString();
    m_game.customEnv = s.value("custom_env", "").toString();
    s.endGroup();

    s.beginGroup("Overlay");
    m_overlay.perfHudEnabled = s.value("perf_hud_enabled", false).toBool();
    m_overlay.perfHudMode = std::clamp(s.value("perf_hud_mode", 0).toInt(), 0, 2);
    m_overlay.perfHudCorner = std::clamp(s.value("perf_hud_corner", OverlaySettings::kDefaultCorner).toInt(), 0, 3);
    m_overlay.perfHudOpacity = std::clamp(
        s.value("perf_hud_opacity", OverlaySettings::kDefaultOpacity).toDouble(),
        OverlaySettings::kMinOpacity, OverlaySettings::kMaxOpacity);
    m_overlay.perfHudShowWithMenu = s.value("perf_hud_show_with_menu", true).toBool();
    s.endGroup();

    s.beginGroup("DiscordRPC");
    m_discordRpc.enabled = s.value("enabled", true).toBool();
    m_discordRpc.showFile = s.value("show_file", true).toBool();
    m_discordRpc.showMission = s.value("show_mission", true).toBool();
    m_discordRpc.showArea = s.value("show_area", true).toBool();
    m_discordRpc.showBoss = s.value("show_boss", true).toBool();
    m_discordRpc.showDifficulty = s.value("show_difficulty", true).toBool();
    m_discordRpc.showTime = s.value("show_time", true).toBool();
    const QString cid = s.value("client_id", DiscordRpcSettings::kDefaultClientId).toString().trimmed();
    m_discordRpc.clientId = cid.isEmpty() ? DiscordRpcSettings::kDefaultClientId : cid;
    s.endGroup();

    m_bindings.clear();
    s.beginGroup("Controls");
    for (const auto& k : s.childKeys()) {
        m_bindings[k] = s.value(k).toString();
    }
    s.endGroup();
}

void Config::save() {
    QFileInfo info(m_iniPath);
    QDir().mkpath(info.absolutePath());
    QSettings s(m_iniPath, QSettings::IniFormat);

    s.beginGroup("Graphics");
    s.setValue("width", m_graphics.width);
    s.setValue("height", m_graphics.height);
    s.setValue("fullscreen", m_graphics.fullscreen);
    s.setValue("vsync", m_graphics.vsync);
    s.setValue("present_mode", m_graphics.presentMode);
    s.setValue("fps_limit", m_graphics.fpsLimit);
    s.setValue("fps_unlocked", m_graphics.fpsUnlocked);
    s.setValue("render_scale", m_graphics.renderScale);
    s.setValue("upscaler", m_graphics.upscaler);
    s.setValue("antialias", m_graphics.antialias);
    s.setValue("frame_gen", m_graphics.frameGen);
    s.setValue("fsr_jitter", m_graphics.fsrJitter);
    s.setValue("fsr_sharpness", m_graphics.fsrSharpness);
    s.setValue("lod_bias", m_graphics.lodBias);
    s.setValue("lod_min", m_graphics.lodMin);
    s.setValue("no_shadow", m_graphics.noShadow);
    s.endGroup();

    s.beginGroup("Audio");
    s.setValue("master_volume", m_audio.masterVolume);
    s.setValue("mute_on_unfocus", m_audio.muteOnUnfocus);
    s.endGroup();

    s.beginGroup("Mouse");
    s.setValue("mouse_look", m_mouse.mouseLook);
    s.setValue("sensitivity", m_mouse.sensitivity);
    s.setValue("y_scale", m_mouse.yScale);
    s.setValue("curve", m_mouse.curve);
    s.setValue("invert_y", m_mouse.invertY);
    s.setValue("menu_nav", m_mouse.menuNav);
    s.setValue("menu_motion", m_mouse.menuMotion);
    s.setValue("menu_cell", m_mouse.menuCell);
    s.endGroup();

    s.beginGroup("Game");
    s.setValue("parallel_threads", m_game.parallelThreads);
    s.setValue("ignore_focus", m_game.ignoreFocus);
    s.setValue("bc7_precompute", m_game.bc7Precompute);
    s.setValue("force_shaders", m_game.forceShaders);
    s.setValue("verbose_log", m_game.verboseLog);
    s.setValue("cutscene_30fps", m_game.cutscene30Fps);
    s.setValue("async_submit", m_game.asyncSubmit);
    s.setValue("xbox_confirm_a", m_game.xboxConfirmA);
    s.setValue("button_prompts", m_game.buttonPrompts);
    s.setValue("language", m_game.language);
    s.setValue("voice_language", m_game.voiceLanguage);
    s.setValue("command_wrapper", m_game.commandWrapper);
    s.setValue("custom_env", m_game.customEnv);
    s.endGroup();

    s.beginGroup("Overlay");
    s.setValue("perf_hud_enabled", m_overlay.perfHudEnabled);
    s.setValue("perf_hud_mode", m_overlay.perfHudMode);
    s.setValue("perf_hud_corner", m_overlay.perfHudCorner);
    s.setValue("perf_hud_opacity", m_overlay.perfHudOpacity);
    s.setValue("perf_hud_show_with_menu", m_overlay.perfHudShowWithMenu);
    s.endGroup();

    s.beginGroup("DiscordRPC");
    s.setValue("enabled", m_discordRpc.enabled);
    s.setValue("show_file", m_discordRpc.showFile);
    s.setValue("show_mission", m_discordRpc.showMission);
    s.setValue("show_area", m_discordRpc.showArea);
    s.setValue("show_boss", m_discordRpc.showBoss);
    s.setValue("show_difficulty", m_discordRpc.showDifficulty);
    s.setValue("show_time", m_discordRpc.showTime);
    s.setValue("client_id", m_discordRpc.clientId.trimmed().isEmpty()
                                  ? QString(DiscordRpcSettings::kDefaultClientId)
                                  : m_discordRpc.clientId.trimmed());
    s.endGroup();

    s.beginGroup("Controls");
    for (const auto& [action, key] : m_bindings) {
        s.setValue(action, key);
    }
    s.endGroup();

    s.sync();
}

QString Config::getBinding(const QString& action, const QString& fallback) const {
    const auto it = m_bindings.find(action);
    return (it != m_bindings.end()) ? it->second : fallback;
}

void Config::setBinding(const QString& action, const QString& key) {
    m_bindings[action] = key;
}

QString Config::buildLaunchEnv() const {
    QStringList env;
    if (m_game.parallelThreads) env << "AC_PARALLEL=1";
    if (m_game.ignoreFocus) env << "AC_IGNORE_FOCUS=1";
    if (m_game.bc7Precompute) {
        env << "AC_BC7_PRECOMPUTE=1";
        env << "LARECOMP_BC7=1";
    }
    if (m_graphics.fullscreen) env << "AC_FULLSCREEN=1";
    if (!m_graphics.upscaler.isEmpty()) env << QString("AC_UPSCALER=%1").arg(m_graphics.upscaler);
    if (!m_graphics.antialias.isEmpty()) env << QString("AC_ANTIALIAS=%1").arg(m_graphics.antialias);
    if (m_graphics.renderScale > 0.0) {
        env << QString("AC_RENDER_SCALE=%1").arg(m_graphics.renderScale, 0, 'f', GraphicsSettings::kFsrSharpnessDecimals);
    }
    if (m_graphics.frameGen) env << "AC_FRAMEGEN=1";
    env << QString("AC_FSR_JITTER=%1").arg(m_graphics.fsrJitter ? 1 : 0);
    env << QString("AC_FSR_SHARPNESS=%1").arg(m_graphics.fsrSharpness, 0, 'f', GraphicsSettings::kFsrSharpnessDecimals);
    if (m_graphics.lodBias > 0.0) env << QString("LARECOMP_LOD_BIAS=%1").arg(m_graphics.lodBias);
    if (m_graphics.lodMin > 0.0) env << QString("LARECOMP_LOD_MIN=%1").arg(m_graphics.lodMin);
    if (m_graphics.noShadow) env << "LARECOMP_NO_SHADOW=1";
    if (m_game.verboseLog) env << "AC_VERBOSE=1";
    if (m_game.cutscene30Fps) env << "AC_CUTSCENE_FPS=30";
    if (m_game.asyncSubmit) env << "AC_ASYNC_SUBMIT=1";
    env << QString("AC_PROMPTS=%1").arg(m_game.buttonPrompts.isEmpty() ? "auto" : m_game.buttonPrompts);
    env << QString("AC_XBOX_CONFIRM_A=%1").arg(m_game.xboxConfirmA ? 1 : 0);
    if (!m_game.language.isEmpty()) {
        env << QString("AC_LANGUAGE=%1").arg(m_game.language);
        env << QString("LARECOMP_LANG=%1").arg(m_game.language);
    }
    if (!m_game.voiceLanguage.isEmpty()) {
        env << QString("AC_VOICE_LANGUAGE=%1").arg(m_game.voiceLanguage);
        env << QString("LARECOMP_VOICE_LANG=%1").arg(m_game.voiceLanguage);
    }
    env << QString("AC_MASTER_VOLUME=%1").arg(m_audio.masterVolume);
    env << QString("AC_MUTE_ON_UNFOCUS=%1").arg(m_audio.muteOnUnfocus ? 1 : 0);
    env << QString("AC_MOUSE_LOOK=%1").arg(m_mouse.mouseLook ? 1 : 0);
    env << QString("AC_MOUSE_SENS=%1").arg(static_cast<int>(m_mouse.sensitivity));
    if (m_mouse.yScale != MouseSettings::kDefaultYScale) env << QString("AC_MOUSE_YSCALE=%1").arg(m_mouse.yScale);
    if (m_mouse.curve != MouseSettings::kDefaultCurve) env << QString("AC_MOUSE_CURVE=%1").arg(m_mouse.curve);
    if (m_mouse.invertY) env << "AC_MOUSE_INVERT=1";
    if (!m_mouse.menuNav) env << "AC_MOUSE_MENUS=0";
    if (!m_mouse.menuMotion) env << "AC_MOUSE_MENU_MOTION=0";
    if (m_mouse.menuCell != MouseSettings::kDefaultMenuCell) env << QString("AC_MOUSE_MENU_CELL=%1").arg(m_mouse.menuCell);
    env << QString("AC_PERF_HUD=%1").arg(m_overlay.perfHudEnabled ? (m_overlay.perfHudMode + 1) : 0);
    env << QString("AC_PERF_HUD_CORNER=%1").arg(m_overlay.perfHudCorner);
    env << QString("AC_PERF_HUD_OPACITY=%1").arg(m_overlay.perfHudOpacity, 0, 'f', 2);
    env << QString("AC_PERF_HUD_WITH_MENU=%1").arg(m_overlay.perfHudShowWithMenu ? 1 : 0);
    env << QString("AC_DISCORD_RPC=%1").arg(m_discordRpc.enabled ? 1 : 0);
    if (!m_discordRpc.clientId.trimmed().isEmpty()) {
        env << QString("AC_DISCORD_CLIENT_ID=%1").arg(m_discordRpc.clientId.trimmed());
    }
    env << "AC_AUDIO_RESYNC=1";
    env << "LARECOMP_VK_LAZY_WB=0";
    QString mode = m_graphics.presentMode.trimmed().toLower();
    if (mode.isEmpty()) mode = m_graphics.vsync ? "fifo" : "mailbox";
    env << QString("LARECOMP_VK_PRESENT=%1").arg(mode);
    env << QString("AC_FPS=%1").arg(m_graphics.fpsLimit);
    const bool effUnlocked = m_graphics.fpsUnlocked || m_graphics.fpsLimit == 0 ||
                             m_graphics.fpsLimit > GraphicsSettings::kDefaultFpsLimit;
    env << QString("AC_FPS_UNLOCKED=%1").arg(effUnlocked ? 1 : 0);
    env << "AC_PRESENT_INTERVAL=1";
    env << QString("AC_WIDTH=%1").arg(m_graphics.width);
    env << QString("AC_HEIGHT=%1").arg(m_graphics.height);
    if (!m_bindings.empty()) {
        QStringList keysList;
        for (const auto& [action, key] : m_bindings) {
            const char* acName = actionToAc(action);
            const int vk = keyToVk(key);
            if (acName && vk > 0) keysList << QString("%1:%2").arg(acName).arg(vk);
        }
        if (!keysList.isEmpty()) env << QString("AC_KEYS=%1").arg(keysList.join(','));
    }
    if (!m_game.customEnv.trimmed().isEmpty()) {
        const QStringList vars = m_game.customEnv.split(QRegularExpression("[;\\s]+"), Qt::SkipEmptyParts);
        for (const auto& v : vars) {
            if (v.contains('=')) env << v.trimmed();
        }
    }
    return env.join(';');
}

} // namespace ac::launcher
