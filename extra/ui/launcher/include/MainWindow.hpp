#pragma once

#include <QMainWindow>
#include <QStringList>
#include <QGraphicsDropShadowEffect>
#include <memory>
#include "Config.hpp"
#include "ProcessRunner.hpp"

class QPushButton;
class QLabel;
class QProgressBar;
class QTimer;

namespace ac::launcher {

class LogWidget;

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
    void onNextArt();
    void refreshGameStatus();

protected:
    void paintEvent(QPaintEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUi();
    void applyTheme();
    void loadArtList();
    void setRunningUi(bool running, qint64 pid = 0);
    void runCommand(const QString& program, const QStringList& args, const QString& statusText, bool isGame = false);

    Config m_config;
    ProcessRunner m_runner;

    QStringList m_artPaths;
    int m_currentArtIndex{0};
    QPixmap m_currentBackground;
    QTimer* m_artTimer{nullptr};

    QPushButton* m_playBtn{nullptr};
    QPushButton* m_diagBtn{nullptr};
    QPushButton* m_setupBtn{nullptr};
    QPushButton* m_shadersBtn{nullptr};
    QPushButton* m_settingsBtn{nullptr};
    QPushButton* m_controlsBtn{nullptr};
    QPushButton* m_artBtn{nullptr};

    QLabel* m_statusLabel{nullptr};
    LogWidget* m_logWidget{nullptr};
    QProgressBar* m_progressBar{nullptr};
    QWidget* m_sidePanel{nullptr};
    QLabel* m_statusDot{nullptr};
    QGraphicsDropShadowEffect* m_playGlow{nullptr};

    bool m_installed{false};
    bool m_isGameRunning{false};
    QString m_buildStatus{"missing"};
    QString m_shaderStatus{"never"};
};

} // namespace ac::launcher
