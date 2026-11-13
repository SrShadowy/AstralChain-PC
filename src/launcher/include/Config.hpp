#pragma once

#include <QString>
#include <QSettings>
#include <unordered_map>

namespace ac::launcher {

struct GraphicsSettings {
    static constexpr int kDefaultWidth = 1920;
    static constexpr int kDefaultHeight = 1080;
    static constexpr int kDefaultFpsLimit = 60;
    static constexpr int kMaxFpsLimit = 240;
    static constexpr int kFpsSpinStep = 5;
    static constexpr double kDefaultRenderScale = 1.0;
    static constexpr double kDefaultFsrSharpness = 0.80;
    static constexpr double kMinFsrSharpness = 0.0;
    static constexpr double kMaxFsrSharpness = 1.0;
    static constexpr double kFsrSharpnessStep = 0.05;
    static constexpr int kFsrSharpnessDecimals = 2;
    static constexpr double kDefaultLodBias = 0.0;
    static constexpr double kDefaultLodMin = 0.0;

    int width{kDefaultWidth};
    int height{kDefaultHeight};
    bool fullscreen{false};
    bool vsync{true};
    QString presentMode{"mailbox"};
    int fpsLimit{kDefaultFpsLimit};
    bool fpsUnlocked{false};  // AC_FPS_UNLOCKED: build with Dynamic-Unlocked FPS patch variant
    double renderScale{kDefaultRenderScale};
    QString upscaler{"fsr3"};
    QString antialias{"smaa"};
    bool frameGen{false};
    bool fsrJitter{true};
    double fsrSharpness{kDefaultFsrSharpness};
    double lodBias{kDefaultLodBias};
    double lodMin{kDefaultLodMin};
    bool noShadow{false};
};

struct AudioSettings {
    static constexpr int kMinVolume = 0;
    static constexpr int kDefaultVolume = 100;

    int masterVolume{kDefaultVolume};
    bool muteOnUnfocus{false};
};

struct MouseSettings {
    static constexpr bool kDefaultMouseLook = true;
    static constexpr double kDefaultSensitivity = 100.0;
    static constexpr double kDefaultYScale = 1.0;
    static constexpr const char* kDefaultCurve = "power";
    static constexpr bool kDefaultInvertY = false;
    static constexpr bool kDefaultMenuNav = true;
    static constexpr bool kDefaultMenuMotion = true;
    static constexpr int kDefaultMenuCell = 40;

    bool mouseLook{kDefaultMouseLook};
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
    bool xboxConfirmA{true};
    QString buttonPrompts{"auto"};
    QString language{"auto"};
    QString voiceLanguage{"auto"};
    QString commandWrapper{""};
    QString customEnv{""};
};

struct OverlaySettings {
    static constexpr int kDefaultCorner = 1;
    static constexpr double kDefaultOpacity = 0.85;
    static constexpr double kMinOpacity = 0.20;
    static constexpr double kMaxOpacity = 1.00;

    bool perfHudEnabled{false};
    int perfHudMode{0};
    int perfHudCorner{kDefaultCorner};
    double perfHudOpacity{kDefaultOpacity};
    bool perfHudShowWithMenu{true};
};

struct DiscordRpcSettings {
    static constexpr const char* kDefaultClientId = "1568955734606086196";

    bool enabled{true};
    bool showFile{true};
    bool showMission{true};
    bool showArea{true};
    bool showBoss{true};
    bool showDifficulty{true};
    bool showTime{true};
    QString clientId{kDefaultClientId};
};

class Config {
public:
    explicit Config(const QString& iniPath = "userdata/launcher.ini");

    void load();
    void save();

    [[nodiscard]] const QString& iniPath() const noexcept { return m_iniPath; }

    [[nodiscard]] GraphicsSettings& graphics() noexcept { return m_graphics; }
    [[nodiscard]] const GraphicsSettings& graphics() const noexcept { return m_graphics; }

    [[nodiscard]] AudioSettings& audio() noexcept { return m_audio; }
    [[nodiscard]] const AudioSettings& audio() const noexcept { return m_audio; }

    [[nodiscard]] MouseSettings& mouse() noexcept { return m_mouse; }
    [[nodiscard]] const MouseSettings& mouse() const noexcept { return m_mouse; }

    [[nodiscard]] GameSettings& game() noexcept { return m_game; }
    [[nodiscard]] const GameSettings& game() const noexcept { return m_game; }

    [[nodiscard]] OverlaySettings& overlay() noexcept { return m_overlay; }
    [[nodiscard]] const OverlaySettings& overlay() const noexcept { return m_overlay; }

    [[nodiscard]] DiscordRpcSettings& discordRpc() noexcept { return m_discordRpc; }
    [[nodiscard]] const DiscordRpcSettings& discordRpc() const noexcept { return m_discordRpc; }

    [[nodiscard]] QString getBinding(const QString& action, const QString& fallback = "") const;
    void setBinding(const QString& action, const QString& key);

    [[nodiscard]] QString buildLaunchEnv() const;

private:
    QString m_iniPath;
    GraphicsSettings m_graphics;
    AudioSettings m_audio;
    MouseSettings m_mouse;
    GameSettings m_game;
    OverlaySettings m_overlay;
    DiscordRpcSettings m_discordRpc;
    std::unordered_map<QString, QString> m_bindings;
};

} // namespace ac::launcher
