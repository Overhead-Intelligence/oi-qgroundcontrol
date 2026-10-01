#pragma once

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtQmlIntegration/QtQmlIntegration>

class Fact;
class QmlObjectListModel;

/// \brief Loads the specified action files and provides access to the actions they contain.
///
/// Action files are loaded from the default MavlinkActions directory. The fact holds file
/// names only, no path.
///
/// More than one file can be named, separated by `;`. Each is loaded in turn and their
/// actions are concatenated in the order the files are named, so a set of actions that only
/// applies to some airframes can live in its own file and be switched on and off rather than
/// being merged by hand into a single list. Changing the fact reloads every file and emits
/// actionsChanged(), which is what makes a file usable without restarting.
///
/// `;` rather than `,` is deliberate: QSettings' INI backend treats an unquoted comma as a
/// list separator, and a settings override file is hand-edited. Both are accepted on read.
class MavlinkActionManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_MOC_INCLUDE("Fact.h")
    Q_MOC_INCLUDE("QmlObjectListModel.h")
    Q_PROPERTY(Fact* actionFileNameFact READ actionFileNameFact WRITE setActionFileNameFact NOTIFY actionFileNameFactChanged)
    Q_PROPERTY(QmlObjectListModel* actions READ actions CONSTANT)

public:
    explicit MavlinkActionManager(QObject *parent = nullptr);
    explicit MavlinkActionManager(Fact *actionFileNameFact, QObject *parent = nullptr);
    ~MavlinkActionManager();

    Fact *actionFileNameFact() { return _actionFileNameFact; }
    void setActionFileNameFact(Fact *actionFileNameFact);
    QmlObjectListModel *actions() { return _actions; }

    /// Splits a settings value into the file names it lists, trimmed and de-duplicated.
    static QStringList fileNamesFromSettingValue(const QString &settingValue);

signals:
    void actionFileNameFactChanged();

    /// The action list has been rebuilt. Anything caching it - the Joystick assignable
    /// action list, for one - needs to rebuild from actions() when this fires.
    void actionsChanged();

private slots:
    void _loadActionsFiles();

private:
    /// Appends every action in one file to the model. Returns false and sets errorString
    /// if the file is malformed; a file which simply is not there is not an error, so a
    /// name left in the settings for a file that has been deleted stays quiet.
    bool _appendActionsFromFile(const QString &fileName, QString &errorString);

    Fact *_actionFileNameFact = nullptr;
    QmlObjectListModel *_actions = nullptr;
};
