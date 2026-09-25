import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

import OI.Controls

/// One key binding row: a label, the key currently bound, and a button that captures
/// the next key press.
///
/// Typing key names by hand was the previous arrangement and it was a trap - "Down"
/// and "PgUp" have to be spelled exactly the way QKeySequence prints them, with no
/// feedback when they are not, so a typo simply produced a binding that never fired.
/// Capturing the real press removes the class of error entirely: what is stored is
/// whatever QKeySequence calls the key the operator actually pressed, which is by
/// construction what the controller will match against later.
///
/// The capture itself happens in C++ (OIKeyboard.beginKeyCapture) because the
/// application-wide event filter sees key presses before any focused QML item and
/// would otherwise swallow the press being bound.
RowLayout {
    id:         root
    spacing:    ScreenTools.defaultFontPixelWidth

    property string label
    property string keyText
    property bool   conflicted: false

    /// Emitted with the captured key name, or "" when the operator clears the binding.
    signal keyChosen(string key)

    // Only the row that armed the capture may consume the result; without this every
    // row on the page would take the same key press.
    property bool _waiting: false

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    QGCLabel {
        Layout.fillWidth:       true
        Layout.minimumWidth:    0
        elide:                  Text.ElideRight
        text:                   root.label
        color:                  root.conflicted ? qgcPal.warningText : qgcPal.text
    }

    QGCButton {
        Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 16
        text: {
            if (root._waiting) {
                return qsTr("Press a key…")
            }
            if (root.keyText === "") {
                return qsTr("Unbound")
            }
            return root.conflicted ? qsTr("%1 (conflict)").arg(root.keyText) : root.keyText
        }
        onClicked: {
            root._waiting = true
            OIKeyboard.beginKeyCapture()
        }
    }

    QGCButton {
        text:       qsTr("Clear")
        enabled:    root.keyText !== "" && !root._waiting
        onClicked:  root.keyChosen("")
    }

    Connections {
        target: OIKeyboard

        function onKeyCaptured(keyName) {
            if (root._waiting) {
                root._waiting = false
                root.keyChosen(keyName)
            }
        }

        // Covers Esc, and another row arming a capture while this one was waiting.
        function onCapturingKeyChanged() {
            if (!OIKeyboard.capturingKey) {
                root._waiting = false
            }
        }
    }
}
