#include "recentgameswidget.h"

#include "Utilities/FolderUtilities.h"
#include "Utilities/ZipReader.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>

RecentGamesWidget::RecentGamesWidget(std::function<void(const QString &)> loadGame, std::function<void()> openGame, QWidget *parent)
    : QWidget(parent)
    , _loadGame(std::move(loadGame))
{
    setObjectName(QStringLiteral("recentGamesPage"));
    setStyleSheet(QStringLiteral(
        "#recentGamesPage { background: #181818; }"
        "QLabel { color: #f2f2f2; }"
        "QPushButton#gameCard { background: #202020; border: 2px solid #48505a; padding: 8px; text-align: left; }"
        "QPushButton#gameCard:hover { background: #292929; border-color: #00a7e8; }"
        "QPushButton#openGameButton { color: white; background: #303842; border: 1px solid #59636e; padding: 7px 14px; }"
        "QPushButton#openGameButton:hover { background: #3b4651; border-color: #00a7e8; }"));

    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(24, 20, 24, 20);
    pageLayout->setSpacing(16);

    auto *headerLayout = new QHBoxLayout();
    auto *title = new QLabel(tr("最近游玩"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    title->setFont(titleFont);
    headerLayout->addWidget(title);
    headerLayout->addStretch();

    auto *openButton = new QPushButton(tr("打开游戏..."), this);
    openButton->setObjectName(QStringLiteral("openGameButton"));
    connect(openButton, &QPushButton::clicked, this, [openGame = std::move(openGame)] {
        openGame();
    });
    headerLayout->addWidget(openButton);
    pageLayout->addLayout(headerLayout);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));

    auto *content = new QWidget(scrollArea);
    content->setStyleSheet(QStringLiteral("background: transparent;"));
    _grid = new QGridLayout(content);
    _grid->setContentsMargins(0, 0, 0, 0);
    _grid->setHorizontalSpacing(14);
    _grid->setVerticalSpacing(16);
    _grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    _emptyLabel = new QLabel(tr("还没有最近游戏"), content);
    _emptyLabel->setAlignment(Qt::AlignCenter);
    _emptyLabel->setStyleSheet(QStringLiteral("color: #a9b0b8; font-size: 15px; padding: 48px;"));
    scrollArea->setWidget(content);
    pageLayout->addWidget(scrollArea);

    refresh();
}

void RecentGamesWidget::refresh()
{
    for (QWidget *card : _cards) {
        delete card;
    }
    _cards.clear();

    QDir recentDirectory(QString::fromStdString(FolderUtilities::GetRecentGamesFolder()));
    const QFileInfoList files = recentDirectory.entryInfoList({QStringLiteral("*.rgd")}, QDir::Files, QDir::Time);
    const int count = std::min(files.size(), 72);
    _cards.reserve(static_cast<size_t>(count));
    for (int index = 0; index < count; ++index) {
        _cards.push_back(createCard(files[index].absoluteFilePath()));
    }

    _emptyLabel->setVisible(_cards.empty());
    updateGrid();
}

QWidget *RecentGamesWidget::createCard(const QString &filePath)
{
    const QFileInfo fileInfo(filePath);
    auto *card = new QPushButton(this);
    card->setObjectName(QStringLiteral("gameCard"));
    card->setCursor(Qt::PointingHandCursor);
    card->setFixedSize(220, 210);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(7);

    auto *preview = new QLabel(card);
    preview->setFixedSize(200, 150);
    preview->setAlignment(Qt::AlignCenter);
    preview->setStyleSheet(QStringLiteral("background: black; color: #7f8790;"));
    preview->setText(tr("无截图"));

    ZipReader reader;
    std::vector<uint8_t> imageData;
    reader.LoadArchive(QFile::encodeName(filePath).constData());
    if (reader.ExtractFile("Screenshot.png", imageData)) {
        QPixmap image;
        if (image.loadFromData(imageData.data(), static_cast<uint32_t>(imageData.size()))) {
            preview->setPixmap(image.scaled(preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            preview->setText({});
        }
    }
    layout->addWidget(preview, 0, Qt::AlignCenter);

    auto *name = new QLabel(fileInfo.completeBaseName(), card);
    name->setStyleSheet(QStringLiteral("font-weight: 600;"));
    name->setTextInteractionFlags(Qt::NoTextInteraction);
    layout->addWidget(name);

    auto *time = new QLabel(tr("最近游玩：%1").arg(QLocale().toString(fileInfo.lastModified(), QLocale::ShortFormat)), card);
    time->setStyleSheet(QStringLiteral("color: #a9b0b8; font-size: 11px;"));
    time->setTextInteractionFlags(Qt::NoTextInteraction);
    layout->addWidget(time);

    connect(card, &QPushButton::clicked, this, [this, filePath] {
        _loadGame(filePath);
    });
    return card;
}

void RecentGamesWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateGrid();
}

void RecentGamesWidget::updateGrid()
{
    const int columns = std::max(1, width() / 234);
    if (columns == _columnCount && _grid->count() > 0) {
        return;
    }
    _columnCount = columns;

    while (QLayoutItem *item = _grid->takeAt(0)) {
        delete item;
    }

    if (_cards.empty()) {
        _grid->addWidget(_emptyLabel, 0, 0, 1, columns);
        return;
    }

    for (size_t index = 0; index < _cards.size(); ++index) {
        _grid->addWidget(_cards[index], static_cast<int>(index) / columns, static_cast<int>(index) % columns);
    }
}
