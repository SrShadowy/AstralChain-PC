#pragma once

#include <QDialog>
#include "Config.hpp"

class QCheckBox;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QSlider;
class QLabel;
class QLineEdit;
class QTimer;

namespace ac::launcher {

class ConfigWatcher;
class MouseSettingsWidget;
class OverlayRpcSettingsWidget;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(Config& config,
                            ConfigWatcher* watcher = nullptr,
                            bool gameRunning = false,
                            QWidget* parent = nullptr);
    ~SettingsDialog() override = default;

    void reject() override;

public slots:
    void reloadExternal();

private slots:
    void onApply();
    void onSave();
    void onLiveControlModified();
    void flushLiveSave();

private:
    void setupUi(bool gameRunning);
    void wireLiveSignals();
    void saveConfigNow();
    void loadFromConfig();
    void writeToConfig();

    QWidget* createGameTab();
    QWidget* createGraphicsTab();
    QWidget* createAudioTab();
    QWidget* createOverlayRpcTab();
    QWidget* createAdvancedTab();

    Config& m_config;
    Config m_snapshot;
    ConfigWatcher* m_watcher{nullptr};
    QTimer* m_liveSaveTimer{nullptr};
    QCheckBox* m_liveSyncCheck{nullptr};
    bool m_loadingUi{false};
    bool m_liveCommittedDirty{false};

    // Game
    QCheckBox* m_parallelCheck{nullptr};
    QCheckBox* m_ignoreFocusCheck{nullptr};
    QCheckBox* m_xboxConfirmACheck{nullptr};
    QComboBox* m_promptsCombo{nullptr};
    QComboBox* m_languageCombo{nullptr};
    QComboBox* m_voiceLangCombo{nullptr};
    MouseSettingsWidget* m_mouseWidget{nullptr};

    // Graphics
    QComboBox* m_resolutionCombo{nullptr};
    QCheckBox* m_fullscreenCheck{nullptr};
    QCheckBox* m_vsyncCheck{nullptr};
    QComboBox* m_presentModeCombo{nullptr};
    QSpinBox* m_fpsSpin{nullptr};
    QCheckBox* m_fpsUnlockedCheck{nullptr};
    QComboBox* m_scaleCombo{nullptr};
    QComboBox* m_upscalerCombo{nullptr};
    QComboBox* m_antialiasCombo{nullptr};
    QCheckBox* m_frameGenCheck{nullptr};
    QCheckBox* m_fsrJitterCheck{nullptr};
    QDoubleSpinBox* m_fsrSharpnessSpin{nullptr};
    QComboBox* m_textureQualityCombo{nullptr};
    QComboBox* m_lodCombo{nullptr};
    QCheckBox* m_noShadowCheck{nullptr};

    // Audio
    QSlider* m_volumeSlider{nullptr};
    QLabel* m_volumeLabel{nullptr};
    QCheckBox* m_muteUnfocusCheck{nullptr};

    // Overlay & Discord RPC
    OverlayRpcSettingsWidget* m_overlayRpcWidget{nullptr};

    // Advanced
    QCheckBox* m_bc7Check{nullptr};
    QCheckBox* m_forceShadersCheck{nullptr};
    QCheckBox* m_verboseLogCheck{nullptr};
    QCheckBox* m_cutscene30Check{nullptr};
    QCheckBox* m_asyncSubmitCheck{nullptr};
    QLineEdit* m_wrapperEdit{nullptr};
    QLineEdit* m_customEnvEdit{nullptr};
};

} // namespace ac::launcher
