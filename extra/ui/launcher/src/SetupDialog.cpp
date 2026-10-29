#include "SetupDialog.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>

namespace ac::launcher {

SetupDialog::SetupDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Configurar Arquivos do Jogo (Setup)");
    setMinimumWidth(560);
    setupUi();
    loadSavedPaths();
    validateInputs();
}

void SetupDialog::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(14);
    layout->setContentsMargins(18, 18, 18, 18);

    auto* header = new QLabel("Selecione os arquivos necessários para a recompilação nativa:", this);
    header->setStyleSheet("font-size: 13px; font-weight: bold; color: #eceff1;");
    layout->addWidget(header);

    auto* grid = new QGridLayout();
    grid->setSpacing(10);

    // Keys
    auto* keysLabel = new QLabel("prod.keys:", this);
    m_keysEdit = new QLineEdit(this);
    m_keysEdit->setPlaceholderText("Caminho para prod.keys");
    auto* keysBtn = new QPushButton("Procurar...", this);
    connect(keysBtn, &QPushButton::clicked, this, &SetupDialog::onBrowseKeys);
    connect(m_keysEdit, &QLineEdit::textChanged, this, &SetupDialog::validateInputs);
    grid->addWidget(keysLabel, 0, 0);
    grid->addWidget(m_keysEdit, 0, 1);
    grid->addWidget(keysBtn, 0, 2);

    // Base
    auto* baseLabel = new QLabel("Jogo Base (XCI/NSP):", this);
    m_baseEdit = new QLineEdit(this);
    m_baseEdit->setPlaceholderText("Caminho para ASTRAL CHAIN (.nsp ou .xci)");
    auto* baseBtn = new QPushButton("Procurar...", this);
    connect(baseBtn, &QPushButton::clicked, this, &SetupDialog::onBrowseBase);
    connect(m_baseEdit, &QLineEdit::textChanged, this, &SetupDialog::validateInputs);
    grid->addWidget(baseLabel, 1, 0);
    grid->addWidget(m_baseEdit, 1, 1);
    grid->addWidget(baseBtn, 1, 2);

    // Update
    auto* updateLabel = new QLabel("Update v1.0.1 (NSP):", this);
    m_updateEdit = new QLineEdit(this);
    m_updateEdit->setPlaceholderText("Caminho para o update v1.0.1 (.nsp)");
    auto* updateBtn = new QPushButton("Procurar...", this);
    connect(updateBtn, &QPushButton::clicked, this, &SetupDialog::onBrowseUpdate);
    connect(m_updateEdit, &QLineEdit::textChanged, this, &SetupDialog::validateInputs);
    grid->addWidget(updateLabel, 2, 0);
    grid->addWidget(m_updateEdit, 2, 1);
    grid->addWidget(updateBtn, 2, 2);

    // Install Destination Directory
    auto* dirLabel = new QLabel("Pasta de Instalação:", this);
    m_installDirEdit = new QLineEdit(this);
    m_installDirEdit->setPlaceholderText("Pasta de destino dos dados (padrão: ./game)");
    auto* dirBtn = new QPushButton("Procurar...", this);
    connect(dirBtn, &QPushButton::clicked, this, &SetupDialog::onBrowseInstallDir);
    grid->addWidget(dirLabel, 3, 0);
    grid->addWidget(m_installDirEdit, 3, 1);
    grid->addWidget(dirBtn, 3, 2);

    layout->addLayout(grid);

    auto* note = new QLabel("Nota: O update v1.0.1 e as chaves originais do console são obrigatórios.", this);
    note->setStyleSheet("color: #90a4ae; font-size: 11px;");
    layout->addWidget(note);

    layout->addStretch();

    // Buttons
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_cancelBtn = new QPushButton("Cancelar", this);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_cancelBtn);

    m_startBtn = new QPushButton("Iniciar Instalação", this);
    m_startBtn->setStyleSheet(
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1565c0, stop:1 #00acc1); "
        "color: white; font-weight: bold; padding: 8px 22px; border-radius: 8px; border: none; font-size: 13px; }"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1976d2, stop:1 #00bcd4); }"
        "QPushButton:disabled { background: rgba(55, 71, 79, 0.6); color: rgba(120, 144, 156, 0.5); }");
    m_startBtn->setEnabled(false);
    connect(m_startBtn, &QPushButton::clicked, this, &SetupDialog::onStart);
    btnLayout->addWidget(m_startBtn);

    layout->addLayout(btnLayout);
}

void SetupDialog::loadSavedPaths()
{
    QFile file("userdata/setup.json");
    if (!file.open(QIODevice::ReadOnly)) return;

    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return;

    const auto obj = doc.object();
    if (obj.contains("keys")) m_keysEdit->setText(obj["keys"].toString());
    if (obj.contains("base")) m_baseEdit->setText(obj["base"].toString());
    if (obj.contains("update")) m_updateEdit->setText(obj["update"].toString());
    if (obj.contains("install_dir")) m_installDirEdit->setText(obj["install_dir"].toString());
}

void SetupDialog::savePaths()
{
    QDir().mkpath("userdata");
    QFile file("userdata/setup.json");
    if (!file.open(QIODevice::WriteOnly)) return;

    QJsonObject obj;
    obj["keys"] = m_keysEdit->text().trimmed();
    obj["base"] = m_baseEdit->text().trimmed();
    obj["update"] = m_updateEdit->text().trimmed();
    obj["install_dir"] = m_installDirEdit->text().trimmed();

    file.write(QJsonDocument(obj).toJson());
}

void SetupDialog::validateInputs()
{
    const bool kOk = QFileInfo::exists(m_keysEdit->text().trimmed()) && QFileInfo(m_keysEdit->text().trimmed()).isFile();
    const bool bOk = QFileInfo::exists(m_baseEdit->text().trimmed()) && QFileInfo(m_baseEdit->text().trimmed()).isFile();
    const bool uOk = QFileInfo::exists(m_updateEdit->text().trimmed()) && QFileInfo(m_updateEdit->text().trimmed()).isFile();

    m_startBtn->setEnabled(kOk && bOk && uOk);
}

void SetupDialog::onBrowseKeys()
{
    const QString f = QFileDialog::getOpenFileName(this, "Selecionar prod.keys", m_keysEdit->text(), "Keys (*.keys);;Todos os arquivos (*)");
    if (!f.isEmpty()) m_keysEdit->setText(f);
}

void SetupDialog::onBrowseBase()
{
    const QString f = QFileDialog::getOpenFileName(this, "Selecionar Jogo Base", m_baseEdit->text(), "Nintendo Switch Games (*.nsp *.xci);;Todos os arquivos (*)");
    if (!f.isEmpty()) m_baseEdit->setText(f);
}

void SetupDialog::onBrowseUpdate()
{
    const QString f = QFileDialog::getOpenFileName(this, "Selecionar Update v1.0.1", m_updateEdit->text(), "Update NSP (*.nsp);;Todos os arquivos (*)");
    if (!f.isEmpty()) m_updateEdit->setText(f);
}

void SetupDialog::onBrowseInstallDir()
{
    const QString cur = m_installDirEdit->text().isEmpty() ? QDir::currentPath() : m_installDirEdit->text();
    const QString d = QFileDialog::getExistingDirectory(this, "Selecionar Pasta de Instalação", cur);
    if (!d.isEmpty()) m_installDirEdit->setText(d);
}

void SetupDialog::onStart()
{
    savePaths();
    accept();
}

QString SetupDialog::keysPath() const { return m_keysEdit->text().trimmed(); }
QString SetupDialog::basePath() const { return m_baseEdit->text().trimmed(); }
QString SetupDialog::updatePath() const { return m_updateEdit->text().trimmed(); }
QString SetupDialog::installDir() const { return m_installDirEdit->text().trimmed(); }

} // namespace ac::launcher
