#include "gamesettingsdialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

GameSettingsDialog::GameSettingsDialog(const GameSettings &settings, QWidget *parent)
    : QDialog(parent)
    , _initialSettings(settings)
{
    setWindowTitle(tr("游戏设置"));
    resize(380, 320);

    auto *rootLayout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    rootLayout->addWidget(tabs);

    auto *overscanPage = new QWidget(tabs);
    auto *overscanLayout = new QVBoxLayout(overscanPage);
    _overrideOverscan = new QCheckBox(tr("覆盖裁剪设置"), overscanPage);
    _overrideOverscan->setChecked(settings.overrideOverscan);
    overscanLayout->addWidget(_overrideOverscan);
    auto *overscanGroup = new QGroupBox(tr("裁剪像素"), overscanPage);
    auto *overscanForm = new QFormLayout(overscanGroup);
    const QStringList labels = {tr("左"), tr("右"), tr("上"), tr("下")};
    const std::array<uint32_t, 4> values = {settings.overscanLeft, settings.overscanRight, settings.overscanTop, settings.overscanBottom};
    for (int index = 0; index < 4; ++index) {
        _overscanInputs[index] = new QSpinBox(overscanGroup);
        _overscanInputs[index]->setRange(0, 1000);
        _overscanInputs[index]->setValue(static_cast<int>(values[index]));
        overscanForm->addRow(labels[index], _overscanInputs[index]);
    }
    overscanGroup->setEnabled(settings.overrideOverscan);
    connect(_overrideOverscan, &QCheckBox::toggled, overscanGroup, &QWidget::setEnabled);
    overscanLayout->addWidget(overscanGroup);
    overscanLayout->addStretch();
    tabs->addTab(overscanPage, tr("裁剪"));

    if (settings.dipSwitchCount > 0) {
        auto *dipPage = new QWidget(tabs);
        auto *dipLayout = new QVBoxLayout(dipPage);
        for (uint32_t index = 0; index < settings.dipSwitchCount && index < 32; ++index) {
            auto *checkBox = new QCheckBox(tr("开关 %1").arg(index + 1), dipPage);
            checkBox->setChecked((settings.dipSwitches & (1U << index)) != 0);
            _dipSwitches.append(checkBox);
            dipLayout->addWidget(checkBox);
        }
        dipLayout->addStretch();
        auto *scrollArea = new QScrollArea(tabs);
        scrollArea->setWidgetResizable(true);
        scrollArea->setWidget(dipPage);
        tabs->addTab(scrollArea, tr("DIP 开关"));
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    rootLayout->addWidget(buttons);
}

GameSettings GameSettingsDialog::settings() const
{
    GameSettings result = _initialSettings;
    result.overrideOverscan = _overrideOverscan->isChecked();
    result.overscanLeft = static_cast<uint32_t>(_overscanInputs[0]->value());
    result.overscanRight = static_cast<uint32_t>(_overscanInputs[1]->value());
    result.overscanTop = static_cast<uint32_t>(_overscanInputs[2]->value());
    result.overscanBottom = static_cast<uint32_t>(_overscanInputs[3]->value());
    result.dipSwitches = 0;
    for (int index = 0; index < _dipSwitches.size(); ++index) {
        if (_dipSwitches[index]->isChecked()) {
            result.dipSwitches |= 1U << index;
        }
    }
    return result;
}