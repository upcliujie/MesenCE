#pragma once

#include <QWidget>

#include <functional>
#include <vector>

class QGridLayout;
class QLabel;
class QResizeEvent;

class RecentGamesWidget final : public QWidget
{
public:
    RecentGamesWidget(std::function<void(const QString &)> loadGame, std::function<void()> openGame, QWidget *parent = nullptr);

    void refresh();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    QWidget *createCard(const QString &filePath);
    void updateGrid();

    std::function<void(const QString &)> _loadGame;
    QGridLayout *_grid = nullptr;
    QLabel *_emptyLabel = nullptr;
    std::vector<QWidget *> _cards;
    int _columnCount = 0;
};
