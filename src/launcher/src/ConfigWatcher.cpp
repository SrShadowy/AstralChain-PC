#include "ConfigWatcher.hpp"
#include "Config.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QTimer>

namespace ac::launcher {

namespace {
constexpr int kIdlePollMs = 1200;
constexpr int kLivePollMs = 400;
} // namespace

ConfigWatcher::ConfigWatcher(Config& config, QObject* parent)
    : QObject(parent)
    , m_config(config) {
    m_watcher = new QFileSystemWatcher(this);
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(kIdlePollMs);

    m_lastDigest = readDigest(m_lastExists);
    ensureWatches();

    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &ConfigWatcher::onPathChanged);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &ConfigWatcher::onPathChanged);
    connect(m_pollTimer, &QTimer::timeout, this, &ConfigWatcher::checkFile);
    m_pollTimer->start();
}

void ConfigWatcher::setFastPolling(bool enabled) {
    m_pollTimer->setInterval(enabled ? kLivePollMs : kIdlePollMs);
}

void ConfigWatcher::saveAndTrack() {
    m_config.save();
    m_lastDigest = readDigest(m_lastExists);
    ensureWatches();
}

void ConfigWatcher::onPathChanged(const QString&) {
    ensureWatches();
    checkFile();
}

void ConfigWatcher::checkFile() {
    bool exists = false;
    const QByteArray digest = readDigest(exists);
    if (!exists) {
        m_lastExists = false;
        m_lastDigest.clear();
        return;
    }
    if (m_lastExists && digest == m_lastDigest) {
        return;
    }
    m_lastExists = true;
    m_lastDigest = digest;
    m_config.load();
    ensureWatches();
    emit externalConfigChanged();
}

void ConfigWatcher::ensureWatches() {
    const QFileInfo fi(m_config.iniPath());
    const QString absFile = fi.absoluteFilePath();
    const QString absDir = fi.absolutePath();
    if (!QDir(absDir).exists()) {
        return;
    }
    if (!m_watcher->directories().contains(absDir)) {
        m_watcher->addPath(absDir);
    }
    if (fi.exists() && !m_watcher->files().contains(absFile)) {
        m_watcher->addPath(absFile);
    }
}

QByteArray ConfigWatcher::readDigest(bool& exists) const {
    QFile f(m_config.iniPath());
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
        exists = false;
        return {};
    }
    exists = true;
    return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256);
}

} // namespace ac::launcher
