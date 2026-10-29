#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

namespace ac::launcher {

class ProcessRunner : public QObject {
    Q_OBJECT

public:
    explicit ProcessRunner(QObject* parent = nullptr);
    ~ProcessRunner() override;

    void start(const QString& program, const QStringList& arguments, const QString& workingDir = "");
    void stop();
    void cancel();
    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] qint64 pid() const noexcept;

signals:
    void started(qint64 pid);
    void lineReceived(const QString& line);
    void finished(int exitCode, QProcess::ExitStatus exitStatus);
    void errorOccurred(const QString& errorText);

private slots:
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    QProcess m_process;
};

} // namespace ac::launcher
