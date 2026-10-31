#pragma once

#include <QString>
#include <QSettings>
#include <memory>
#include <unordered_map>

namespace ac::launcher {

struct GraphicsSettings {
    int width{1920};
    int height{1080};
    bool fullscreen{false};
    bool vsync{true};
    QString presentMode{"fifo"};
    int fpsLimit{60};
    double renderScale{1.0};
    QString upscaler{"bilinear"};
};

struct AudioSettings {
    int masterVolume{100};
    bool muteOnUnfocus{false};
};

struct GameSettings {
    bool parallelThreads{true};
    bool ignoreFocus{false};
    bool bc7Precompute{false};
    bool forceShaders{false};
    bool verboseLog{false};
    bool cutscene30Fps{false};
    bool asyncSubmit{true};
    QString buttonPrompts{"auto"};
    QString commandWrapper{""};
    QString customEnv{""};
};

class Config {
public:
    explicit Config(const QString& iniPath = "userdata/launcher.ini");

    void load();
    void save();

    [[nodiscard]] GraphicsSettings& graphics() noexcept { return m_graphics; }
    [[nodiscard]] const GraphicsSettings& graphics() const noexcept { return m_graphics; }

    [[nodiscard]] AudioSettings& audio() noexcept { return m_audio; }
    [[nodiscard]] const AudioSettings& audio() const noexcept { return m_audio; }

    [[nodiscard]] GameSettings& game() noexcept { return m_game; }
    [[nodiscard]] const GameSettings& game() const noexcept { return m_game; }

    [[nodiscard]] QString getBinding(const QString& action, const QString& fallback = "") const;
    void setBinding(const QString& action, const QString& key);

    [[nodiscard]] QString buildLaunchEnv() const;

private:
    QString m_iniPath;
    GraphicsSettings m_graphics;
    AudioSettings m_audio;
    GameSettings m_game;
    std::unordered_map<QString, QString> m_bindings;
};

} // namespace ac::launcher
