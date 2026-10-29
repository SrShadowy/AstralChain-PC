#include "ControlsDialog.hpp"
#include "Config.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QVector>
#include <QPair>

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

ControlsDialog::ControlsDialog(Config& config, QWidget* parent)
    : QDialog(parent)
    , m_config(config)
{
    setWindowTitle("Astral Chain - Controls Configuration");
    resize(480, 520);
    setupUi();
    loadBindings();
}

void ControlsDialog::setupUi()
{
    auto* layout = new QVBoxLayout(this);

    auto* header = new QLabel("Double-click a cell under 'Key / Mouse' to edit binding:", this);
    layout->addWidget(header);

    m_table = new QTableWidget(kDefaultActions.size(), 2, this);
    m_table->setHorizontalHeaderLabels({"Action", "Key / Mouse"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_table);

    auto* btnLayout = new QHBoxLayout();
    m_resetButton = new QPushButton("Reset Defaults", this);
    m_saveButton = new QPushButton("Save", this);
    m_cancelButton = new QPushButton("Cancel", this);

    btnLayout->addWidget(m_resetButton);
    btnLayout->addStretch();
    btnLayout->addWidget(m_saveButton);
    btnLayout->addWidget(m_cancelButton);

    layout->addLayout(btnLayout);

    connect(m_resetButton, &QPushButton::clicked, this, &ControlsDialog::onResetDefaults);
    connect(m_saveButton, &QPushButton::clicked, this, &ControlsDialog::onSave);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void ControlsDialog::loadBindings()
{
    for (int i = 0; i < kDefaultActions.size(); ++i) {
        const auto& act = kDefaultActions[i];
        auto* actionItem = new QTableWidgetItem(act.label);
        actionItem->setData(Qt::UserRole, act.key);
        actionItem->setFlags(actionItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(i, 0, actionItem);

        const QString key = m_config.getBinding(act.key, act.defaultKey);
        auto* keyItem = new QTableWidgetItem(key);
        m_table->setItem(i, 1, keyItem);
    }
}

void ControlsDialog::onResetDefaults()
{
    for (int i = 0; i < kDefaultActions.size(); ++i) {
        m_table->item(i, 1)->setText(kDefaultActions[i].defaultKey);
    }
}

void ControlsDialog::onSave()
{
    for (int i = 0; i < m_table->rowCount(); ++i) {
        const QString action = m_table->item(i, 0)->data(Qt::UserRole).toString();
        const QString key = m_table->item(i, 1)->text();
        m_config.setBinding(action, key);
    }
    m_config.save();
    accept();
}

} // namespace ac::launcher
