#pragma once

#include <QDialog>
#include <memory>

class QCheckBox;
class QComboBox;
class QSpinBox;
class QSlider;
class QLabel;
class QLineEdit;

namespace ac::launcher {

class Config;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(Config& config, QWidget* parent = nullptr);
    ~SettingsDialog() override = default;

private slots:
    void onSave();

private:
    void setupUi();
    void loadFromConfig();
    void writeToConfig();

    QWidget* createGameTab();
    QWidget* createGraphicsTab();
    QWidget* createAudioTab();
    QWidget* createAdvancedTab();

    Config& m_config;

    // Game
    QCheckBox* m_parallelCheck{nullptr};
    QCheckBox* m_ignoreFocusCheck{nullptr};
    QComboBox* m_promptsCombo{nullptr};

    // Graphics
    QComboBox* m_resolutionCombo{nullptr};
    QCheckBox* m_fullscreenCheck{nullptr};
    QCheckBox* m_vsyncCheck{nullptr};
    QComboBox* m_presentModeCombo{nullptr};
    QSpinBox* m_fpsSpin{nullptr};
    QComboBox* m_scaleCombo{nullptr};
    QComboBox* m_upscalerCombo{nullptr};

    // Audio
    QSlider* m_volumeSlider{nullptr};
    QLabel* m_volumeLabel{nullptr};
    QCheckBox* m_muteUnfocusCheck{nullptr};

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
