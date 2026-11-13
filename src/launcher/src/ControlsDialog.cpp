#include "ControlsDialog.hpp"
#include "Config.hpp"
#include "ConfigWatcher.hpp"

#include <QApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVector>

namespace ac::launcher {

namespace {

struct ActionDef {
    QString key;
    QString label;
    QString defaultKey;
};

const QVector<ActionDef> kDefaultActions = {
    {"Button_A", "Botão A (Interagir/Confirmar)", "E"},
    {"Button_B", "Botão B (Esquiva/Cancelar)", "Space"},
    {"Button_X", "Botão X (Habilidade Legion 1)", "Q"},
    {"Button_Y", "Botão Y (Habilidade Legion 2)", "R"},
    {"Button_L", "Botão L (Ação Legion)", "F"},
    {"Button_R", "Botão R (Dispensar Legion)", "C"},
    {"Button_ZL", "Gatilho ZL (Comandar Legion/Mira)", "Mouse_Right"},
    {"Button_ZR", "Gatilho ZR (Ataque)", "Mouse_Left"},
    {"Button_Plus", "Botão + (Menu/IRIS)", "Tab"},
    {"Button_Minus", "Botão - (Opções/Pausa)", "Escape"},
    {"DPad_Up", "D-Pad Cima (Item 1)", "1"},
    {"DPad_Down", "D-Pad Baixo (Item 2)", "2"},
    {"DPad_Left", "D-Pad Esquerda (Item 3)", "3"},
    {"DPad_Right", "D-Pad Direita (Item 4)", "4"},
    {"Stick_L_Up", "Analógico Esq. Cima (Mover)", "W"},
    {"Stick_L_Down", "Analógico Esq. Baixo (Mover)", "S"},
    {"Stick_L_Left", "Analógico Esq. Esquerda (Mover)", "A"},
    {"Stick_L_Right", "Analógico Esq. Direita (Mover)", "D"},
    {"Stick_L_Click", "Clique Analógico Esq. (L3 / Correr)", "LeftShift"},
    {"Stick_R_Click", "Clique Analógico Dir. (R3 / Lock-on)", "Mouse_Middle"}
};

} // namespace

ControlsDialog::ControlsDialog(Config& config, ConfigWatcher* watcher, QWidget* parent)
    : QDialog(parent)
    , m_config(config)
    , m_watcher(watcher) {
    setWindowTitle("IRIS // Mapeamento de Controles");
    resize(560, 580);
    setupUi();
    loadBindings();

    if (m_watcher) {
        connect(m_watcher, &ConfigWatcher::externalConfigChanged, this, &ControlsDialog::reloadExternal);
    }
}

void ControlsDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    auto* header = new QLabel("Dê um duplo clique em 'Tecla / Mouse' para editar (Aplicar atualiza o jogo ao vivo):", this);
    header->setStyleSheet("color: #59e8ff; font-weight: 600;");
    layout->addWidget(header);

    m_table = new QTableWidget(kDefaultActions.size(), 2, this);
    m_table->setHorizontalHeaderLabels({"Ação no Jogo", "Tecla / Mouse"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->verticalHeader()->setVisible(false);
    layout->addWidget(m_table);

    auto* hint = new QLabel(
        "Teclas aceitas: A-Z, 0-9, F1-F12, Space, Tab, Escape, Return, LeftShift, LCtrl, LAlt, Backspace, "
        "Insert, Delete, Home, End, PageUp, PageDown, Mouse_Left, Mouse_Right, Mouse_Middle, Mouse_X1, Mouse_X2, Wheel_Up, Wheel_Down",
        this);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #88a4c0; font-size: 11px;");
    layout->addWidget(hint);

    auto* btnLayout = new QHBoxLayout();
    m_resetButton = new QPushButton("Restaurar Padrões", this);
    m_cancelButton = new QPushButton("Cancelar", this);
    m_applyButton = new QPushButton("Aplicar", this);
    m_saveButton = new QPushButton("Salvar Controles", this);
    m_saveButton->setObjectName("primaryBtn");

    btnLayout->addWidget(m_resetButton);
    btnLayout->addStretch();
    btnLayout->addWidget(m_cancelButton);
    btnLayout->addWidget(m_applyButton);
    btnLayout->addWidget(m_saveButton);
    layout->addLayout(btnLayout);

    connect(m_resetButton, &QPushButton::clicked, this, &ControlsDialog::onResetDefaults);
    connect(m_applyButton, &QPushButton::clicked, this, &ControlsDialog::onApply);
    connect(m_saveButton, &QPushButton::clicked, this, &ControlsDialog::onSave);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void ControlsDialog::loadBindings() {
    for (int i = 0; i < kDefaultActions.size(); ++i) {
        const auto& act = kDefaultActions[i];
        QTableWidgetItem* actionItem = m_table->item(i, 0);
        if (!actionItem) {
            actionItem = new QTableWidgetItem(act.label);
            actionItem->setData(Qt::UserRole, act.key);
            actionItem->setFlags(actionItem->flags() & ~Qt::ItemIsEditable);
            m_table->setItem(i, 0, actionItem);
        }

        const QString key = m_config.getBinding(act.key, act.defaultKey);
        QTableWidgetItem* keyItem = m_table->item(i, 1);
        if (!keyItem) {
            keyItem = new QTableWidgetItem(key);
            m_table->setItem(i, 1, keyItem);
        } else {
            keyItem->setText(key);
        }
    }
}

void ControlsDialog::reloadExternal() {
    QWidget* fw = QApplication::focusWidget();
    if (m_table && fw && fw != m_table && m_table->isAncestorOf(fw)) {
        return;
    }
    loadBindings();
}

void ControlsDialog::onResetDefaults() {
    for (int i = 0; i < kDefaultActions.size(); ++i) {
        m_table->item(i, 1)->setText(kDefaultActions[i].defaultKey);
    }
}

void ControlsDialog::writeBindingsAndSave() {
    for (int i = 0; i < m_table->rowCount(); ++i) {
        const QString action = m_table->item(i, 0)->data(Qt::UserRole).toString();
        const QString key = m_table->item(i, 1)->text().trimmed();
        m_config.setBinding(action, key);
    }
    if (m_watcher) {
        m_watcher->saveAndTrack();
    } else {
        m_config.save();
    }
}

void ControlsDialog::onApply() {
    writeBindingsAndSave();
}

void ControlsDialog::onSave() {
    writeBindingsAndSave();
    accept();
}

} // namespace ac::launcher
