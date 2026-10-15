/****************************************************************************
 *
 * Overhead Intelligence QGroundControl custom build.
 *
 * See OIAppStorage.h for why this exists and why it resolves per call.
 *
 * QGroundControl is licensed according to the terms in the file COPYING.md
 * in the root of the source code directory.
 *
 ****************************************************************************/

#include "OIAppStorage.h"

#include "AppSettings.h"
#include "SettingsManager.h"

#include <QtCore/QDir>

namespace OIAppStorage {

QString folder(const QString &folderName)
{
    if (folderName.isEmpty()) {
        return QString();
    }

    const QString rootPath = SettingsManager::instance()->appSettings()->savePath()->rawValue().toString();
    if (rootPath.isEmpty()) {
        return QString();
    }

    // The save path itself can be missing - the operator can point the setting at a
    // folder that does not exist yet - so make it before asking for a child of it.
    QDir root(rootPath);
    if (!root.exists() && !QDir().mkpath(rootPath)) {
        return QString();
    }

    if (!root.exists(folderName) && !root.mkpath(folderName)) {
        return QString();
    }

    return root.filePath(folderName);
}

} // namespace OIAppStorage
