#pragma once

#include "emulationsession.h"

#include <QDialog>

#include <array>

class QCheckBox;
class QSpinBox;

class GameSettingsDialog final : public QDialog
{
public:
    explicit GameSettingsDialog(const GameSettings &settings, QWidget *parent = nullptr);

    GameSettings settings() const;

private:
    GameSettings _initialSettings;
    QCheckBox *_overrideOverscan = nullptr;
    std::array<QSpinBox *, 4> _overscanInputs = {};
    QList<QCheckBox *> _dipSwitches;
};