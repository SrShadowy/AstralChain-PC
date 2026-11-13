#include "MainWindow.hpp"
#include "ConfigWatcher.hpp"
#include "SettingsDialog.hpp"
#include "ControlsDialog.hpp"
#include "SetupDialog.hpp"
#include "LogWidget.hpp"
#include "CustomButton.hpp"
#include "LauncherIcons.hpp"
#include "Theme.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QPainter>
#include <QDir>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QCloseEvent>
#include <QStyle>

namespace ac::launcher {

namespace {
constexpr int kDefaultWinWidth       = 980;
constexpr int kDefaultWinHeight      = 620;
constexpr int kMinWinWidth           = 840;
constexpr int kMinWinHeight          = 520;
constexpr int kInitialStatusDelayMs  = 200;
constexpr int kIrisGridStepPx        = 36;
constexpr int kCornerMarginPx        = 8;
constexpr int kCornerLengthPx        = 14;
constexpr int kStatusProcTimeoutMs   = 3000;
constexpr int kProgressMaxPct        = 100;
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Astral Chain PC Launcher v%1 (Linux)").arg(AC_APP_VERSION));
    resize(kDefaultWinWidth, kDefaultWinHeight);
    setMinimumSize(kMinWinWidth, kMinWinHeight);

    setupUi();
    applyTheme();
    m_configWatcher = new ConfigWatcher(m_config, this);

    connect(&m_runner, &ProcessRunner::started, this, &MainWindow::onProcessStarted);
    connect(&m_runner, &ProcessRunner::lineReceived, this, &MainWindow::onProcessLine);
    connect(&m_runner, &ProcessRunner::finished, this, &MainWindow::onProcessFinished);

    QTimer::singleShot(kInitialStatusDelayMs, this, &MainWindow::refreshGameStatus);
}

void MainWindow::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QLinearGradient bg(0, 0, width(), height());
    bg.setColorAt(0.0, QColor(11, 19, 34));
    bg.setColorAt(0.5, QColor(8, 14, 25));
    bg.setColorAt(1.0, QColor(6, 10, 18));
    p.fillRect(rect(), bg);

    // Subtle IRIS holographic grid
    p.setPen(QPen(QColor(0, 212, 255, 9), 1));
    for (int x = 0; x < width(); x += kIrisGridStepPx) p.drawLine(x, 0, x, height());
    for (int y = 0; y < height(); y += kIrisGridStepPx) p.drawLine(0, y, width(), y);

    // Outer IRIS tactical corner brackets
    p.setPen(QPen(QColor(0, 212, 255, 160), 2));
    const int m = kCornerMarginPx;
    const int l = kCornerLengthPx;
    const int w = width();
    const int h = height();
    p.drawLine(m, m, m + l, m);
    p.drawLine(m, m, m, m + l);
    p.drawLine(w - m - l, m, w - m, m);
    p.drawLine(w - m, m, w - m, m + l);
    p.drawLine(m, h - m - l, m, h - m);
    p.drawLine(m, h - m, m + l, h - m);
    p.drawLine(w - m - l, h - m, w - m, h - m);
    p.drawLine(w - m, h - m - l, w - m, h - m);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_runner.isRunning() && m_isGameRunning) {
        const auto ret = QMessageBox::question(this, "Jogo em Execução",
            "Astral Chain ainda está em execução.\nDeseja encerrar o processo antes de sair?",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (ret == QMessageBox::Yes) {
            m_runner.stop();
            event->accept();
        } else if (ret == QMessageBox::No) {
            event->accept();
        } else {
            event->ignore();
            return;
        }
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::applyTheme()
{
    setStyleSheet(getLauncherStyleSheet());
}

namespace {
QString appRootDir()
{
    const QString envAppDir = qEnvironmentVariable("AC_APP_DIR").trimmed();
    return envAppDir.isEmpty() ? QStringLiteral(".") : envAppDir;
}

QString appScriptPath(const QString& scriptName)
{
    return QDir(appRootDir()).filePath(scriptName);
}

QString resolvePythonExecutable()
{
    const QString envPy = qEnvironmentVariable("AC_PYTHON").trimmed();
    if (!envPy.isEmpty() && QFile::exists(envPy)) {
        return envPy;
    }
    const QString bundledPy = QDir(appRootDir()).filePath("usr/bin/python3");
    if (QFile::exists(bundledPy)) {
        return bundledPy;
    }
    return QFile::exists(".venv/bin/python") ? QStringLiteral(".venv/bin/python") : QStringLiteral("python3");
}
} // namespace

QProcessEnvironment MainWindow::gameProcessEnvironment() const
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const auto& g = m_config.graphics();
    const bool effUnlocked = g.fpsUnlocked || g.fpsLimit == 0 || g.fpsLimit > GraphicsSettings::kDefaultFpsLimit;
    env.insert("AC_FPS_UNLOCKED", effUnlocked ? "1" : "0");
    env.insert("PYTHONPATH", QDir(appRootDir()).filePath("src"));
    return env;
}

void MainWindow::refreshGameStatus()
{
    // [extra] The FPS patch variant is a build input: ask setup_game about the build that
    // matches the selected variant, or a correct build would be reported stale.
    QProcess proc;
    proc.setProcessEnvironment(gameProcessEnvironment());
    const QString py = resolvePythonExecutable();
    proc.start(py, {"-m", "astral_chain.build.setup_game", "--status"});
    if (proc.waitForFinished(kStatusProcTimeoutMs)) {
        const auto doc = QJsonDocument::fromJson(proc.readAllStandardOutput());
        if (doc.isObject()) {
            const auto obj = doc.object();
            m_installed = obj.value("installed").toBool();
            m_buildStatus = obj.value("build").toString();
            m_shaderStatus = obj.value("shaders").toString();

            m_statusLabel->setText(QString("SISTEMA // Jogo: %1  │  Binário: %2  │  Shaders: %3")
                .arg(m_installed ? "Pronto" : "Não extraído").arg(m_buildStatus).arg(m_shaderStatus));
            m_statusDot->setStyleSheet(m_installed ? "font-size: 12px; color: #26eb8c;" : "font-size: 12px; color: #ff2e57;");

            if (!m_runner.isRunning()) {
                m_playBtn->setEnabled(m_installed);
                m_diagBtn->setEnabled(m_installed);
                m_shadersBtn->setEnabled(m_installed);
            }
            return;
        }
    }
    m_statusLabel->setText("SISTEMA // Pronto (configure na primeira execução)");
    m_statusDot->setStyleSheet("font-size: 12px; color: #ffb826;");
}

void MainWindow::setRunningUi(bool running, qint64 pid)
{
    if (running) {
        m_playBtn->setText(" PARAR");
        m_playBtn->setIcon(makeLauncherIcon("stop", QColor("#ffeef2"), QColor("#ffffff"), kDefaultUiIconPx));
        m_playBtn->setDangerMode(true);
        m_playBtn->setEnabled(true);
        m_statusDot->setStyleSheet("font-size: 12px; color: #26eb8c;");
    } else {
        m_playBtn->setText(" JOGAR");
        m_playBtn->setIcon(makeLauncherIcon("play", QColor("#dff2fc"), QColor("#08101c"), kDefaultUiIconPx));
        m_playBtn->setDangerMode(false);
        m_playBtn->setEnabled(m_installed);
        m_statusDot->setStyleSheet("font-size: 12px; color: #00d4ff;");
    }
    m_logWidget->setRunningState(running, pid);
    if (m_configWatcher) {
        m_configWatcher->setFastPolling(running);
    }
}

void MainWindow::runCommand(const QString& program, const QStringList& args, const QString& statusText, bool isGame)
{
    m_isGameRunning = isGame;
    m_statusLabel->setText(statusText);
    m_statusDot->setStyleSheet("font-size: 12px; color: #00d4ff;");
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);

    m_setupBtn->setEnabled(false);
    m_shadersBtn->setEnabled(false);
    m_diagBtn->setEnabled(!isGame);

    if (isGame) {
        setRunningUi(true, 0);
    } else {
        m_playBtn->setEnabled(false);
    }
    m_runner.start(program, args, "", gameProcessEnvironment());
}

void MainWindow::onPlay()
{
    if (m_runner.isRunning() && m_isGameRunning) {
        m_statusLabel->setText("Encerrando jogo...");
        m_playBtn->setText(" ENCERRANDO...");
        m_playBtn->setIcon(makeLauncherIcon("hourglass", QColor("#ffeef2"), QColor("#ffffff"), kDefaultUiIconPx));
        m_playBtn->setEnabled(false);
        m_runner.stop();
        return;
    }
    m_config.load();
    QStringList args;
    const QString env = m_config.buildLaunchEnv();
    if (!env.isEmpty()) args << "--env" << env;
    if (!m_config.game().commandWrapper.trimmed().isEmpty()) {
        args << "--wrapper" << m_config.game().commandWrapper.trimmed();
    }
    runCommand(appScriptPath("play.sh"), args, "Executando Astral Chain...", true);
}

void MainWindow::onPlayDiagnostics()
{
    if (m_runner.isRunning() && m_isGameRunning) {
        m_runner.stop();
        return;
    }
    m_config.load();
    QStringList args;
    const QString env = m_config.buildLaunchEnv();
    if (!env.isEmpty()) args << "--env" << env;
    if (!m_config.game().commandWrapper.trimmed().isEmpty()) {
        args << "--wrapper" << m_config.game().commandWrapper.trimmed();
    }
    runCommand(appScriptPath("play-diagnostics.sh"), args, "Executando com Diagnósticos...", true);
}

void MainWindow::onSetupGame()
{
    SetupDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    QStringList args = {"--keys", dlg.keysPath(), "--base", dlg.basePath(), "--update", dlg.updatePath()};
    if (!dlg.installDir().isEmpty()) args << "--install-dir" << dlg.installDir();
    runCommand(appScriptPath("setup.sh"), args, "Iniciando configuração do jogo...", false);
}

void MainWindow::onCompileShaders()
{
    QStringList args = {"-m", "astral_chain.build.setup_game", "--rebuild-if-stale"};
    if (m_config.game().forceShaders) args << "--force-shaders";
    const QString py = resolvePythonExecutable();
    runCommand(py, args, "Compilando shaders...", false);
}

void MainWindow::onOpenSettings()
{
    const bool gameRunning = m_runner.isRunning() && m_isGameRunning;
    SettingsDialog dlg(m_config, m_configWatcher, gameRunning, this);
    dlg.exec();
}

void MainWindow::onOpenControls()
{
    ControlsDialog dlg(m_config, m_configWatcher, this);
    dlg.exec();
}

void MainWindow::onProcessStarted(qint64 pid)
{
    if (m_isGameRunning) {
        setRunningUi(true, pid);
        m_statusLabel->setText(QString("Astral Chain em execução (PID %1)").arg(pid));
    }
}

void MainWindow::onProcessLine(const QString& line)
{
    m_logWidget->appendLog(line);

    if (line.startsWith("PROGRESS:")) {
        const auto parts = line.split(':');
        if (parts.size() >= 3) {
            bool ok = false;
            const int pct = parts[1].toInt(&ok);
            if (ok) {
                m_progressBar->setValue(pct);
                const QString msg = parts.mid(2).join(':').trimmed();
                m_statusLabel->setText(msg);
                m_progressBar->setFormat(QString("%1 (%p%)").arg(msg));
            }
        }
    }
}

void MainWindow::onProcessFinished(int exitCode, QProcess::ExitStatus)
{
    m_isGameRunning = false;
    setRunningUi(false);
    m_progressBar->setValue(exitCode == 0 ? 100 : 0);
    m_setupBtn->setEnabled(true);
    refreshGameStatus();
    m_logWidget->appendLog(QString("\nProcesso finalizado (código %1).").arg(exitCode));
}

} // namespace ac::launcher
