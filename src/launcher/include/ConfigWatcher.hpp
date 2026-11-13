#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class QFileSystemWatcher;
class QTimer;

namespace ac::launcher {

class Config;

// Watches userdata/launcher.ini for external edits (e.g. from the in-game ImGui overlay),
// suppresses self-echoes by SHA-256 content digest, and reloads Config automatically.
class ConfigWatcher : public QObject {
    Q_OBJECT

public:
    explicit ConfigWatcher(Config& config, QObject* parent = nullptr);

    void setFastPolling(bool enabled);
    void saveAndTrack();

signals:
    void externalConfigChanged();

private slots:
    void onPathChanged(const QString& path);
    void checkFile();

private:
    void ensureWatches();
    QByteArray readDigest(bool& exists) const;

    Config& m_config;
    QFileSystemWatcher* m_watcher{nullptr};
    QTimer* m_pollTimer{nullptr};
    QByteArray m_lastDigest;
    bool m_lastExists{false};
};

} // namespace ac::launcher
