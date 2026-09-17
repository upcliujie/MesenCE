#pragma once

#include "emulationsession.h"

#include <QDialog>

#include <vector>

class QCheckBox;
class QTableWidget;

class CheatDialog final : public QDialog
{
public:
    explicit CheatDialog(EmulationSession &session, QWidget *parent = nullptr);

private:
    void loadCheats();
    bool saveAndApply();
    void refreshTable();
    bool editCheat(FrontendCheat &cheat);
    void importFromDatabase();

    EmulationSession &_session;
    std::vector<FrontendCheat> _cheats;
    QTableWidget *_table = nullptr;
    QCheckBox *_disableAll = nullptr;
};