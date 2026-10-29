#include "ProcessRunner.hpp"
#include <QTimer>

#if defined(Q_OS_UNIX)
#include <csignal>
#include <unistd.h>
#endif

namespace ac::launcher {

ProcessRunner::ProcessRunner(QObject* parent)
    : QObject(parent)
{
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &ProcessRunner::onReadyReadStandardOutput);
    connect(&m_process, &QProcess::readyReadStandardError, this, &ProcessRunner::onReadyReadStandardError);
    connect(&m_process, &QProcess::finished, this, &ProcessRunner::onProcessFinished);
    connect(&m_process, &QProcess::errorOccurred, this, &ProcessRunner::onProcessError);
    connect(&m_process, &QProcess::started, this, [this]() {
        emit started(m_process.processId());
    });

#if defined(Q_OS_UNIX)
    m_process.setChildProcessModifier([]() {
        ::setpgid(0, 0);
    });
#endif
}

ProcessRunner::~ProcessRunner()
{
    if (m_process.state() != QProcess::NotRunning) {
        stop();
        if (!m_process.waitForFinished(1000)) {
            const qint64 p = m_process.processId();
#if defined(Q_OS_UNIX)
            if (p > 0) ::kill(-static_cast<pid_t>(p), SIGKILL);
#endif
            m_process.kill();
            m_process.waitForFinished(200);
        }
    }
}

void ProcessRunner::start(const QString& program, const QStringList& arguments, const QString& workingDir)
{
    if (!workingDir.isEmpty()) {
        m_process.setWorkingDirectory(workingDir);
    }
    m_process.start(program, arguments);
}

void ProcessRunner::stop()
{
    if (m_process.state() == QProcess::NotRunning) return;

    const qint64 p = m_process.processId();
#if defined(Q_OS_UNIX)
    if (p > 0) {
        ::kill(-static_cast<pid_t>(p), SIGTERM);
        ::kill(static_cast<pid_t>(p), SIGTERM);
    }
#endif
    m_process.terminate();

    QTimer::singleShot(1000, this, [this, p]() {
        if (m_process.state() != QProcess::NotRunning) {
#if defined(Q_OS_UNIX)
            if (p > 0) {
                ::kill(-static_cast<pid_t>(p), SIGKILL);
                ::kill(static_cast<pid_t>(p), SIGKILL);
            }
#endif
            m_process.kill();
        }
    });
}

void ProcessRunner::cancel()
{
    stop();
}

bool ProcessRunner::isRunning() const noexcept
{
    return m_process.state() != QProcess::NotRunning;
}

qint64 ProcessRunner::pid() const noexcept
{
    return m_process.processId();
}

void ProcessRunner::onReadyReadStandardOutput()
{
    const QByteArray data = m_process.readAllStandardOutput();
    const QString text = QString::fromUtf8(data);
    const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    for (const auto& line : lines) {
        emit lineReceived(line);
    }
}

void ProcessRunner::onReadyReadStandardError()
{
    const QByteArray data = m_process.readAllStandardError();
    const QString text = QString::fromUtf8(data);
    const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    for (const auto& line : lines) {
        emit lineReceived(line);
    }
}

void ProcessRunner::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    emit finished(exitCode, exitStatus);
}

void ProcessRunner::onProcessError(QProcess::ProcessError)
{
    emit errorOccurred(m_process.errorString());
}

} // namespace ac::launcher
