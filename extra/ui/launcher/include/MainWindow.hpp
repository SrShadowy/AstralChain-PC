#pragma once

#include <QMainWindow>
#include <QStringList>
#include <QProcessEnvironment>
#include <memory>
#include "Config.hpp"
#include "ProcessRunner.hpp"

class QLabel;
class QProgressBar;
class QTimer;

namespace ac::launcher {

class LogWidget;
class CustomButton;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onPlay();
    void onPlayDiagnostics();
    void onSetupGame();
    void onCompileShaders();
    void onOpenSettings();
    void onOpenControls();
    void onProcessLine(const QString& line);
    void onProcessStarted(qint64 pid);
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void refreshGameStatus();

protected:
    void paintEvent(QPaintEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUi();
    void applyTheme();
    QProcessEnvironment gameProcessEnvironment() const;  // [extra] build inputs from the config
    void setRunningUi(bool running, qint64 pid = 0);
    void runCommand(const QString& program, const QStringList& args, const QString& statusText, bool isGame = false);

    Config m_config;
    ProcessRunner m_runner;

    CustomButton* m_playBtn{nullptr};
    CustomButton* m_diagBtn{nullptr};
    CustomButton* m_setupBtn{nullptr};
    CustomButton* m_shadersBtn{nullptr};
    CustomButton* m_settingsBtn{nullptr};
    CustomButton* m_controlsBtn{nullptr};

    QLabel* m_statusLabel{nullptr};
    LogWidget* m_logWidget{nullptr};
    QProgressBar* m_progressBar{nullptr};
    QWidget* m_sidePanel{nullptr};
    QLabel* m_statusDot{nullptr};

    bool m_installed{false};
    bool m_isGameRunning{false};
    QString m_buildStatus{"missing"};
    QString m_shaderStatus{"never"};
};

} // namespace ac::launcher
