#pragma once

#include <QStringList>

class RecentFiles final
{
public:
    RecentFiles();

    const QStringList &items() const;
    void add(const QString &filePath);

private:
    void save() const;

    QStringList _items;
};