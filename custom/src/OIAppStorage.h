/****************************************************************************
 *
 * Overhead Intelligence QGroundControl custom build.
 *
 * Where an OI feature keeps files it has taken ownership of.
 *
 * Two features import a file and keep their own copy of it rather than
 * referencing where the operator picked it from - map overlay layers and fleet
 * Lua scripts - because a reference that disappears when somebody tidies their
 * Downloads folder is not a reference. Each had grown its own copy of "make a
 * named folder under the application save path", and the two had already
 * drifted: one resolved the path on every use, the other once at construction.
 * That is invisible until the operator moves the save path, at which point one
 * feature follows it and the other does not.
 *
 * Resolving per call is the behaviour worth keeping. The save path is a
 * setting, settings change while the app is running, and caching an answer
 * derived from one buys nothing here - these calls happen when an operator
 * clicks Import, not in a loop.
 *
 * QGroundControl is licensed according to the terms in the file COPYING.md
 * in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QString>

namespace OIAppStorage {

/// Absolute path to @p folderName under the application save path, created if it is
/// not there yet.
///
/// Returns an empty string when the save path is unset or the folder cannot be
/// created, which callers must treat as "storage unavailable" rather than as a path.
/// Resolved on every call on purpose - see the note at the top of this file.
QString folder(const QString &folderName);

} // namespace OIAppStorage
