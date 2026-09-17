#include "recentfiles.h"

#include <QFileInfo>
#include <QSettings>

namespace
{
constexpr int MaxRecentFiles = 10;
}

RecentFiles::RecentFiles()
{
    QSettings settings;
    const QStringList savedItems = settings.value("recentFiles").toStringList();
    for (const QString &filePath : savedItems) {
        if (QFileInfo::exists(filePath) && !_items.contains(filePath)) {
            _items.append(filePath);
        }
    }
    _items = _items.mid(0, MaxRecentFiles);
}

const QStringList &RecentFiles::items() const
{
    return _items;
}

void RecentFiles::add(const QString &filePath)
{
    const QString canonicalPath = QFileInfo(filePath).canonicalFilePath();
    if (canonicalPath.isEmpty()) {
        return;
    }

    _items.removeAll(canonicalPath);
    _items.prepend(canonicalPath);
    _items = _items.mid(0, MaxRecentFiles);
    save();
}

void RecentFiles::save() const
{
    QSettings settings;
    settings.setValue("recentFiles", _items);
}