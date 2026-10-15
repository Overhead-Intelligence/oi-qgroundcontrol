import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FactControls

/// Fact-bound text field with an optional slider and enable/disable checkbox.
/// The slider range is determined by (in priority order): explicit sliderMin/Max,
/// fact.userMin/Max, or fact.min/max when allowUsingMinMax is true.
///
/// **The slider is off unless a caller asks for it.** Its range comes from userMin/userMax,
/// which FactMetaData treats as a display hint rather than a limit - validation runs off
/// min/max - so on a field holding a typed quantity that range is advisory and often arbitrary.
/// Worse, updateSliderToClampedValue() clamps the *handle* to the range while the fact keeps
/// whatever was typed: entering 150 into a field whose userMax is 121.92 leaves the handle
/// pinned at 121.92 beside a text field reading 150. A control showing a different number from
/// the one in force is worse than no control.
///
/// It stays available for the places a slider is the better instrument - a continuous feel
/// adjustment with no meaningful number to type, like joystick expo - which opt in with
/// showSlider: true.
Rectangle {
    property string label
    property alias fact:                    factTextField.fact
    property alias textFieldPreferredWidth: factTextField.textFieldPreferredWidth
    property alias textFieldUnitsLabel:     factTextField.textFieldUnitsLabel
    property alias textFieldShowUnits:      factTextField.textFieldShowUnits
    property alias textFieldShowHelp:       factTextField.textFieldShowHelp
    property alias textField:               factTextField
    property alias enableCheckBoxChecked:   enableCheckbox.checked
    property bool  warnOnUserMinMaxInvalid: true  ///< true: warn if userMin/userMax are invalid, false: no warning

    property bool   showEnableCheckbox: false ///< true: show enable/disable checkbox, false: hide
    property bool   showSlider:         false ///< true: show the slider below the text field
    property bool   allowUsingMinMax:   false ///< true: fall back to fact.min/max when userMin/Max not set
    property var    sliderMin:          undefined ///< explicit slider minimum, overrides fact.userMin/min
    property var    sliderMax:          undefined ///< explicit slider maximum, overrides fact.userMax/max
    property color  backgroundColor:    _ftfsBackgroundColor

    signal enableCheckboxClicked

    id:             control
    implicitHeight: mainLayout.implicitHeight
    color:          backgroundColor
    radius:         ScreenTools.defaultBorderRadius

    property bool _loadComplete:            false
    property bool _showSlider:              showSlider && _sliderMin !== undefined && _sliderMax !== undefined
    property color _ftfsBackgroundColor:    Qt.rgba(qgcPal.windowShadeLight.r, qgcPal.windowShadeLight.g, qgcPal.windowShadeLight.b, 0.2)
    property var  _sliderMin:               sliderMin !== undefined ? sliderMin : (fact.userMin !== undefined ? fact.userMin : (allowUsingMinMax ? fact.min : undefined))
    property var  _sliderMax:               sliderMax !== undefined ? sliderMax : (fact.userMax !== undefined ? fact.userMax : (allowUsingMinMax ? fact.max : undefined))

    function updateSliderToClampedValue() {
        if (_showSlider && sliderLoader.item) {
            let clampedSliderValue = control.fact.value
            if (clampedSliderValue > control._sliderMax) {
                clampedSliderValue = control._sliderMax
            } else if (clampedSliderValue < control._sliderMin) {
                clampedSliderValue = control._sliderMin
            }
            sliderLoader.item.value = clampedSliderValue
        }
    }

    Component.onCompleted: {
        _loadComplete = true
        // Only worth warning when a slider was actually asked for; without one the metadata
        // goes unused and its absence is not a defect.
        if (showSlider && warnOnUserMinMaxInvalid && !allowUsingMinMax && sliderMin === undefined && sliderMax === undefined && (fact.userMin === undefined || fact.userMax === undefined)) {
            console.warn("FactTextFieldSlider: userMin/userMax not set for", fact.name)
        }
    }

    Connections {
        target: control.fact

        function onValueChanged() {
            control.updateSliderToClampedValue()
        }
    }

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    ColumnLayout {
        id:         mainLayout
        width:      parent.width
        spacing:    0

        RowLayout {
            spacing: ScreenTools.defaultFontPixelWidth

            QGCCheckBox {
                id:                 enableCheckbox
                Layout.fillWidth:   visible
                text:               control.label
                visible:            control.showEnableCheckbox

                onClicked: control.enableCheckboxClicked()
            }

            LabelledFactTextField {
                id:                 factTextField
                Layout.fillWidth:   !control.showEnableCheckbox
                label:              control.showEnableCheckbox ? "" : control.label
                fact:               control.fact
                enabled:            !control.showEnableCheckbox || enableCheckbox.checked
            }
        }

        Loader {
            id:                 sliderLoader
            Layout.fillWidth:   true
            sourceComponent:    control._showSlider ? sliderComponent : null
            enabled:            !control.showEnableCheckbox || enableCheckbox.checked

            onLoaded: control.updateSliderToClampedValue()
        }

        Component {
            id: sliderComponent

            QGCSlider {
                id:                 slider
                Layout.fillWidth:   true
                from:               control._sliderMin
                to:                 control._sliderMax
                showBoundaryValues: true

                onMoved: {
                    if (control._loadComplete) {
                        control.fact.value = slider.value
                    }
                }
            }
        }
    }
}
