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
    double lodBias{0.0};   // texture quality: global LOD bias (0 = native, + = lighter)
    double lodMin{0.0};    // LoD floor: min-LOD (0 = off)
    bool noShadow{false};  // disable shadow-map sampling
};

struct AudioSettings {
    int masterVolume{100};
    bool muteOnUnfocus{false};
};

struct MouseSettings {
    static constexpr double kDefaultSensitivity = 100.0;
    static constexpr double kDefaultYScale = 1.0;
    static constexpr const char* kDefaultCurve = "power";
    static constexpr bool kDefaultInvertY = false;
    static constexpr bool kDefaultMenuNav = true;
    static constexpr bool kDefaultMenuMotion = true;
    static constexpr int kDefaultMenuCell = 40;

    double sensitivity{kDefaultSensitivity};
    double yScale{kDefaultYScale};
    QString curve{kDefaultCurve};
    bool invertY{kDefaultInvertY};
    bool menuNav{kDefaultMenuNav};
    bool menuMotion{kDefaultMenuMotion};
    int menuCell{kDefaultMenuCell};
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

    [[nodiscard]] MouseSettings& mouse() noexcept { return m_mouse; }
    [[nodiscard]] const MouseSettings& mouse() const noexcept { return m_mouse; }

    [[nodiscard]] GameSettings& game() noexcept { return m_game; }
    [[nodiscard]] const GameSettings& game() const noexcept { return m_game; }

    [[nodiscard]] QString getBinding(const QString& action, const QString& fallback = "") const;
    void setBinding(const QString& action, const QString& key);

    [[nodiscard]] QString buildLaunchEnv() const;

private:
    QString m_iniPath;
    GraphicsSettings m_graphics;
    AudioSettings m_audio;
    MouseSettings m_mouse;
    GameSettings m_game;
    std::unordered_map<QString, QString> m_bindings;
};

} // namespace ac::launcher
