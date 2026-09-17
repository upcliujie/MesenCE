#include "cheatdialog.h"

#include "emulationsession.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSplitter>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>

CheatDialog::CheatDialog(EmulationSession &session, QWidget *parent)
    : QDialog(parent)
    , _session(session)
{
    setWindowTitle(tr("金手指"));
    resize(760, 480);

    auto *rootLayout = new QVBoxLayout(this);
    auto *toolbar = new QHBoxLayout();
    auto *addButton = new QPushButton(tr("添加"), this);
    auto *editButton = new QPushButton(tr("编辑"), this);
    auto *removeButton = new QPushButton(tr("删除"), this);
    auto *databaseButton = new QPushButton(tr("金手指数据库"), this);
    toolbar->addWidget(addButton);
    toolbar->addWidget(editButton);
    toolbar->addWidget(removeButton);
    toolbar->addStretch();
    toolbar->addWidget(databaseButton);
    rootLayout->addLayout(toolbar);

    _table = new QTableWidget(this);
    _table->setColumnCount(4);
    _table->setHorizontalHeaderLabels({tr("启用"), tr("描述"), tr("类型"), tr("代码")});
    _table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _table->setSelectionMode(QAbstractItemView::SingleSelection);
    _table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _table->verticalHeader()->setVisible(false);
    _table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    _table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    _table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    _table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    rootLayout->addWidget(_table);

    _disableAll = new QCheckBox(tr("禁用所有金手指"), this);
    rootLayout->addWidget(_disableAll);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    rootLayout->addWidget(buttons);

    loadCheats();
    refreshTable();

    connect(addButton, &QPushButton::clicked, this, [this] {
        FrontendCheat cheat;
        const std::vector<CheatTypeOption> types = _session.cheatTypeOptions();
        if (!types.empty()) {
            cheat.type = types.front().type;
        }
        if (editCheat(cheat)) {
            _cheats.push_back(cheat);
            refreshTable();
        }
    });
    connect(editButton, &QPushButton::clicked, this, [this] {
        const int row = _table->currentRow();
        if (row >= 0 && editCheat(_cheats[static_cast<size_t>(row)])) {
            refreshTable();
            _table->selectRow(row);
        }
    });
    connect(removeButton, &QPushButton::clicked, this, [this] {
        const int row = _table->currentRow();
        if (row >= 0) {
            _cheats.erase(_cheats.begin() + row);
            refreshTable();
        }
    });
    connect(databaseButton, &QPushButton::clicked, this, &CheatDialog::importFromDatabase);
    databaseButton->setEnabled(!_session.cheatDatabaseName().isEmpty());
    connect(_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (editCheat(_cheats[static_cast<size_t>(row)])) {
            refreshTable();
            _table->selectRow(row);
        }
    });
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (saveAndApply()) {
            accept();
        }
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void CheatDialog::loadCheats()
{
    QFile file(_session.cheatFilePath());
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonArray cheats = QJsonDocument::fromJson(file.readAll()).object().value("cheats").toArray();
        for (const QJsonValue &value : cheats) {
            const QJsonObject object = value.toObject();
            _cheats.push_back({
                object.value("description").toString(),
                object.value("type").toInt(),
                object.value("enabled").toBool(true),
                object.value("codes").toString(),
            });
        }
    }
    _disableAll->setChecked(QSettings().value("cheats/disableAll", false).toBool());
}

bool CheatDialog::saveAndApply()
{
    for (int row = 0; row < _table->rowCount(); ++row) {
        _cheats[static_cast<size_t>(row)].enabled = _table->item(row, 0)->checkState() == Qt::Checked;
    }
    QString errorMessage;
    for (const FrontendCheat &cheat : _cheats) {
        if (!_session.validateCheat(cheat, errorMessage)) {
            QMessageBox::warning(this, tr("无效金手指"), tr("%1\n%2").arg(cheat.description, errorMessage));
            return false;
        }
    }
    if (!_session.applyCheats(_cheats, _disableAll->isChecked(), errorMessage)) {
        QMessageBox::warning(this, tr("应用失败"), errorMessage);
        return false;
    }

    QJsonArray cheats;
    for (const FrontendCheat &cheat : _cheats) {
        cheats.append(QJsonObject{
            {"description", cheat.description},
            {"type", cheat.type},
            {"enabled", cheat.enabled},
            {"codes", cheat.codes},
        });
    }
    QFile file(_session.cheatFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || file.write(QJsonDocument(QJsonObject{{"cheats", cheats}}).toJson()) < 0) {
        QMessageBox::warning(this, tr("保存失败"), tr("无法保存金手指文件。"));
        return false;
    }
    QSettings().setValue("cheats/disableAll", _disableAll->isChecked());
    return true;
}

void CheatDialog::refreshTable()
{
    const std::vector<CheatTypeOption> types = _session.cheatTypeOptions();
    _table->setRowCount(static_cast<int>(_cheats.size()));
    for (int row = 0; row < _table->rowCount(); ++row) {
        const FrontendCheat &cheat = _cheats[static_cast<size_t>(row)];
        auto *enabled = new QTableWidgetItem();
        enabled->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        enabled->setCheckState(cheat.enabled ? Qt::Checked : Qt::Unchecked);
        _table->setItem(row, 0, enabled);
        _table->setItem(row, 1, new QTableWidgetItem(cheat.description));
        const auto type = std::find_if(types.begin(), types.end(), [&cheat](const CheatTypeOption &option) {
            return option.type == cheat.type;
        });
        _table->setItem(row, 2, new QTableWidgetItem(type == types.end() ? tr("未知") : type->name));
        _table->setItem(row, 3, new QTableWidgetItem(QString(cheat.codes).replace('\n', "; ")));
    }
}

bool CheatDialog::editCheat(FrontendCheat &cheat)
{
    QDialog dialog(this);
    dialog.setWindowTitle(cheat.description.isEmpty() ? tr("添加金手指") : tr("编辑金手指"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout();
    auto *description = new QLineEdit(cheat.description, &dialog);
    auto *type = new QComboBox(&dialog);
    for (const CheatTypeOption &option : _session.cheatTypeOptions()) {
        type->addItem(option.name, option.type);
    }
    const int typeIndex = type->findData(cheat.type);
    type->setCurrentIndex(typeIndex >= 0 ? typeIndex : 0);
    auto *codes = new QTextEdit(&dialog);
    codes->setPlainText(cheat.codes);
    codes->setPlaceholderText(tr("每行一条代码；NES 连续写入格式：AAAA-BB-CC"));
    form->addRow(tr("描述"), description);
    form->addRow(tr("类型"), type);
    form->addRow(tr("代码"), codes);
    layout->addLayout(form);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    FrontendCheat candidate = cheat;
    candidate.description = description->text().trimmed();
    candidate.type = type->currentData().toInt();
    candidate.codes = codes->toPlainText().trimmed();
    static const QRegularExpression nesRangePattern("(^|[;\\n\\r])\\s*[0-9A-Fa-f]{4}-[0-9A-Fa-f]{2}-[0-9A-Fa-f]{2}\\s*($|[;\\n\\r])");
    if (_session.cheatDatabaseName() == "Nes" && candidate.codes.contains(nesRangePattern)) {
        candidate.type = 2;
    }
    QString errorMessage;
    if (candidate.description.isEmpty() || !_session.validateCheat(candidate, errorMessage)) {
        QMessageBox::warning(this, tr("无效金手指"), candidate.description.isEmpty() ? tr("请输入描述。") : errorMessage);
        return false;
    }
    cheat = candidate;
    return true;
}

void CheatDialog::importFromDatabase()
{
    const QString databaseFileName = "CheatDb." + _session.cheatDatabaseName() + ".json";
    QString databasePath = QCoreApplication::applicationDirPath() + "/CheatDatabase/" + databaseFileName;
    if (!QFile::exists(databasePath)) {
        databasePath = QString::fromUtf8(MESEN_DATA_DIR) + "/CheatDatabase/" + databaseFileName;
    }
    QFile file(databasePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("数据库不可用"), tr("无法打开金手指数据库：%1").arg(databasePath));
        return;
    }
    const QJsonArray games = QJsonDocument::fromJson(file.readAll()).object().value("games").toArray();

    QDialog dialog(this);
    dialog.setWindowTitle(tr("金手指数据库"));
    dialog.resize(680, 460);
    auto *layout = new QVBoxLayout(&dialog);
    auto *search = new QLineEdit(&dialog);
    search->setPlaceholderText(tr("搜索游戏"));
    auto *gameList = new QListWidget(&dialog);
    auto *cheatList = new QListWidget(&dialog);
    cheatList->setSelectionMode(QAbstractItemView::NoSelection);
    auto *splitter = new QSplitter(&dialog);
    splitter->addWidget(gameList);
    splitter->addWidget(cheatList);
    layout->addWidget(search);
    layout->addWidget(splitter);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("导入所选游戏"));
    layout->addWidget(buttons);

    auto populateGames = [games, gameList](const QString &filter) {
        gameList->clear();
        for (int index = 0; index < games.size(); ++index) {
            const QJsonObject game = games[index].toObject();
            const QString name = game.value("name").toString();
            if (filter.isEmpty() || name.contains(filter, Qt::CaseInsensitive)) {
                auto *item = new QListWidgetItem(name, gameList);
                item->setData(Qt::UserRole, index);
            }
        }
    };
    populateGames({});
    connect(search, &QLineEdit::textChanged, &dialog, populateGames);
    connect(gameList, &QListWidget::currentItemChanged, &dialog, [games, cheatList](QListWidgetItem *current) {
        cheatList->clear();
        if (!current) {
            return;
        }
        const QJsonArray cheats = games[current->data(Qt::UserRole).toInt()].toObject().value("cheats").toArray();
        for (const QJsonValue &value : cheats) {
            const QJsonObject cheat = value.toObject();
            cheatList->addItem(cheat.value("desc").toString() + "  [" + cheat.value("code").toString() + "]");
        }
    });
    const QString hash = _session.cheatDatabaseHash();
    for (int row = 0; row < gameList->count(); ++row) {
        const QJsonObject game = games[gameList->item(row)->data(Qt::UserRole).toInt()].toObject();
        if (game.value("sha1").toString().compare(hash, Qt::CaseInsensitive) == 0) {
            gameList->setCurrentRow(row);
            break;
        }
    }
    if (gameList->currentRow() < 0 && gameList->count() > 0) {
        gameList->setCurrentRow(0);
    }
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted || !gameList->currentItem()) {
        return;
    }

    const QJsonArray imported = games[gameList->currentItem()->data(Qt::UserRole).toInt()].toObject().value("cheats").toArray();
    for (const QJsonValue &value : imported) {
        const QJsonObject object = value.toObject();
        const QString code = object.value("code").toString();
        int type = 0;
        if (_session.cheatDatabaseName() == "Nes") {
            static const QRegularExpression nesRangePattern("^[0-9A-Fa-f]{4}-[0-9A-Fa-f]{2}-[0-9A-Fa-f]{2}$");
            type = (code.contains(':') || nesRangePattern.match(code).hasMatch()) ? 2 : 0;
        } else {
            type = code.contains('-') ? 5 : 6;
        }
        FrontendCheat cheat{object.value("desc").toString(), type, false, QString(code).replace(';', '\n')};
        const auto duplicate = std::find_if(_cheats.begin(), _cheats.end(), [&cheat](const FrontendCheat &existing) {
            return existing.description == cheat.description && existing.type == cheat.type && existing.codes == cheat.codes;
        });
        if (duplicate == _cheats.end()) {
            _cheats.push_back(cheat);
        }
    }
    refreshTable();
}