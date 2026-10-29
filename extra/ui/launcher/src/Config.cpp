#include "Config.hpp"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace ac::launcher {

Config::Config(const QString& iniPath)
    : m_iniPath(iniPath)
{
    load();
}

void Config::load()
{
    QSettings s(m_iniPath, QSettings::IniFormat);

    s.beginGroup("Graphics");
    m_graphics.width = s.value("width", 1920).toInt();
    m_graphics.height = s.value("height", 1080).toInt();
    m_graphics.fullscreen = s.value("fullscreen", false).toBool();
    m_graphics.vsync = s.value("vsync", true).toBool();
    m_graphics.fpsLimit = s.value("fps_limit", 60).toInt();
    m_graphics.renderScale = s.value("render_scale", 1.0).toDouble();
    m_graphics.upscaler = s.value("upscaler", "bilinear").toString();
    s.endGroup();

    s.beginGroup("Audio");
    m_audio.masterVolume = s.value("master_volume", 100).toInt();
    m_audio.muteOnUnfocus = s.value("mute_on_unfocus", false).toBool();
    s.endGroup();

    s.beginGroup("Game");
    m_game.parallelThreads = s.value("parallel_threads", true).toBool();
    m_game.ignoreFocus = s.value("ignore_focus", false).toBool();
    m_game.bc7Precompute = s.value("bc7_precompute", false).toBool();
    m_game.forceShaders = s.value("force_shaders", false).toBool();
    m_game.verboseLog = s.value("verbose_log", false).toBool();
    m_game.cutscene30Fps = s.value("cutscene_30fps", false).toBool();
    m_game.buttonPrompts = s.value("button_prompts", "auto").toString();
    m_game.commandWrapper = s.value("command_wrapper", "").toString();
    m_game.customEnv = s.value("custom_env", "").toString();
    s.endGroup();

    s.beginGroup("Controls");
    const auto keys = s.childKeys();
    for (const auto& k : keys) {
        m_bindings[k] = s.value(k).toString();
    }
    s.endGroup();
}

void Config::save()
{
    QFileInfo info(m_iniPath);
    QDir().mkpath(info.absolutePath());

    QSettings s(m_iniPath, QSettings::IniFormat);

    s.beginGroup("Graphics");
    s.setValue("width", m_graphics.width);
    s.setValue("height", m_graphics.height);
    s.setValue("fullscreen", m_graphics.fullscreen);
    s.setValue("vsync", m_graphics.vsync);
    s.setValue("fps_limit", m_graphics.fpsLimit);
    s.setValue("render_scale", m_graphics.renderScale);
    s.setValue("upscaler", m_graphics.upscaler);
    s.endGroup();

    s.beginGroup("Audio");
    s.setValue("master_volume", m_audio.masterVolume);
    s.setValue("mute_on_unfocus", m_audio.muteOnUnfocus);
    s.endGroup();

    s.beginGroup("Game");
    s.setValue("parallel_threads", m_game.parallelThreads);
    s.setValue("ignore_focus", m_game.ignoreFocus);
    s.setValue("bc7_precompute", m_game.bc7Precompute);
    s.setValue("force_shaders", m_game.forceShaders);
    s.setValue("verbose_log", m_game.verboseLog);
    s.setValue("cutscene_30fps", m_game.cutscene30Fps);
    s.setValue("button_prompts", m_game.buttonPrompts);
    s.setValue("command_wrapper", m_game.commandWrapper);
    s.setValue("custom_env", m_game.customEnv);
    s.endGroup();

    s.beginGroup("Controls");
    for (const auto& [action, key] : m_bindings) {
        s.setValue(action, key);
    }
    s.endGroup();

    s.sync();
}

QString Config::getBinding(const QString& action, const QString& fallback) const
{
    const auto it = m_bindings.find(action);
    return (it != m_bindings.end()) ? it->second : fallback;
}

void Config::setBinding(const QString& action, const QString& key)
{
    m_bindings[action] = key;
}

static int keyToVk(const QString& key) {
    if (key.length() == 1) {
        char ch = key.at(0).toUpper().toLatin1();
        if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) return (int)ch;
    }
    if (key == "Space") return 0x20;
    if (key == "LeftShift" || key == "Shift") return 0x10;
    if (key == "Tab") return 0x09;
    if (key == "Escape" || key == "Esc") return 0x1B;
    if (key == "Return" || key == "Enter") return 0x0D;
    if (key == "Mouse_Left") return 0x01;
    if (key == "Mouse_Right") return 0x02;
    if (key == "Mouse_Middle") return 0x04;
    if (key == "Up") return 0x26;
    if (key == "Down") return 0x28;
    if (key == "Left") return 0x25;
    if (key == "Right") return 0x27;
    return 0;
}

static const char* actionToAc(const QString& action) {
    if (action == "Button_A") return "A";
    if (action == "Button_B") return "B";
    if (action == "Button_X") return "X";
    if (action == "Button_Y") return "Y";
    if (action == "Button_L") return "L";
    if (action == "Button_R") return "R";
    if (action == "Button_ZL") return "ZL";
    if (action == "Button_ZR") return "ZR";
    if (action == "Button_Plus") return "PLUS";
    if (action == "Button_Minus") return "MINUS";
    if (action == "DPad_Up") return "DUP";
    if (action == "DPad_Down") return "DDOWN";
    if (action == "DPad_Left") return "DLEFT";
    if (action == "DPad_Right") return "DRIGHT";
    if (action == "Stick_L_Up") return "LUP";
    if (action == "Stick_L_Down") return "LDOWN";
    if (action == "Stick_L_Left") return "LLEFT";
    if (action == "Stick_L_Right") return "LRIGHT";
    if (action == "Stick_L_Click") return "LSTICK";
    if (action == "Stick_R_Click") return "RSTICK";
    return nullptr;
}

QString Config::buildLaunchEnv() const
{
    QStringList env;
    if (m_game.parallelThreads) env << "AC_PARALLEL=1";
    if (m_game.ignoreFocus) env << "AC_IGNORE_FOCUS=1";
    if (m_game.bc7Precompute) env << "AC_BC7_PRECOMPUTE=1";
    if (m_graphics.fullscreen) env << "AC_FULLSCREEN=1";
    if (m_graphics.upscaler == "fsr1") env << "AC_UPSCALER=fsr1";
    if (m_game.verboseLog) env << "AC_VERBOSE=1";
    if (m_game.cutscene30Fps) env << "AC_CUTSCENE_FPS=30";
    if (m_game.buttonPrompts == "switch") env << "AC_PROMPTS=switch";
    else if (m_game.buttonPrompts == "keyboard") env << "AC_PROMPTS=keyboard";
    else env << "AC_PROMPTS=auto";
    env << "AC_AUDIO_RESYNC=1";
    env << "LARECOMP_VK_LAZY_WB=0";
    env << "LARECOMP_VK_COPY_VERTICES=0";
    env << "AC_PRESENT_INTERVAL=1";
    env << QString("AC_WIDTH=%1").arg(m_graphics.width);
    env << QString("AC_HEIGHT=%1").arg(m_graphics.height);
    if (!m_bindings.empty()) {
        QStringList keysList;
        for (const auto& [action, key] : m_bindings) {
            const char* acName = actionToAc(action);
            int vk = keyToVk(key);
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
